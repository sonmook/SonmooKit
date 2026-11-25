#pragma once
namespace {
class StereoArc
{
public:
    using Pair = std::array<float,2>;
    struct Image
    {
        float width, gain, asymSin, asymCos, rotSin, rotCos;
        Image(float w,float db,float asym,float rot)
            : width(w), gain(juce::Decibels::decibelsToGain(db)),
              asymSin(std::sin(juce::degreesToRadians(asym))), asymCos(std::cos(juce::degreesToRadians(asym))),
              rotSin(std::sin(juce::degreesToRadians(rot))), rotCos(std::cos(juce::degreesToRadians(rot))) {}
        Pair map(float l,float r) const
        {
            const float side=(l-r)*.5f*width;
            const float mid=(l+r)*.5f-asymSin*side;
            const float left=mid+asymCos*side, right=mid-asymCos*side;
            return {gain*(rotCos*left-rotSin*right),gain*(rotSin*left+rotCos*right)};
        }
    };
    static State::ParameterLayout layout()
    {
        State::ParameterLayout p;
        auto knob=[&](const char* id,const char* name,float lo,float hi,float step,float initial){
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>{lo,hi,step},initial));};
        knob("gain","Gain (dB)",-24,0,.1f,0);knob("width","Width",0,3,.01f,1);
        knob("asym","Asymmetry (degrees)",-90,90,.1f,0);knob("rotation","Rotation (degrees)",-45,45,.1f,0);
        for(auto id:{"input","meter"})
            p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id,1},juce::String(id)=="input"?"Input format":"Meter format",juce::StringArray{"L/R","M/S"},0));
        for(auto item:std::array<std::pair<const char*,const char*>,3>{{{"invert1","Invert first input"},{"invert2","Invert second input"},{"swap","Swap inputs"}}})
            p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{item.first,1},item.second,false));
        return p;
    }
    void prepare(double sr,State& state)
    {
        const char* ids[]{"width","gain","asym","rotation","input","invert1","invert2","swap"};
        for(size_t i=0;i<params.size();++i)params[i]=state.getRawParameterValue(ids[i]);
        rate=sr;for(auto& c:matrix)c.reset(sr,.01);
    }
    void reset()
    {
        if(params[0]) update(true);
        for(auto& peak:peaks)peak.store(0);
        clipped.store(false);
    }
    void process(juce::AudioBuffer<float>& b)
    {
        update(false);
        std::array<float,4> block{};
        auto* left=b.getWritePointer(0);auto* right=b.getWritePointer(1);
        for(int n=0;n<b.getNumSamples();++n)
        {
            const float x=left[n],y=right[n];
            const float l=x*matrix[0].getNextValue()+y*matrix[1].getNextValue();
            const float r=x*matrix[2].getNextValue()+y*matrix[3].getNextValue();
            left[n]=l;right[n]=r;
            const std::array<float,4> values{l,r,.5f*(l+r),.5f*(l-r)};
            for(size_t i=0;i<4;++i)block[i]=std::max(block[i],std::abs(values[i]));
        }
        if(std::max(block[0],block[1])>1)clipped.store(true,std::memory_order_relaxed);
        const float fall=(float)std::exp(-b.getNumSamples()/(rate*.32));
        for(size_t i=0;i<4;++i)peaks[i].store(std::max(block[i],peaks[i].load()*fall));
    }
    std::array<std::atomic<float>,4> peaks{};
    std::atomic<bool> clipped{false};
private:
    void update(bool initialise)
    {
        const Image image(params[0]->load(),params[1]->load(),params[2]->load(),params[3]->load());
        const bool ms=params[4]->load()>.5f,swap=params[7]->load()>.5f;
        const float a=params[5]->load()>.5f?-1.f:1.f,b=params[6]->load()>.5f?-1.f:1.f;
        auto column=[&](float x,float y){
            x*=a;y*=b;if(swap)std::swap(x,y);
            return ms?image.map(x+y,x-y):image.map(x,y);
        };
        const auto x=column(1,0),y=column(0,1);
        const std::array<float,4> coefficients{x[0],y[0],x[1],y[1]};
        for(size_t i=0;i<4;++i)
            if(initialise)matrix[i].setCurrentAndTargetValue(coefficients[i]);
            else matrix[i].setTargetValue(coefficients[i]);
    }
    double rate=44100;
    std::array<std::atomic<float>*,8> params{};
    std::array<juce::SmoothedValue<float>,4> matrix;
};
#include "ArcPanel.h"
}
