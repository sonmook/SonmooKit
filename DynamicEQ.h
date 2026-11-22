constexpr int dynamicBands = 6;
constexpr std::array<float, dynamicBands> centreFrequencies{45, 125, 350, 1000, 2800, 8000};
constexpr std::array<const char*, 7> dynamicKeys{"freq", "q", "gain", "range", "threshold", "attack", "release"};

juce::String dynamicId(int band, const char* key)
{
    return "band" + juce::String(band + 1) + "_" + key;
}

State::ParameterLayout dynamicLayout()
{
    State::ParameterLayout result;
    const auto number = [&] (juce::String key, juce::String label, float lo, float hi,
                            float step, float value, float centre = 0)
    {
        juce::NormalisableRange<float> limits(lo, hi, step);
        if (centre != 0) limits.setSkewForCentre(centre);
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{key, 1}, label, limits, value));
    };
    const auto check = [&] (juce::String key, juce::String label, bool value)
    {
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{key, 1}, label, value));
    };
    const auto choice = [&] (juce::String key, juce::String label, juce::StringArray items)
    {
        result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{key, 1}, label, items, 0));
    };
    for (int i = 0; i < dynamicBands; ++i)
    {
        const auto prefix = "Band " + juce::String(i + 1) + " ";
        const auto n = [&] (const char* key, const char* label, float lo, float hi,
                            float step, float value, float centre = 0)
        {
            number(dynamicId(i, key), prefix + label, lo, hi, step, value, centre);
        };
        check(dynamicId(i, "enabled"), prefix + "Enabled", true);
        n("freq", "Frequency", 16, 21000, .1f, centreFrequencies[i], 1000);
        n("q", "Q", .4f, 60, .01f, 1, 3);
        n("gain", "Gain", -18, 18, .1f, 0);
        n("range", "Dynamic Range", -18, 18, .1f, 0);
        n("threshold", "Threshold", -60, 0, .1f, 0);
        n("attack", "Attack", .5f, 500, .1f, 16, 20);
        n("release", "Release", 5, 5000, 1, 160, 180);
        choice(dynamicId(i, "shape"), prefix + "Shape", {"Bell", "Low Shelf", "High Shelf"});
        choice(dynamicId(i, "channel"), prefix + "Channel", {"Stereo", "Mid", "Side"});
        choice(dynamicId(i, "source"), prefix + "Key Source", {"Internal", "External"});
        choice(dynamicId(i, "detect"), prefix + "Detection", {"Split", "Wide"});
    }
    check("hp", "High Pass", false);
    check("lp", "Low Pass", false);
    number("hpFreq", "High Pass Frequency", 16, 21000, .1f, 20, 1000);
    number("lpFreq", "Low Pass Frequency", 16, 21000, .1f, 20000, 1000);
    check("autoRelease", "Adaptive Release", true);
    number("mix", "Mix", 0, 100, .1f, 100);
    number("output", "Output", -24, 12, .1f, 0);
    return result;
}

struct DynamicDisplay : DisplayData
{
    std::array<std::atomic<float>, dynamicBands> activity{}, detector{};
    std::atomic<int> listenBand{-1};
    std::atomic<bool> stereo{true}, externalAvailable{false};
};

struct BiquadMemory
{
    double z1 = 0, z2 = 0;
    float process(float x, const Biquad& c)
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return float(y);
    }
};

float dynamicValue(State& state, const juce::String& key)
{
    return Parameters::get(state.getRawParameterValue(key));
}

Biquad keyFilter(double rate, double hz, double q, int shape)
{
    if (shape == 1 || shape == 2)
        return filter(rate, hz, 0, .7071, shape == 1 ? 3 : 4);
    const double w = 2 * pi * std::min(hz, rate * .485) / rate;
    const double a = std::sin(w) / (2 * q);
    return {a / (1 + a), 0, -a / (1 + a), -2 * std::cos(w) / (1 + a), (1 - a) / (1 + a)};
}

#include "DynamicEditor.h"

