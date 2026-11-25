#include <juce_audio_processors/juce_audio_processors.h>
#include "SonmooBrand.h"
#include <array>
#include <cmath>

namespace {
using namespace juce;
using State = AudioProcessorValueTreeState;
enum Model { Air = 12, Bass, Sweep, Press, Lift, Dip, Room };
constexpr int model = EFFECT_KIND;
constexpr const char* labels[]{"Air","Body","Cutoff","Pressure","Level","Depth","Space"};
constexpr const char* label = labels[model-Air];
constexpr double beats[]{4,2,1.5,1,.75,2./3,.5,1./3,.25,.125};
constexpr float initial = model == Sweep ? 10.f : 0.f;

State::ParameterLayout parameters()
{
    State::ParameterLayout p;
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"amount",1},label,NormalisableRange<float>{0,10,.01f},initial));
    auto menu = [&](const char* id,const char* title,StringArray items,int index){
        p.add(std::make_unique<AudioParameterChoice>(ParameterID{id,1},title,items,index));
    };
    if constexpr(model==Sweep) menu("res","Resonance",{"None","Low","Medium","High"},1);
    if constexpr(model==Press) menu("input","Input",{"Normal","Pad -6 dB","Boost +6 dB"},0);
    if constexpr(model==Dip){
        menu("rate","Rate",{"1/1","1/2","3/8","1/4","3/16","1/6","1/8","1/12","1/16","1/32"},3);
        menu("offset","Offset",{"0","90","180","-90"},0);
    }
    return p;
}

