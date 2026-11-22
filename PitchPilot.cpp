#include <juce_audio_processors/juce_audio_processors.h>
#include "SonmooBrand.h"
#include <array>
#include <cmath>

namespace {
using namespace juce;
using State = AudioProcessorValueTreeState;
const StringArray roots {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const StringArray scales {"Chromatic","Major","Natural minor","Harmonic minor","Major pentatonic","Minor pentatonic"};
const StringArray ranges {"Bass","Baritone","Tenor","Alto","Mezzo","Soprano","Generic"};
enum Control { Speed, Transition, Cents, Hold, Amount, Vibrato, Reference, Range, Root, Scale,
               Enabled, Formant, Midi, Linked, FirstKey };
State::ParameterLayout layout()
{
    State::ParameterLayout p;
    auto number = [&](const char* id,const char* name,float lo,float hi,float step,float initial)
    { p.add(std::make_unique<AudioParameterFloat>(ParameterID{id,1},name,NormalisableRange<float>{lo,hi,step},initial)); };
    auto choice = [&](String id,String name,StringArray values,int initial)
    { p.add(std::make_unique<AudioParameterChoice>(ParameterID{id,1},name,values,initial)); };
    auto toggle = [&](const char* id,const char* name,bool initial)
    { p.add(std::make_unique<AudioParameterBool>(ParameterID{id,1},name,initial)); };
    number("speed","Speed (ms)",0,800,1,15); number("transition","Note transition (ms)",0,800,1,120);
    number("cents","Tolerance (cents)",0,100,1,0); number("hold","Tolerance time (ms)",0,300,1,0);
    number("amount","Correction (%)",0,100,1,100); number("vibrato","Vibrato (%)",0,200,1,100);
    number("reference","Reference (Hz)",415,466,.1f,440);
    choice("range","Voice range",ranges,6); choice("root","Root",roots,0); choice("scale","Scale",scales,0);
    toggle("enabled","Correction on",true); toggle("formant","Preserve formants",true);
    toggle("midi","MIDI target",false); toggle("link","Link transition to speed",false);
    for(int n=0;n<12;++n) choice("key"+String(n),roots[n]+" override",{"Scale","Allow","Exclude"},0);
    toggle("group","Group octaves",true);
    toggle("vibratoOn","Vibrato control",true);
    number("toneLevel","Reference tone (dB)",-60,-6,.5f,-20);
    for(int n=24;n<84;++n)
        choice("note"+String(n),"Local override "+String(n),{"Inherit","Allow","Exclude"},0);
    return p;
}

class PitchPilot final : public AudioProcessor {
public:
    PitchPilot() : AudioProcessor(BusesProperties().withInput("Vocal",AudioChannelSet::stereo(),true)
                                                   .withOutput("Output",AudioChannelSet::stereo(),true)),
                   state(*this,nullptr,"PitchPilot",layout())
    {
        for (int i=0;i<(int)params.size();++i)
            params[i]=state.getRawParameterValue(dynamic_cast<RangedAudioParameter*>(getParameters()[i])->getParameterID());
    }
    State state;
    std::atomic<float> displayNote{-1}, deviation{0}, inputLevel{0}, outputLevel{0};
    std::atomic<int> targetNote{-1}, audition{-1}, keyboardTarget{-1};
    float value(int i) const { return params[i]->load(std::memory_order_relaxed); }
    enum { Group = FirstKey+12, VibratoOn, ToneLevel, FirstLocal };
    bool allowed(int note) const
    {
        if(note>=24 && note<84 && value(Group)<.5f)
        {
            const int local=(int)value(FirstLocal+note-24);
            if(local) return local==1;
        }
        const int key=note%12, overrideMode=(int)value(FirstKey+key);
        if(overrideMode) return overrideMode==1;
        static constexpr int masks[]{4095,2741,1453,2477,661,1193};
        return (masks[(int)value(Scale)] & (1<<((key-(int)value(Root)+12)%12)))!=0;
    }
    const String getName() const override {return "PitchPilot";}
    bool acceptsMidi() const override {return true;}
    bool producesMidi() const override {return false;}
    double getTailLengthSeconds() const override {return 0.05;}
    bool hasEditor() const override {return true;}
    AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override {return {};}
    void changeProgramName(int,const String&) override {}
    bool isBusesLayoutSupported(const BusesLayout& b) const override
    {
        auto set=b.getMainOutputChannelSet();
        return b.getMainInputChannelSet()==set && (set==AudioChannelSet::mono() || set==AudioChannelSet::stereo());
    }
    void getStateInformation(MemoryBlock& out) override {if(auto xml=state.copyState().createXml()) copyXmlToBinary(*xml,out);}
    void setStateInformation(const void* bytes,int len) override
    {if(auto xml=getXmlFromBinary(bytes,len);xml && xml->hasTagName(state.state.getType())) state.replaceState(ValueTree::fromXml(*xml));}
    void releaseResources() override {}
    void prepareToPlay(double sr,int) override
    {
        rate=sr; stride=jmax(1,roundToInt(sr/6000)); detectorRate=sr/stride;
        latency=roundToInt(sr*.045); setLatencySamples(latency);
        delay.setSize(2,latency*4+8); delay.clear(); write=0;
        window.fill(0); keys.fill(0); tick=decCount=hop=pos=0; average=lowpass=0;
        target=pending=-1; pendingMs=0; period=sr/220; phase=0; gain=0; ratio=1;
        head=oldHead=latency; goal=1; voiced=false; validBefore=false; monitorGain=0; oscillator=0;
        displayNote=-1; targetNote=-1; inputLevel=outputLevel=0;
    }
private:
    std::array<std::atomic<float>*,FirstLocal+60> params{};
    std::array<float,512> window{};
    std::array<int64_t,128> keys{};
    AudioBuffer<float> delay;
    double rate=48000,detectorRate=6000,period=218,phase=0,head=0,oldHead=0,goal=1,ratio=1;
    double oscillator=0,slowNote=0,glide=0,pendingMs=0;
    bool voiced=false,validBefore=false;
    int64_t tick=0;
    int stride=8,latency=2160,write=0,pos=0,hop=0,decCount=0,target=-1,pending=-1;
    float average=0,lowpass=0,gain=0,monitorGain=0;
    float detect()
    {
        static constexpr int low[]{55,65,90,125,145,175,55},high[]{350,460,620,800,960,1250,1250};
        int r=(int)value(Range),first=jmax(2,(int)(detectorRate/high[r])),last=jmin(150,(int)std::ceil(detectorRate/low[r]));
        std::array<float,152> dif{}, raw{};
        auto at=[&](int age){return window[(pos-1-age+1024)&511];};
        float energy=0;
        for(int i=0;i<128;++i) energy+=at(i)*at(i);
        if(energy<.00002f) return 0;
        double cumulative=0;
        for(int lag=1;lag<=last+1;++lag)
        {
            float d=0;
            for(int i=0;i<128;++i){const float x=at(i)-at(i+lag); d+=x*x;}
            raw[lag]=d; cumulative+=d; dif[lag]=cumulative>1e-15 ? (float)(d*lag/cumulative) : 1;
            if(lag>first && dif[lag-1]<.15f && dif[lag]>=dif[lag-1])
            {
                const int k=lag-1;
                const float a=raw[k-1], b=raw[k], c=raw[k+1];
                const double offset=jlimit(-.5,.5,.5*(a-c)/jmax(1e-8f,a+c-2*b));
                const float hz=(float)(detectorRate/(k+offset));
                if(hz<low[r]*.98 || hz>high[r]*1.02) return 0;
                return hz;
            }
        }
        return 0;
    }
    void analyse()
    {
        const float hz=detect();
        voiced=hz>0;
        if(!voiced){target=pending=-1; displayNote=-1;targetNote=-1;validBefore=false; goal=1;return;}
        period=rate/hz;
        const double note=69+12*std::log2(hz/value(Reference)), dt=48000/detectorRate;
        int candidate=keyboardTarget.load();
        if(candidate<0 && value(Midi)>.5f)
        {
            auto best=std::max_element(keys.begin(),keys.end());
            if(*best) candidate=(int)(best-keys.begin());
        }
        const bool forced=candidate>=0;
        if(!forced)
        {
            double best=1e8;
            for(int n=jmax(0,(int)note-12);n<=jmin(127,(int)note+13);++n)
                if(allowed(n) && std::abs(note-n)<best){candidate=n;best=std::abs(note-n);}
        }
        displayNote=(float)note;
        if(candidate<0){targetNote=target=-1;voiced=validBefore=false;goal=1;return;}
        if(candidate!=target)
        {
            if(candidate!=pending){pending=candidate;pendingMs=0;}
            pendingMs+=dt;
            const bool beyond=std::abs(note-target)-std::abs(note-candidate)>=.02*value(Cents);
            if(forced || target<0 || !allowed(target) || (beyond && pendingMs>=value(Hold))) target=candidate;
        } else {pending=-1;pendingMs=0;}
        if(!validBefore){slowNote=note;glide=target;}
        slowNote+=(note-slowNote)*(1-std::exp(-dt/140));
        const double transition=value(Linked)>.5 ? value(Speed):value(Transition);
        glide+=(target-glide)*(1-std::exp(-dt/jmax(.01,transition)));
        // Remove fast deviations at 0%, retain them at 100%, exaggerate at 200%.
        const double retainedVibrato=value(VibratoOn)>.5f ? value(Vibrato)*.01 : 0.;
        double delta=glide-note+retainedVibrato*(note-slowNote);
        goal=std::exp2(jlimit(-12.,12.,delta*value(Amount)*.01)/12);
        deviation=(float)((note-target)*100); targetNote=target; validBefore=true;
    }
    float read(int ch,double age) const
    {
        const int size=delay.getNumSamples(); age=jlimit(1.,(double)size-2,age);
        int i=(int)age, a=(write-i+size)%size,b=(a+size-1)%size;
        return (float)(delay.getSample(ch,a)+(age-i)*(delay.getSample(ch,b)-delay.getSample(ch,a)));
    }
    void midiEvent(const MidiMessage& m)
    {
        if(m.isNoteOn()) keys[m.getNoteNumber()]=++tick;
        else if(m.isNoteOff()) keys[m.getNoteNumber()]=0;
        else if(m.isAllNotesOff() || m.isAllSoundOff()) keys.fill(0);
    }
public:
    void processBlock(AudioBuffer<float>& b,MidiBuffer& midi) override
    {
        ScopedNoDenormals guard;
        auto event=midi.cbegin(); float inPeak=0,outPeak=0;
        const double smoothing=1-std::exp(-1/(rate*.001*jmax(1.f,value(Speed))));
        const float toneGain=Decibels::decibelsToGain(value(ToneLevel));
        for(int i=0;i<b.getNumSamples();++i)
        {
            while(event!=midi.cend() && (*event).samplePosition<=i){midiEvent((*event).getMessage());++event;}
            float mono=0;
            for(int ch=0;ch<b.getNumChannels();++ch)
            {float in=b.getSample(ch,i);delay.setSample(ch,write,in);mono+=in;inPeak=jmax(inPeak,std::abs(in));}
            lowpass+=(mono/b.getNumChannels()-lowpass)*jmin(1.f,(float)(14500/rate));
            average+=lowpass;
            if(++decCount==stride)
            {
                window[pos]=average/stride;pos=(pos+1)&511;average=0;decCount=0;
                if(++hop==48){hop=0;analyse();}
            }
            ratio+=smoothing*(goal-ratio);
            // Two overlapping grains. Each new grain starts at the same source pitch phase.
            const double outputPeriod=period/ratio;
            const double readSpeed=value(Formant)>.5f ? 1. : ratio;
            phase+=1/outputPeriod;
            head+=1-readSpeed;oldHead+=1-readSpeed;
            if(phase>=1)
            {
                phase-=1;oldHead=head;

                // Anchor the new grain one source cycle after the previous grain's start.
                const double desired=head+outputPeriod*readSpeed-period;
                head=desired-std::round((desired-latency)/period)*period;
            }
            const double w=.5-.5*std::cos(MathConstants<double>::pi*phase);
            const float wet=(voiced && value(Enabled)>.5f && value(Amount)>0 && std::abs(ratio-1)>.0002) ? 1.f:0.f;
            gain+=(wet-gain)*(float)jmin(1.,250/rate);
            const int listen=audition.load();monitorGain+=((listen>=0?toneGain:0.f)-monitorGain)*(float)jmin(1.,250/rate);
            if(listen>=0) oscillator+=value(Reference)*std::exp2((listen-69)/12.)/rate;
            oscillator-=std::floor(oscillator);
            for(int ch=0;ch<b.getNumChannels();++ch)
            {
                const float dry=read(ch,latency);
                const double shifted=w*read(ch,head)+(1-w)*read(ch,oldHead);
                float out=(float)(dry+gain*(shifted-dry)+monitorGain*std::sin(MathConstants<double>::twoPi*oscillator));
                b.setSample(ch,i,out);outPeak=jmax(outPeak,std::abs(out));
            }
            write=(write+1)%delay.getNumSamples();
        }
        const float decay=(float)std::exp(-b.getNumSamples()/(rate*.12));
        inputLevel=jmax(inPeak,inputLevel.load()*decay);outputLevel=jmax(outPeak,outputLevel.load()*decay);
        midi.clear();
    }
};
#include "PitchPanel.h"
AudioProcessorEditor* PitchPilot::createEditor(){return new PitchPanel(*this);}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new PitchPilot();}