class Processor final : public juce::AudioProcessor
{
public:
    Processor() : AudioProcessor(BusesProperties()
                    .withInput("Input", juce::AudioChannelSet::stereo(), true)
                    .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                    .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
                  state(*this, nullptr, "PARAMETERS", dynamicLayout())
    {
        for (int i = 0; i < dynamicBands; ++i)
        {
            auto& p = controls[i];
            const auto get = [&] (const char* key) { return state.getRawParameterValue(dynamicId(i, key)); };
            for (size_t j = 0; j < dynamicKeys.size(); ++j) p.knobs[j] = get(dynamicKeys[j]);
            p.enabled = get("enabled"); p.shape = get("shape"); p.channel = get("channel");
            p.source = get("source"); p.detect = get("detect");
        }
        for (size_t i = 0; i < globals.size(); ++i)
            globals[i] = state.getRawParameterValue(globalKeys[i]);
    }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return .5; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new DynamicEditor(*this, state, display); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout& buses) const override
    {
        using Set = juce::AudioChannelSet;
        const auto input = buses.getMainInputChannelSet(), output = buses.getMainOutputChannelSet();
        const auto sc = buses.getChannelSet(true, 1);
        return input == output && (input == Set::mono() || input == Set::stereo()) &&
               (sc.isDisabled() || sc == Set::mono() || sc == Set::stereo());
    }

    void prepareToPlay(double sr, int) override
    {
        rate = std::max(1., sr);
        display.sampleRate.store(rate, std::memory_order_relaxed);
        reset();
    }
    void releaseResources() override {}
    void reset() override
    {
        running = {};
        passes = {};
        tick = 0;
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals guard;
        auto audio = getBusBuffer(buffer, false, 0);
        const auto side = getBusBuffer(buffer, true, 1);
        const int channels = audio.getNumChannels();
        const int sideChannels = side.getNumChannels();
        const bool stereo = channels == 2;
        display.stereo.store(stereo, std::memory_order_relaxed);
        display.externalAvailable.store(sideChannels > 0, std::memory_order_relaxed);
        const auto g = [&] (int i) { return Parameters::get(globals[i]); };
        const float mix = g(0) * .01f;
        const float output = juce::Decibels::decibelsToGain(g(1));
        const bool adaptive = g(2) > .5f;
        const std::array<Biquad, 2> cut{
            g(3) > .5f ? filter(rate, g(4), 0, .7071, 4) : Biquad{},
            g(5) > .5f ? filter(rate, g(6), 0, .7071, 3) : Biquad{}};
        std::array<Config, dynamicBands> bands;
        for (int i = 0; i < dynamicBands; ++i)
        {
            auto& cfg = bands[i];
            const auto& p = controls[i];
            const auto v = [&] (int j) { return Parameters::get(p.knobs[j]); };
            cfg = {v(0), v(1), v(2), v(3), v(4), coefficient(v(5)), coefficient(v(6)),
                   int(Parameters::get(p.shape)), stereo ? int(Parameters::get(p.channel)) : 0,
                   Parameters::get(p.enabled) > .5f, Parameters::get(p.source) > .5f,
                   Parameters::get(p.detect) > .5f, {}};
            if (!cfg.wide) cfg.key = keyFilter(rate, cfg.hz, cfg.q, cfg.shape);
        }

        const float detectorFall = float(std::exp(-1. / (rate * .010)));
        const int solo = display.listenBand.load(std::memory_order_relaxed);
        std::array<float, 2> peaks{};
        auto dst = audio.getArrayOfWritePointers();
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const std::array<float, 2> raw{dst[0][sample], stereo ? dst[1][sample] : dst[0][sample]};
            const float sideL = sideChannels ? side.getSample(0, sample) : 0;
            const std::array<float, 2> external{sideL, sideChannels > 1 ? side.getSample(1, sample) : sideL};
            std::array<float, 2> wet = raw, keyListen{};
            for (int i = 0; i < dynamicBands; ++i)
            {
                auto& rt = running[i];
                const auto& cfg = bands[i];
                auto key = cfg.external ? external : raw;
                if (cfg.channel == 1) key = { (key[0] + key[1]) * .5f, 0 };
                if (cfg.channel == 2) key = { (key[0] - key[1]) * .5f, 0 };
                for (int ch = 0; ch < 2; ++ch) key[ch] = rt.key[ch].process(key[ch], cfg.key);
                const float value = std::max(std::abs(key[0]), std::abs(key[1]));
                const float hold = value >= rt.envelope ? 0.f : detectorFall;
                rt.envelope = value + hold * (rt.envelope - value);
                if (tick == 0 || sample == 0)
                {
                    const float db = juce::Decibels::gainToDecibels(rt.envelope, -100.f);
                    const float target = cfg.enabled
                        ? std::copysign(std::clamp((db - cfg.threshold) * .8f, 0.f, std::abs(cfg.range)), cfg.range)
                        : 0;
                    const float release = adaptive ? std::pow(cfg.release, 1.f / (1.f + std::abs(rt.gain) / 12.f))
                                                   : cfg.release;
                    const float smoothing = std::abs(target) > std::abs(rt.gain) ? cfg.attack : release;
                    if (tick == 0) rt.gain = target + smoothing * (rt.gain - target);
                    const float effective = cfg.enabled ? std::clamp(cfg.staticGain + rt.gain, -36.f, 36.f) : 0.f;
                    if (std::abs(rt.lastGain - effective) > .015f || sample == 0)
                    {
                        rt.lastGain = effective;
                        rt.filter = filter(rate, cfg.hz, effective, cfg.q, cfg.shape);
                    }
                }
                if (i == solo)
                    keyListen = cfg.channel == 2 ? std::array<float, 2>{key[0], -key[0]}
                              : cfg.channel == 1 ? std::array<float, 2>{key[0], key[0]} : key;
                if (cfg.channel == 0)
                {
                    for (int ch = 0; ch < channels; ++ch) wet[ch] = rt.audio[ch].process(wet[ch], rt.filter);
                }
                else
                {
                    float mid = (wet[0] + wet[1]) * .5f;
                    float sideSample = (wet[0] - wet[1]) * .5f;
                    if (cfg.channel == 1) mid = rt.audio[0].process(mid, rt.filter);
                    else sideSample = rt.audio[0].process(sideSample, rt.filter);
                    wet = {mid + sideSample, mid - sideSample};
                }
            }
            for (int ch = 0; ch < channels; ++ch)
            {
                const float shaped = passes[ch][1].process(passes[ch][0].process(wet[ch], cut[0]), cut[1]);
                const float result = output * (solo >= 0 ? keyListen[ch] : raw[ch] + mix * (shaped - raw[ch]));
                dst[ch][sample] = result;
                peaks[ch] = std::max(peaks[ch], std::abs(result));
            }
            const float left = dst[0][sample], right = stereo ? dst[1][sample] : left;
            display.push(std::abs(left) >= std::abs(right) ? left : right);
            tick = (tick + 1) % 16;
        }

