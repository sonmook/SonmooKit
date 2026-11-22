#include <juce_audio_processors/juce_audio_processors.h>
#include "SonmooBrand.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace {
constexpr int effect = EFFECT_KIND;
using State = juce::AudioProcessorValueTreeState;

}
#include "SonmooEditor.h"
#if EFFECT_KIND == 8
namespace {
#include "FetEditor.h"
}
#endif
namespace {

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
    else if constexpr (effect == 2)
    {
        add("threshold", "Threshold (dB)", -36, 0, 0);
        add("ceiling", "Ceiling (dBFS)", -18, 0, -0.3f);
        add("release", "Release (ms)", 5, 1000, 80);
    }
    else if constexpr (effect == 3)
    {
        add("threshold", "Threshold (dB)", -80, 0, -12.5f);
        add("range", "Range (dB)", -48, 0, -14);
        add("width", "Detection Width (%)", 0, 100, 50);
        add("mode", "Wide to Split (%)", 0, 100, 50);
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"monitor", 1}, "Audition Sibilance", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"lookahead", 1}, "12 ms Lookahead", false));
    }
    else if constexpr (effect == 4)
    {
        add("drive", "Drive", 0, 10, 0);
        add("tone", "Tone (%)", -100, 100, 0);
        add("trim", "Output Trim (dB)", -24, 12, 0);
        add("mix", "Mix (%)", 0, 100, 100);
    }
    else
    {
        add("input", "Input (dB)", -48, 24, 0);
        add("output", "Output (dB)", -48, 24, 0);
        add("attack", "Attack", 1, 7, 3);
        add("release", "Release", 1, 7, 4);
        add("mix", "Mix (%)", 0, 100, 100);
        add("trim", "Trim (dB)", -18, 18, 0);
        const auto choice = [&] (const char* key, const char* label, juce::StringArray items)
        {
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{key, 1}, label, items, 0));
        };
        choice("ratio", "Ratio", {"4:1", "8:1", "12:1", "20:1", "All"});
        choice("meter", "Meter", {"Gain Reduction", "Input", "Output"});
        choice("analog", "Analog Hum", {"Off", "50 Hz", "60 Hz"});
        choice("character", "Character", {"Smooth", "Bright"});
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"makeup", 1}, "Auto Makeup", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypassComp", 1}, "Compression Off", false));
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
#if EFFECT_KIND == 8
        return new FetEditor(*this, state, activity);
#else
        return new SonmooEditor(*this, state, activity);
