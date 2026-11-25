#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <stdexcept>
using namespace juce;
struct Clock final : AudioPlayHead {
    int64_t sample=0; double sr=48000,bpm=120;
    Optional<PositionInfo> getPosition() const override {
        PositionInfo i; i.setTimeInSamples(sample);i.setTimeInSeconds(sample/sr);i.setBpm(bpm);
        i.setPpqPosition(sample/sr*bpm/60);i.setIsPlaying(true);return i;
    }
};
struct Stats {double diff=0,rms=0,peak=0,tail=0;};
int main(int argc,char** argv){
    if(argc!=2)return 2;
    ScopedJuceInitialiser_GUI gui; VST3PluginFormat fmt; OwnedArray<PluginDescription> ds;
    fmt.findAllTypesForFile(ds,argv[1]); if(ds.size()!=1)return 3;
    const String name=File(argv[1]).getFileNameWithoutExtension();
    String err;Clock clock; auto p=fmt.createInstanceFromDescription(*ds[0],48000,128,err);
    if(!p){std::cerr<<err;return 4;}p->setPlayHead(&clock);
    auto parameter=[&](String key){for(auto* v:p->getParameters())if(v->getName(128)==key)return v;
        throw std::runtime_error(("Missing "+key).toStdString());};
    const String knob = name=="AirLift"?"Air":name=="BassBody"?"Body":name=="SweepTone"?"Cutoff":
        name=="DrumPress"?"Pressure":name=="LevelLift"?"Level":name=="BeatDip"?"Depth":name=="RoomBloom"?"Space":"Drive";
    auto* main=parameter(knob); auto choose=[&](String key,String choice){auto* a=parameter(key);a->setValueNotifyingHost(a->getValueForText(choice));};
    for(double sr:{44100.,48000.,96000.}){
        clock.sr=sr; const int block=sr==44100?63:sr==48000?128:513;
        p->setPlayConfigDetails(2,2,sr,block);p->prepareToPlay(sr,block);
        AudioBuffer<float> b(2,block); MidiBuffer midi;
        auto render=[&](double hz,float amp,bool impulse=false){
            Stats s; double raw=0,wet=0; clock.sample=0;
            const int last=int(sr*1.2);
            for(int k=0;k<last;k+=block){
                int len=jmin(block,last-k);b.setSize(2,len,false,false,true);
                for(int i=0;i<len;++i){
                    float x=impulse?(k+i==int(sr*.1)?amp:0.f):amp*std::sin(MathConstants<double>::twoPi*hz*(k+i)/sr);
                    b.setSample(0,i,x);b.setSample(1,i,x);
                }
                auto input=b;clock.sample=k;p->processBlock(b,midi);
                for(int i=0;i<len;++i){double x=input.getSample(0,i),y=b.getSample(0,i);
                    if(!std::isfinite(y)||!std::isfinite(b.getSample(1,i)))throw std::runtime_error("nonfinite");
                    if(k+i>sr*.25){s.diff=jmax(s.diff,std::abs(x-y));raw+=x*x;wet+=y*y;s.peak=jmax(s.peak,std::abs(y));}
                    if(k+i>sr*.11)s.tail+=y*y;
                }
            }s.rms=std::sqrt(wet/jmax(raw,1e-18));return s;
        };
        const float neutral=name=="SweepTone"?1.f:0.f;
        main->setValueNotifyingHost(neutral);
        auto dry=render(523,.13f);
        bool ok=dry.diff<.00002;
        main->setValueNotifyingHost(name=="SweepTone"?.3f:1.f);
        render(523,.13f);
        MemoryBlock state;p->getStateInformation(state);float saved=main->getValue();
        main->setValueNotifyingHost(neutral);render(523,.13f);p->setStateInformation(state.getData(),int(state.getSize()));
        const auto restored=render(523,.13f);
        ok=ok && restored.diff>.00001;
        if(!ok)std::cerr<<"stored "<<saved<<" reported "<<main->getValue()<<" restored diff "<<restored.diff<<std::endl;
        double a=0,bval=0;
        if(name=="AirLift"||name=="BassBody"){
            a=render(60,.1f).rms;bval=render(12000,.1f).rms;
            ok &= name=="AirLift"? bval>a*2 : a>bval*2;
        }else if(name=="SweepTone"){
            a=render(60,.1f).rms;bval=render(8000,.1f).rms;ok &= a>bval*8;
            main->setValueNotifyingHost(.5f);choose("Resonance","None");a=render(770,.08f).rms;
            choose("Resonance","High");bval=render(770,.08f).rms;ok &= bval>a*2;
        }else if(name=="DrumPress"||name=="LevelLift"){
            a=render(220,.01f).rms;bval=render(220,.8f).rms;ok &= a>bval*2;
            if(name=="DrumPress"){choose("Input","Pad -6 dB");auto pad=render(220,.01f);
                choose("Input","Boost +6 dB");auto boost=render(220,.01f);ok &= boost.rms>pad.rms*1.1;}
        }else if(name=="RoomBloom"){
            auto tail=render(0,.5f,true);a=tail.tail;ok &= a>.00001;
            auto buses=p->getBusesLayout();buses.getChannelSet(true,0)=AudioChannelSet::mono();
            p->releaseResources();
            ok &= p->setBusesLayout(buses);
            p->prepareToPlay(sr,block);
            double leftTail=0,rightTail=0,width=0;
            for(int k=0;k<int(sr*.6);k+=block){
                b.setSize(2,block,false,false,true);b.clear();
                if(!k)b.setSample(0,0,.5f);
                p->processBlock(b,midi);
                if(k<sr*.02)continue;
                for(int i=0;i<block;++i){
                    const double l=b.getSample(0,i),r=b.getSample(1,i);
                    leftTail+=l*l;rightTail+=r*r;width+=(l-r)*(l-r);
                }
            }
            ok &= leftTail>1.e-6 && rightTail>1.e-6 && width>1.e-6;
        }else if(name=="BeatDip"){
            choose("Rate","1/4");choose("Offset","0");clock.bpm=120;
            auto rms=render(210,.3f).rms;ok &= rms>.3 && rms<.9;
            auto at=[&](int64_t cursor){clock.sample=cursor;b.setSize(2,block,false,false,true);
                for(int c=0;c<2;++c)FloatVectorOperations::fill(b.getWritePointer(c),.5f,block);
                p->processBlock(b,midi);return b.getSample(0,0);};
            a=at(int(sr*.5));bval=at(int(sr*.375));ok &= a<.02 && bval>.45;
            choose("Offset","180");ok &= at(int(sr*.5))>.25;
            choose("Offset","0");choose("Rate","1/8");clock.bpm=90;ok &= at(int(sr/3))<.02;
        }else {auto hot=render(997,.4f);a=hot.diff;ok &= a>.01;}
        std::cout<<name<<" "<<sr<<" neutral error "<<dry.diff<<" A "<<a<<" B "<<bval<<" "<<(ok?"PASS":"FAIL")<<std::endl;
        p->releaseResources();if(!ok)return 6;
    }
    p->setPlayHead(nullptr);return 0;
}
