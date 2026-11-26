#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <complex>
#include <iostream>
#include <stdexcept>
using namespace juce;

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    ScopedJuceInitialiser_GUI gui;
    VST3PluginFormat format;
    OwnedArray<PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, argv[1]);
    if (descriptions.size() != 1 || descriptions[0]->name != "PrismEQ") return 3;
    for (double rate : {44100., 48000., 96000.})
    {
        String error;
        const int block = rate == 44100 ? 63 : rate == 48000 ? 128 : 513;
        auto plugin = format.createInstanceFromDescription(*descriptions[0], rate, block, error);
        if (!plugin) return 4;
        plugin->setPlayConfigDetails(2, 2, rate, block);
        plugin->prepareToPlay(rate, block);
        auto set = [&](String key, String value)
        {
            for (auto* p : plugin->getParameters()) if (p->getName(128) == key)
            {
                p->setValueNotifyingHost(p->getValueForText(value)); return;
            }
            throw std::runtime_error(key.toStdString());
        };
        using Complex = std::complex<double>;
        const auto run = [&]
        {
            const int latency = plugin->getLatencySamples();
            if (latency != 1024 && latency != 0) throw std::runtime_error("Latency");
            std::array<Complex,3> result{};
            AudioBuffer<float> audio(2,block); MidiBuffer midi;
            for (int pos = 0; pos < 65536; pos += block)
            {
                const int length = jmin(block,65536-pos);
                audio.setSize(2,length,false,false,true);
                for (int i=0; i<length; ++i)
                {
                    const double phase = MathConstants<double>::twoPi*(pos+i)/1024;
                    const float x = float(.4*std::cos(20*phase)+.008*std::cos(30*phase));
                    for (int c=0;c<2;++c) audio.setSample(c,i,x);
                }
                plugin->processBlock(audio,midi);
                for (int i=0;i<length;++i) if (pos+i>=32768)
                {
                    const double phase = -MathConstants<double>::twoPi*(pos+i-latency)/1024;
                    result[0] += double(audio.getSample(0,i))*std::polar(1.,20*phase);
                    result[1] += double(audio.getSample(0,i))*std::polar(1.,30*phase);
                    result[2] += double(audio.getSample(1,i))*std::polar(1.,20*phase);
                }
            }
            for(int i=0;i<3;++i) result[i] /= 16384*(i==1?.008:.4);
            return result;
        };
        const auto flat=run();
        if (plugin->getLatencySamples()!=1024) return 6;
        set("Band 24 Enabled","On"); set("Band 24 Frequency",String(rate*20/1024));
        set("Band 24 Q","1"); set("Band 24 Gain","6"); set("Band 24 Channel","Left");
        const auto boost=run();
        MemoryBlock state; plugin->getStateInformation(state);
        set("Band 24 Gain","0"); set("Band 24 Channel","Stereo");
        set("Band 24 Threshold","-25"); set("Band 24 Dynamic Range","-12");
        set("Band 24 Detection","Spectral");
        const auto spectral=run();
        plugin->setStateInformation(state.getData(),int(state.getSize()));
        const auto restored=run();
        set("Engine","Live");
        AudioBuffer<float> warmup(2,block); warmup.clear(); MidiBuffer empty;
        plugin->processBlock(warmup,empty);
        const auto live=run();
        set("Band 24 Channel","Stereo"); set("Band 24 Gain","0"); set("Band 24 Threshold","-30");
        set("Band 24 Dynamic Range","-9"); set("Band 24 Detection","Split");
        const auto dynamic=run();
        const bool pass = plugin->getLatencySamples()==0 && std::abs(live[0]-boost[0])<.005
            && std::abs(dynamic[0])<.5 && std::abs(dynamic[0])>.3 && std::abs(flat[0]-Complex{1,0})<.001 && std::abs(flat[1]-Complex{1,0})<.001
            && std::abs(boost[0])>1.95 && std::abs(boost[0])<2.04 && std::abs(boost[2]-Complex{1,0})<.001
            && std::abs(spectral[0])<.5 && std::abs(spectral[1])>.9 && std::abs(spectral[1])<1.1
            && std::abs(restored[0]-boost[0])<.005;
        std::cout<<rate<<" Hz  flat "<<std::abs(flat[0])<<"  left/right "<<std::abs(boost[0])<<"/"<<std::abs(boost[2])
                 <<"  spectral strong/weak "<<std::abs(spectral[0])<<"/"<<std::abs(spectral[1])
                 <<"  restored "<<std::abs(restored[0])<<(pass?" PASS":" FAIL")<<std::endl;
        if (!pass) return 5;
        plugin->releaseResources();
    }
    return 0;
}
