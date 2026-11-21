#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace {
constexpr int effect = EFFECT_KIND;
using State = juce::AudioProcessorValueTreeState;

State::ParameterLayout makeLayout()
{
    State::ParameterLayout layout;
    auto add = [&] (const char* id, const char* label, float lo, float hi, float value)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, 1}, label, juce::NormalisableRange<float>{lo, hi}, value));
    };
    if constexpr (effect == 0)
    {
        add("time", "Time (ms)", 10, 2000, 375);
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"sync", 1}, "Tempo sync",
            juce::StringArray{"Off", "1/16", "1/8", "1/8 dotted", "1/4", "1/2", "1/1"}, 0));
        add("feedback", "Feedback (%)", 0, 95, 32);
        add("lowcut", "Low Cut (Hz)", 20, 2000, 120);
        add("highcut", "High Cut (Hz)", 500, 20000, 6500);
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"ping", 1}, "Ping-Pong", false));
        add("mix", "Mix (%)", 0, 100, 25);
    }
    else if constexpr (effect == 1)
    {
        add("threshold", "Threshold (dB)", -60, 0, -18);
        add("ratio", "Ratio", 1, 20, 4);
        add("attack", "Attack (ms)", 0.1f, 200, 10);
        add("release", "Release (ms)", 5, 2000, 120);
        add("knee", "Knee (dB)", 0, 24, 6);
        add("makeup", "Makeup (dB)", -12, 24, 0);
        add("mix", "Mix (%)", 0, 100, 100);
    }
    else
    {
        add("threshold", "Threshold (dB)", -36, 0, 0);
        add("ceiling", "Ceiling (dBFS)", -18, 0, -0.3f);
        add("release", "Release (ms)", 5, 1000, 80);
    }
    return layout;
}

class Plugin final : public juce::AudioProcessor
{
public:
    Plugin() : AudioProcessor(BusesProperties()
                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
               state(*this, nullptr, "PARAMETERS", makeLayout()) {}

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override
    {
        return effect == 0 ? 720.0 : effect == 2 ? 0.002 : 0.0;
    }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override
    {
        return new juce::GenericAudioProcessorEditor(*this);
    }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {
        const auto channels = layout.getMainOutputChannelSet();
        return layout.getMainInputChannelSet() == channels &&
               (channels == juce::AudioChannelSet::mono() ||
                channels == juce::AudioChannelSet::stereo());
    }

    void prepareToPlay(double sampleRate, int) override
    {
        rate = std::max(1.0, sampleRate);
        lookahead = std::max(1, juce::roundToInt(rate * 0.002));
        if constexpr (effect != 1)
        {
            const int size = effect == 0 ? static_cast<int>(rate * 8) + 2 : lookahead + 1;
            for (auto& line : ring)
                line.resize(static_cast<size_t>(size));
        }
        setLatencySamples(effect == 2 ? lookahead : 0);
        reset();
    }

    void releaseResources() override {}
    void reset() override
    {
        for (auto& line : ring)
            std::fill(line.begin(), line.end(), 0.0f);
        cursor = hold = 0;
        reductionDb = 0;
        limiterGain = 1;
        delay = 0;
        lowpass = {};
        bass = {};
    }

    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals guard;
        if constexpr (effect == 0) echo(audio);
        if constexpr (effect == 1) compress(audio);
        if constexpr (effect == 2) limit(audio);
    }

    void getStateInformation(juce::MemoryBlock& data) override
    {
        if (auto xml = state.copyState().createXml())
            copyXmlToBinary(*xml, data);
    }

    void setStateInformation(const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary(data, size))
            if (xml->hasTagName(state.state.getType()))
                state.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    float param(const char* id) const { return state.getRawParameterValue(id)->load(); }
    float decay(float ms) const { return float(std::exp(-1000.0 / (rate * ms))); }
    float pole(float hz) const
    {
        return float(-std::expm1(-2.0 * juce::MathConstants<double>::pi * hz / rate));
    }