#endif
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
        if constexpr (effect == 0 || effect == 2 || effect == 3)
        {
            const int size = effect == 0 ? static_cast<int>(rate * 8) + 2 : effect == 3 ? juce::roundToInt(rate * .012) + 1 : lookahead + 1;
            for (auto& line : ring)
                line.resize(static_cast<size_t>(size));
        }
        setLatencySamples(effect == 2 ? lookahead : effect == 3 && param("lookahead") > .5f ? int(ring[0].size()) - 1 : 0);
        reset();
    }

    void releaseResources() override {}
    void reset() override
    {
        for (auto& line : ring)
            std::fill(line.begin(), line.end(), 0.0f);
        activity.input.store(0);
        activity.output.store(0);
        activity.reduction.store(0);
        cursor = hold = 0;
        reductionDb = detectionEnvelope = driveBlend = 0;
        limiterGain = 1;
        humPhase = 0;
        delay = 0;
        lowpass = {};
        bass = {};
        detectorHigh = {};
        detectorLow = {};
        activity.detection.store(0);
        activity.echoTimeMs.store(0);
    }

    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals guard;
        updatePeak(activity.input, signalPeak(audio));
        blockReduction = 0;
        if constexpr (effect == 0) echo(audio);
        if constexpr (effect == 1) compress(audio);
        if constexpr (effect == 2) limit(audio);
        if constexpr (effect == 3) deEss(audio);
        if constexpr (effect == 4) driveTone(audio);
        if constexpr (effect == 8) fetCompress(audio);
        updatePeak(activity.output, signalPeak(audio));
        updatePeak(activity.reduction, blockReduction);
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

    static float signalPeak(const juce::AudioBuffer<float>& audio)
    {
        float peak = 0;
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            peak = std::max(peak, audio.getMagnitude(ch, 0, audio.getNumSamples()));
        return peak;
    }

    static void updatePeak(std::atomic<float>& meter, float peak)
    {
        float previous = meter.load(std::memory_order_relaxed);
        while (peak > previous &&
               !meter.compare_exchange_weak(previous, peak, std::memory_order_relaxed)) {}
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
        activity.echoTimeMs.store(float(target * 1000.0 / rate), std::memory_order_relaxed);
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
        float maximumReduction = 0;
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
            maximumReduction = std::max(maximumReduction, reductionDb);
            const float gain = 1 - mix + mix * makeup * juce::Decibels::decibelsToGain(-reductionDb);
            for (int ch = 0; ch < channels; ++ch)
                samples[ch][i] *= gain;
        }
        blockReduction = maximumReduction;
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
        float minimumGain = 1;
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
            minimumGain = std::min(minimumGain, gain);
        }
        blockReduction = -juce::Decibels::gainToDecibels(minimumGain, -120.0f);
    }

    void deEss(juce::AudioBuffer<float>& audio)
    {
        const int length = int(ring[0].size());
        if (length < 2) return;
        const bool preRead = param("lookahead") > .5f;
        const int latency = preRead ? length - 1 : 0;
        if (getLatencySamples() != latency)
            setLatencySamples(latency);

        const bool audition = param("monitor") > .5f;
        const float width = param("width") * .01f;
        const float detectorHP = pole(6400 - 3400 * width);
        const float detectorLP = pole(std::min(float(rate * .45), 8500 + 4500 * width));
        const float split = pole(4000);
        const float threshold = param("threshold");
        const float maximum = -param("range");
        const float splitMix = param("mode") * .01f;
        const float attack = decay(.35f), release = decay(48);
        const float holdDecay = decay(17);
        const int channels = audio.getNumChannels();
        auto samples = audio.getArrayOfWritePointers();
        float detectedPeak = 0;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            float peak = 0;
            for (int ch = 0; ch < channels; ++ch)
            {
                const float input = samples[ch][i];
                detectorLow[ch] += detectorHP * (input - detectorLow[ch]);
                detectorHigh[ch] += detectorLP * (input - detectorLow[ch] - detectorHigh[ch]);
                peak = std::max(peak, std::abs(detectorHigh[ch]));
                ring[ch][cursor] = input;
            }
            detectedPeak = std::max(detectedPeak, peak);
            detectionEnvelope = std::max(peak, detectionEnvelope * holdDecay);
            const float db = juce::Decibels::gainToDecibels(detectionEnvelope, -100.0f);
            const float target = std::clamp((db - threshold) * .8f, 0.0f, maximum);
            const float smoothing = target > reductionDb ? attack : release;
            reductionDb = target + smoothing * (reductionDb - target);
            blockReduction = std::max(blockReduction, reductionDb);
            const float removedGain = 1 - juce::Decibels::decibelsToGain(-reductionDb);
            const int next = (cursor + 1) % length;
            for (int ch = 0; ch < channels; ++ch)
            {
                const float dry = ring[ch][preRead ? next : cursor];
                bass[ch] += split * (dry - bass[ch]);
                const float removed = removedGain * (dry - splitMix * bass[ch]);
                samples[ch][i] = audition ? removed : dry - removed;
            }
            cursor = next;
        }
        updatePeak(activity.detection, detectedPeak);
    }

    void driveTone(juce::AudioBuffer<float>& audio)
    {
        const float drive = param("drive") * .1f;
        const float pregain = juce::Decibels::decibelsToGain(30.0f * drive);
        const float compensation = 1.0f / std::sqrt(pregain);
        const float trim = juce::Decibels::decibelsToGain(param("trim"));
        const float mix = param("mix") * .01f;
        const float tone = param("tone") * .01f;
        const float rolloff = pole(std::min(float(rate * .45), 6500 + tone * 4000));
        const float smooth = 1 - decay(10);
        auto samples = audio.getArrayOfWritePointers();
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            driveBlend += smooth * (drive - driveBlend);
            const float amount = mix * std::min(1.0f, driveBlend * 4.0f);
            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            {
                const float dry = samples[ch][i];
                const float shaped = std::tanh(dry * pregain) * compensation;
                lowpass[ch] += rolloff * (shaped - lowpass[ch]);
                samples[ch][i] = trim * (dry + amount * (lowpass[ch] - dry));
            }
        }
    }

    void fetCompress(juce::AudioBuffer<float>& audio)
    {
        const auto gain = [] (float db) { return juce::Decibels::decibelsToGain(db); };
        const float driveDb = param("input");
        const float drive = gain(driveDb), output = gain(param("output")), trim = gain(param("trim"));
        const float mix = param("mix") * .01f;
        constexpr std::array<float, 5> ratios{4, 8, 12, 20, 22};
        const int mode = std::clamp(int(param("ratio")), 0, 4);
        const bool all = mode == 4;
        const bool bright = param("character") > .5f;
        const bool off = param("bypassComp") > .5f;
        const float threshold = all ? -23.f : -18.f + .55f * mode;
        const float slope = 1 - 1 / ratios[mode];
        const float attackMs = .8f * std::pow(.025f, (param("attack") - 1) / 6);
        const float releaseMs = 1100.f * std::pow(50.f / 1100.f, (param("release") - 1) / 6);
        const float attack = decay(attackMs);
        const float release = decay(releaseMs);
        const float slow = decay(releaseMs * 2.8f);
        const float knee = all ? 8.f : bright ? 2.5f : 4.f;
        const float makeup = param("makeup") > .5f && !off
            ? gain(std::max(0.f, (-12 + driveDb - threshold) * slope) * .65f) : 1.f;
        const float saturation = all ? 1.2f : bright ? .56f : .34f;
        const int analog = int(param("analog"));
        const float step = float((analog == 1 ? 50 : 60) / rate);
        const float humLevel = analog == 0 ? 0.f : gain(-77.f);
        auto samples = audio.getArrayOfWritePointers();
        const int channels = audio.getNumChannels();
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            float peak = 0;
            for (int ch = 0; ch < channels; ++ch)
                peak = std::max(peak, std::abs(samples[ch][i] * drive));
            const float over = juce::Decibels::gainToDecibels(peak, -120.f) - threshold;
            float target = slope * std::max(0.f, over);
            if (std::abs(over) < knee * .5f)
            {
                const float x = over + .5f * knee;
                target = slope * x * x / (2 * knee);
            }
            if (off) target = 0;
            const float recovery = release + (slow - release) * std::min(reductionDb / 20, 1.f);
            const float c = target > reductionDb ? attack : recovery;
            reductionDb = target + c * (reductionDb - target);
            const float attenuation = off ? 1.f : gain(-reductionDb);
            if (!off) blockReduction = std::max(blockReduction, reductionDb);
            const float hum = humLevel == 0 ? 0 : humLevel * std::sin(juce::MathConstants<float>::twoPi * humPhase);
            humPhase += step;
            if (humPhase >= 1) humPhase -= 1;
            for (int ch = 0; ch < channels; ++ch)
            {
                const float dry = samples[ch][i];
                const float compressed = dry * drive * attenuation;
                const float wet = (std::tanh(compressed * saturation) / saturation + hum) * makeup * output;
                samples[ch][i] = trim * (dry + mix * (wet - dry));
            }
        }
    }

    State state;
    Activity activity;
    float blockReduction = 0, detectionEnvelope = 0, driveBlend = 0;
    std::array<float, 2> detectorLow{}, detectorHigh{};
    std::array<std::vector<float>, 2> ring;
    std::array<float, 2> lowpass{}, bass{};
    double rate = 44100, delay = 0;
    int lookahead = 0, cursor = 0, hold = 0;
    float reductionDb = 0, limiterGain = 1, humPhase = 0;
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Plugin(); }