        for (int i = 0; i < dynamicBands; ++i)
        {
            display.activity[i].store(running[i].gain, std::memory_order_relaxed);
            display.detector[i].store(running[i].envelope, std::memory_order_relaxed);
        }
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& meter = display.channelPeak[ch];
            float previous = meter.load(std::memory_order_relaxed);
            while (peaks[ch] > previous && !meter.compare_exchange_weak(previous, peaks[ch], std::memory_order_relaxed)) {}
        }
    }

    void getStateInformation(juce::MemoryBlock& data) override
    {
        if (const auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, data);
    }
    void setStateInformation(const void* data, int size) override
    {
        if (const auto xml = getXmlFromBinary(data, size))
            if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    struct ParametersByBand
    {
        std::array<std::atomic<float>*, 7> knobs{};
        std::atomic<float> *enabled{}, *shape{}, *channel{}, *source{}, *detect{};
    };
    struct Config
    {
        float hz, q, staticGain, range, threshold, attack, release;
        int shape, channel;
        bool enabled, external, wide;
        Biquad key;
    };
    struct Running
    {
        std::array<BiquadMemory, 2> key{}, audio{};
        float envelope = 0, gain = 0, lastGain = -100;
        Biquad filter;
    };
    float coefficient(float ms) const { return float(std::exp(-16000. / (rate * ms))); }

    static constexpr std::array<const char*, 7> globalKeys{"mix", "output", "autoRelease", "hp", "hpFreq", "lp", "lpFreq"};
    State state;
    DynamicDisplay display;
    std::array<ParametersByBand, dynamicBands> controls{};
    std::array<std::atomic<float>*, 7> globals{};
    std::array<Running, dynamicBands> running{};
    std::array<std::array<BiquadMemory, 2>, 2> passes{};
    double rate = 48000;
    int tick = 0;
};