    void echo(juce::AudioBuffer<float>& audio)
    {
        const auto length = static_cast<int>(ring[0].size());
        if (length < 2) return;
        auto samples = audio.getArrayOfWritePointers();
        const int channels = audio.getNumChannels();
        const float mix = param("mix") * 0.01f;
        const float feedback = param("feedback") * 0.01f;
        const bool ping = channels == 2 && param("ping") > 0.5f;
        const float hp = pole(param("lowcut")), lp = pole(param("highcut"));
        const int sync = std::clamp(int(param("sync")), 0, 6);
        double seconds = param("time") * 0.001;
        if (sync > 0)
        {
            double bpm = 120.0;
            if (auto* head = getPlayHead())
                if (const auto position = head->getPosition())
                    if (const auto tempo = position->getBpm())
                        bpm = *tempo;
            constexpr std::array<double, 7> beats{0, .25, .5, .75, 1, 2, 4};
            seconds = beats[sync] * 60.0 / std::max(30.0, bpm);
        }
        const double target = std::clamp(seconds * rate, 1.0, double(length - 2));
        if (delay == 0) delay = target;
        const double slew = 1.0 - decay(25);
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            delay += slew * (target - delay);
            double read = cursor - delay;
            if (read < 0) read += length;
            const int first = int(read);
            const int second = (first + 1) % length;
            const float fraction = float(read - first);
            for (int ch = 0; ch < channels; ++ch)
            {
                const auto& line = ring[ch];
                const float tap = line[first] + fraction * (line[second] - line[first]);
                bass[ch] += hp * (tap - bass[ch]);
                lowpass[ch] += lp * (tap - bass[ch] - lowpass[ch]);
            }
            const float mono = ping ? 0.5f * (samples[0][i] + samples[1][i]) : 0;
            for (int ch = 0; ch < channels; ++ch)
            {
                const float dry = samples[ch][i];
                const float send = ping ? (ch == 0 ? mono : 0) : dry;
                ring[ch][cursor] = send + feedback * lowpass[ping ? 1 - ch : ch];
                samples[ch][i] = dry + mix * (lowpass[ch] - dry);
            }
            cursor = (cursor + 1) % length;
        }
    }

    void compress(juce::AudioBuffer<float>& audio)
    {
        auto samples = audio.getArrayOfWritePointers();
        const int channels = audio.getNumChannels();
        const float threshold = param("threshold"), knee = param("knee");
        const float slope = 1 - 1 / param("ratio");
        const float attack = decay(param("attack")), release = decay(param("release"));
        const float makeup = juce::Decibels::decibelsToGain(param("makeup"));
        const float mix = param("mix") * 0.01f;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            float peak = 0;
            for (int ch = 0; ch < channels; ++ch)
                peak = std::max(peak, std::abs(samples[ch][i]));
            const float over = juce::Decibels::gainToDecibels(peak, -140.0f) - threshold;
            float target = slope * std::max(0.0f, over);
            if (knee > 0 && std::abs(over) < knee * .5f)
            {
                const float offset = over + knee * .5f;
                target = slope * offset * offset / (2 * knee);
            }
            const float coefficient = target > reductionDb ? attack : release;
            reductionDb = target + coefficient * (reductionDb - target);
            const float gain = 1 - mix + mix * makeup * juce::Decibels::decibelsToGain(-reductionDb);
            for (int ch = 0; ch < channels; ++ch)
                samples[ch][i] *= gain;
        }
    }

    void limit(juce::AudioBuffer<float>& audio)
    {
        const auto length = static_cast<int>(ring[0].size());
        if (length < 2) return;
        auto samples = audio.getArrayOfWritePointers();
        const int channels = audio.getNumChannels();
        const float ceiling = juce::Decibels::decibelsToGain(param("ceiling"));
        const float drive = ceiling / juce::Decibels::decibelsToGain(param("threshold"));
        const float release = decay(param("release"));
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            float peak = 0;
            for (int ch = 0; ch < channels; ++ch)
            {
                ring[ch][cursor] = samples[ch][i] * drive;
                peak = std::max(peak, std::abs(ring[ch][cursor]));
            }
            const float target = peak > ceiling ? ceiling / peak : 1.0f;
            if (target <= limiterGain && target < 1)
            {
                limiterGain = target;
                hold = lookahead;
            }
            else if (hold > 0)
                --hold;
            else
                limiterGain = 1 + release * (limiterGain - 1);

            cursor = (cursor + 1) % length;
            float delayedPeak = 0;
            for (int ch = 0; ch < channels; ++ch)
                delayedPeak = std::max(delayedPeak, std::abs(ring[ch][cursor]));
            const float gain = delayedPeak > ceiling
                ? std::min(limiterGain, ceiling / delayedPeak) : limiterGain;
            for (int ch = 0; ch < channels; ++ch)
                samples[ch][i] = ring[ch][cursor] * gain;
        }
    }

    State state;
    std::array<std::vector<float>, 2> ring;
    std::array<float, 2> lowpass{}, bass{};
    double rate = 44100, delay = 0;
    int lookahead = 0, cursor = 0, hold = 0;
    float reductionDb = 0, limiterGain = 1;
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Plugin(); }