class Macro final : public AudioProcessor {
public:
    Macro() : AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true)
                              .withOutput("Output",AudioChannelSet::stereo(),true)),state(*this,nullptr,"SonmooMacro",parameters())
    {
        amount=state.getRawParameterValue("amount");
        if constexpr(model==Sweep) option=state.getRawParameterValue("res");
        if constexpr(model==Press) option=state.getRawParameterValue("input");
        if constexpr(model==Dip){option=state.getRawParameterValue("rate");offset=state.getRawParameterValue("offset");}
    }
    State state;
    std::atomic<float> meter{0};
    const String getName() const override {return JucePlugin_Name;}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    double getTailLengthSeconds() const override {return model==Room ? 12. : 0.;}
    bool hasEditor() const override {return true;}
    AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override {return {};}
    void changeProgramName(int,const String&) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& b) const override
    {
        const auto in=b.getMainInputChannelSet(),out=b.getMainOutputChannelSet();
        return (in==AudioChannelSet::mono()||in==AudioChannelSet::stereo()) &&
               (in==out || (model==Room && in==AudioChannelSet::mono() && out==AudioChannelSet::stereo()));
    }
    void getStateInformation(MemoryBlock& out) override {if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,out);}
    void setStateInformation(const void* data,int bytes) override
    {if(auto xml=getXmlFromBinary(data,bytes);xml && xml->hasTagName(state.state.getType()))state.replaceState(ValueTree::fromXml(*xml));}
    void reset() override
    {
        memory={}; roomActive=false; lastRoom=-1; envelope=0; limiter=1; phase=0; meter=0; room.reset();
        smooth.setCurrentAndTargetValue(amount->load()*.1f);
    }
    void prepareToPlay(double sampleRate,int) override
    {
        sr=sampleRate; smooth.reset(sr,.012);
        if constexpr(model==Room) room.setSampleRate(sr);
        reset();
    }
    void processBlock(AudioBuffer<float>& b,MidiBuffer&) override
    {
        ScopedNoDenormals guard;
        int channels=b.getNumChannels(),n=b.getNumSamples();
        if(!n || !channels)return;
        if constexpr(model==Room) if(getTotalNumInputChannels()==1 && channels==2)b.copyFrom(1,0,b,0,0,n);
        smooth.setTargetValue(amount->load()*.1f);
        const int mode=option?int(option->load()):0;
        double inc=0;
        if constexpr(model==Dip){
            double bpm=120;
            if(auto* h=getPlayHead())if(auto time=h->getPosition()){
                if(auto tempo=time->getBpm())bpm=*tempo;
                if(time->getIsPlaying())if(auto ppq=time->getPpqPosition())phase=*ppq/beats[mode]-std::floor(*ppq/beats[mode]);
            }
            inc=bpm/(60.*sr*beats[mode]);
        }
        const float inputScale=mode==1 ? .501187f : mode==2 ? 1.99526f : 1.f;
        const float attack=float(std::exp(-1./(sr*.002))),release=float(std::exp(-1./(sr*.09)));
        const double offsets[]{0,.25,.5,-.25};
        float peak=0;
        // Keep coefficient updates bounded while smoothing automated macro moves.
        for(int start=0;start<n;start+=16){
            const int len=jmin(16,n-start);
            const float x=smooth.skip(len);
            if constexpr(model==Room){
                Reverb::Parameters p;
                p.roomSize=.12f+.84f*x; p.damping=.12f+.76f*x; p.width=1; p.freezeMode=0;
                p.wetLevel=.42f*x; p.dryLevel=.5f*(1-.25f*x);
                if(x!=lastRoom){room.setParameters(p);lastRoom=x;}
                // JUCE smooths internal gains. Keep the neutral path bit-transparent.
                if(x==0){if(roomActive)room.reset();roomActive=false;continue;}
                roomActive=true;
                if(channels==2) room.processStereo(b.getWritePointer(0,start),b.getWritePointer(1,start),len);
                else room.processMono(b.getWritePointer(0,start),len);
                continue;
            }
            double cutoff=100,q=.707;
            if constexpr(model==Air)cutoff=2400+6800*x*x;
            if constexpr(model==Bass)cutoff=95+115*x;
            if constexpr(model==Sweep){
                cutoff=30*std::pow(jmin(20000.,.46*sr)/30.,x);
                constexpr double qs[]{.6,.85,1.8,5.5}; q=qs[mode];
            }
            const double g=std::tan(MathConstants<double>::pi*jmin(cutoff,sr*.46)/sr);
            const double k=1/q, norm=1/(1+g*(g+k));
            const float boost=Decibels::decibelsToGain((model==Bass?14:15)*x)-1;
            for(int i=start;i<start+len;++i){
                float gain=1;
                if constexpr(model==Press || model==Lift){
                    float level=0;for(int c=0;c<channels;++c)level=jmax(level,std::abs(b.getSample(c,i)));
                    if constexpr(model==Press) level*=inputScale;
                    float coeff=level>envelope ? attack : release;
                    envelope=level+coeff*(envelope-level);
                    const float db=Decibels::gainToDecibels(envelope,-100.f);
                    if constexpr(model==Press){
                        float gr=jmax(0.f,db+6+30*x)*(1-1/(1+15*x));
                        gain=inputScale*Decibels::decibelsToGain(9*x-gr);
                    }else{
                        gain=Decibels::decibelsToGain(x*jlimit(0.f,24.f,(-db-2)*.74f));
                        const float target=1.f/jmax(1.f,level*gain);
                        limiter=jmin(target,1.f+release*(limiter-1.f));
                        gain*=limiter;
                    }
                }
                if constexpr(model==Dip){
                    double t=phase+offsets[int(offset->load())];t-=std::floor(t);
                    const double envelopeShape = .5-.5*std::cos(MathConstants<double>::pi*jmin(1.,t/.8));
                    // Round the last 2% of the cycle into the next downbeat.
                    const double tail = t>.98 ? .5+.5*std::cos(MathConstants<double>::pi*(t-.98)/.02) : 1.;
                    gain=float(1-x+x*envelopeShape*tail); phase+=inc;phase-=std::floor(phase);
                }
                for(int c=0;c<channels;++c){
                    float dry=b.getSample(c,i),out=dry;
                    if constexpr(model==Air || model==Bass){
                        // Trapezoidal one-pole split: complementary low and high bands.
                        double v=(dry-memory[c][0])*g/(1+g);
                        double low=memory[c][0]+v;
                        memory[c][0]=low+v;
                        if constexpr(model==Air)out=dry+boost*float(dry-low);
                        else out=dry+boost*float(low);
                    }
                    if constexpr(model==Sweep){
                        auto& s=memory[c]; double v=(s[0]+g*(dry-s[1]))*norm;
                        double low=s[1]+g*v; s[0]=2*v-s[0]; s[1]=2*low-s[1];


                        out=float(low+jlimit(0.f,1.f,(x-.95f)*20)*(dry-low));
                        if(x>=1)out=dry;
                    }
                    if constexpr(model==Press)out=dry+x*(std::tanh(dry*gain*(1+1.6f*x))/(1+1.6f*x)-dry);
                    if constexpr(model==Lift)out=dry+x*(dry*gain-dry);
                    if constexpr(model==Dip)out=dry*gain;
                    b.setSample(c,i,out);
                }
            }
        }
        for(int c=0;c<channels;++c)peak=jmax(peak,b.getMagnitude(c,0,n));
        float old=meter.load();while(old<peak && !meter.compare_exchange_weak(old,peak)){}
    }
private:
    std::atomic<float> *amount{},*option{},*offset{};
    double sr=48000,phase=0; float envelope=0,limiter=1,lastRoom=-1;
    std::array<std::array<double,2>,2> memory{};
    SmoothedValue<float> smooth;
    bool roomActive=false;
    Reverb room;
};
#include "MacroPanel.h"
AudioProcessorEditor* Macro::createEditor(){return new MacroPanel(*this);}
}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Macro;}

