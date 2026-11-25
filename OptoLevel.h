#pragma once
namespace {
class OptoLevel
{
public:
    static State::ParameterLayout layout()
    {
        State::ParameterLayout p;
        auto knob = [&](const char* id,const char* name,float lo,float hi,float step,float initial)
        {
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1}, name,
                  juce::NormalisableRange<float>{lo,hi,step},initial));
        };
        auto choice = [&](const char* id,const char* name,juce::StringArray options,int initial)
        {
            p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id,1},name,options,initial));
        };
        knob("gain","Gain",0,100,.01f,40);
        knob("peak","Peak Reduction",0,100,.01f,46);
        knob("focus","HiFreq",0,100,.1f,50);
        knob("mix","Mix (%)",0,100,.1f,100);
        knob("trim","Trim (dB)",-18,18,.1f,0);
        choice("mode","Mode",{"Compress","Limit"},0);
        choice("meter","VU Display",{"In","GR","Out"},1);
        choice("analog","Analog",{"Off","50 Hz","60 Hz"},2);
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"auto",1},"Auto Makeup",false));
        return p;
    }
    std::atomic<float> inVU{},outVU{},gr{};
    void prepare(double sr,State& state)
    {
        rate=sr;
        const char* names[]{"gain","peak","focus","mix","trim","mode","analog","auto"};
        for(size_t i=0;i<params.size();++i) params[i]=state.getRawParameterValue(names[i]);
        auto coefficient=[sr](double seconds){return (float)-std::expm1(-1./(sr*seconds));};
        attack=coefficient(.010); release=coefficient(.077);
        fill=coefficient(.36); drain=coefficient(1.35);
        rectDecay=1.f-coefficient(.004); shelf=coefficient(1./(2*juce::MathConstants<double>::pi*900));
        vu=coefficient(.105); smoothing=coefficient(.012);
    }
    void reset()
    {
        low={}; rect=fast=memory=phase=inputMean=outputMean=0; noise=12739;
        inVU=outVU=gr=0;
        for(size_t i=0;i<current.size();++i) if(params[i]) current[i]=params[i]->load();
    }
    void process(juce::AudioBuffer<float>& audio)
    {
        std::array<float,8> targets{};
        for(size_t k=0;k<params.size();++k) targets[k]=params[k]->load(std::memory_order_relaxed);
        const float mains=targets[6]==1 ? 50.f : 60.f;
        auto channels=audio.getArrayOfWritePointers();
        const int count=audio.getNumChannels();
        for(int i=0;i<audio.getNumSamples();++i)
        {
            for(size_t k=0;k<current.size();++k) current[k]+=smoothing*(targets[k]-current[k]);
            const float emphasis=(100-current[2])*.0096f;
            float sense=0,input=0,output=0;
            for(int c=0;c<count;++c)
            {
                const float x=channels[c][i];
                low[c]+=shelf*(x-low[c]);
                sense=std::max(sense,std::abs(x-emphasis*low[c]));
                input=std::max(input,std::abs(x));
            }
            rect=std::max(sense,rect*rectDecay);
            const float threshold=8.f-.55f*current[1];
            const float above=juce::Decibels::gainToDecibels(rect,-110.f)-threshold;
            const float knee=above<=-3 ? 0 : above>=3 ? above : (above+3)*(above+3)/12;
            const float slope=.67f+.28f*current[5];
            const float desired=std::min(40.f,knee*slope*std::min(current[1]*.1f,1.f));
            fast+=(desired>fast ? attack : release)*(desired-fast);
            // The slower reservoir charges during sustained illumination of the optical cell.
            const float stored=desired*(.38f+.18f*std::min(desired/18.f,1.f));
            memory+=(stored>memory ? fill : drain)*(stored-memory);
            const float reduction=std::max(fast,memory);
            const float makeup=current[7]*.55f*std::max(0.f,(-18.f-threshold)*slope);
            const float attenuation=juce::Decibels::decibelsToGain(makeup-reduction);
            const float outputGain=std::pow(current[0]/32.28f,2.f);
            const float trim=juce::Decibels::decibelsToGain(current[4]);
            phase+=mains/rate;if(phase>=1.) phase-=1.;
            noise=noise*1664525u+1013904223u;
            const float noiseSample=(float)(noise>>8)*(2.f/16777216.f)-1.f;
            const float analogBlend=std::min(current[6],1.f);
            const float hum=analogBlend*(.00005f*(float)std::sin(juce::MathConstants<double>::twoPi*phase)+.000012f*noiseSample);
            for(int c=0;c<count;++c)
            {
                const float dry=channels[c][i];
                const float driven=dry*attenuation*outputGain;
                const float wet=std::tanh(.34f*driven)/.34f+hum;
                channels[c][i]=trim*(dry+.01f*current[3]*(wet-dry));
                output=std::max(output,std::abs(channels[c][i]));
            }
            inputMean+=vu*(input-inputMean);
            outputMean+=vu*(output-outputMean);
        }
        inVU.store(inputMean);outVU.store(outputMean);gr.store(std::max(fast,memory));
    }
private:
    std::array<std::atomic<float>*,8> params{};
    std::array<float,8> current{};
    std::array<float,2> low{};
    double rate=44100,phase=0;
    float attack{},release{},fill{},drain{},rectDecay{},shelf{},vu{},smoothing{};
    float rect{},fast{},memory{},inputMean{},outputMean{};
    uint32_t noise=12739;
};
#include "OptoPanel.h"
}
