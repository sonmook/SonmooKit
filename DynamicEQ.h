constexpr bool prism = EFFECT_KIND == 19;
constexpr int dynamicBands = prism ? 24 : 6;
#if EFFECT_KIND == 19
#include "PrismSpectral.h"
#endif
juce::StringArray shapes() {
    juce::StringArray result{"Bell","Low Shelf","High Shelf"};
    if constexpr (prism) result.addArray(juce::StringArray{"High Cut","Low Cut","Notch","Band Pass","All Pass"});
    return result;
}
juce::StringArray placements() {
    juce::StringArray result{"Stereo","Mid","Side"};
    if constexpr (prism) result.addArray(juce::StringArray{"Left","Right"});
    return result;
}
juce::StringArray detectionModes() {
    juce::StringArray result{"Split","Wide"};
    if constexpr (prism) result.add("Spectral");
    return result;
}
constexpr std::array<float, dynamicBands> centreFrequencies{45, 125, 350, 1000, 2800, 8000};
constexpr std::array<const char*, 7> dynamicKeys{"freq", "q", "gain", "range", "threshold", "attack", "release"};

juce::String dynamicId(int band, const char* key)
{
    return "band" + juce::String(band + 1) + "_" + key;
}

juce::StringArray spectrumChoices() {
    juce::StringArray result{"L+R","Left","Right","Side"};
    if constexpr (prism) result.add("Pre L+R");
    return result;
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
        check(dynamicId(i, "enabled"), prefix + "Enabled", !prism);
        n("freq", "Frequency", 16, 21000, .1f, prism ? float(30 * std::pow(600., i/23.)) : centreFrequencies[i], 1000);
        n("q", "Q", .4f, 60, .01f, 1, 3);
        n("gain", "Gain", -18, 18, .1f, 0);
        n("range", "Dynamic Range", -18, 18, .1f, 0);
        n("threshold", "Threshold", -60, 0, .1f, 0);
        n("attack", "Attack", .5f, 500, .1f, 16, 20);
        n("release", "Release", 5, 5000, 1, 160, 180);
        choice(dynamicId(i, "shape"), prefix + "Shape", shapes());
        choice(dynamicId(i, "channel"), prefix + "Channel", placements());
        choice(dynamicId(i, "source"), prefix + "Key Source", {"Internal", "External"});
        choice(dynamicId(i, "detect"), prefix + "Detection", detectionModes());
    }
    check("hp", "High Pass", false);
    check("lp", "Low Pass", false);
    number("hpFreq", "High Pass Frequency", 16, 21000, .1f, 20, 1000);
    number("lpFreq", "Low Pass Frequency", 16, 21000, .1f, 20000, 1000);
    check("autoRelease", "Adaptive Release", true);
    number("mix", "Mix", 0, 100, .1f, 100);
    number("output", "Output", -24, 12, .1f, 0);
    for (auto id : {"hpSlope", "lpSlope"})
        result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, 1},
            juce::String(id) == "hpSlope" ? "High Pass Slope" : "Low Pass Slope",
            juce::StringArray{"6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct"}, 1));
    choice("spectrumSource", "Spectrum Source", spectrumChoices());
    if constexpr (prism) {
        choice("engine","Engine",{"Spectral ready","Live"});
        choice("character","Character",{"Clean","Soft","Dense"});
    }
    return result;
}

struct DynamicDisplay : DisplayData
{
    std::array<std::atomic<float>, dynamicBands> activity{}, detector{};
    std::atomic<int> listenBand{-1};
    std::atomic<bool> stereo{true}, externalAvailable{false};

    void push(float sample)
    {
        if (!listening.load(std::memory_order_relaxed))
        {
            filled = write = hop = 0;
            return;
        }
        history[write] = sample;
        write = (write + 1) % fftSize;
        if (filled < fftSize) ++filled;
        if (++hop < 512) return;
        hop = 0;
        if (filled != fftSize || ready.load(std::memory_order_acquire)) return;

        // The editor owns frame until it clears ready. Keep collecting input meanwhile.
        const auto split = history.begin() + write;
        const auto end = std::copy(split, history.end(), frame.begin());
        std::copy(history.begin(), split, end);
        ready.store(true, std::memory_order_release);
    }

private:
    std::array<float, fftSize> history{};
    int write = 0, filled = 0, hop = 0;
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

std::array<Biquad, 2> cutStages(double rate, double hz, int order, bool highPass)
{
    std::array<Biquad, 2> stages{};
    const double k = std::tan(pi * std::min(hz, rate * .485) / rate);
    if (order % 2 == 1)
    {
        const double b = (highPass ? 1. : k) / (1. + k);
        stages[0] = {b, highPass ? -b : b, 0., (k - 1.) / (k + 1.), 0.};
    }
    for (int i = 0; i < order / 2; ++i)
    {
        const double q = .5 / std::sin(pi * (2 * i + 1) / (2 * order));
        stages[order % 2 + i] = filter(rate, hz, 0, q, highPass ? 4 : 3);
    }
    return stages;
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
        if constexpr (prism) {
            engine = state.getRawParameterValue("engine");
            character = state.getRawParameterValue("character");
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
#if EFFECT_KIND == 19
        setLatencySamples(engine->load() < .5f ? PrismSpectral::size : 0);
#endif
        display.sampleRate.store(rate, std::memory_order_relaxed);
        reset();
    }
    void releaseResources() override {}
    void reset() override
    {
        running = {};
#if EFFECT_KIND == 19
        spectral.reset();
#endif
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
        const int spectrumSource = int(g(9));
#if EFFECT_KIND == 19
        const bool spectralReady = engine->load() < .5f;
        const int latency = spectralReady ? PrismSpectral::size : 0;
        if (latency != getLatencySamples()) { spectral.reset(); setLatencySamples(latency); }
        const int colourMode = int(character->load());
#endif
        const auto hp = cutStages(rate, g(4), g(3) > .5f ? int(g(7)) + 1 : 0, true);
        const auto lp = cutStages(rate, g(6), g(5) > .5f ? int(g(8)) + 1 : 0, false);
        std::array<Config, dynamicBands> bands;
        for (int i = 0; i < dynamicBands; ++i)
        {
            auto& cfg = bands[i];
            const auto& p = controls[i];
            const auto v = [&] (int j) { return Parameters::get(p.knobs[j]); };
            cfg = {v(0), v(1), v(2), v(3), v(4), coefficient(v(5)), coefficient(v(6)),
                   int(Parameters::get(p.shape)), stereo ? int(Parameters::get(p.channel)) : 0,
                   Parameters::get(p.enabled) > .5f, Parameters::get(p.source) > .5f,
                   int(Parameters::get(p.detect)) == 1, int(Parameters::get(p.detect)) == 2, {}};
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
                if constexpr (prism) if (!cfg.enabled && i != solo) continue;
                auto key = cfg.external ? external : raw;
                if (cfg.channel == 3) key[1] = 0;
                if (cfg.channel == 4) key[0] = 0;
                if (cfg.channel == 1) key = { (key[0] + key[1]) * .5f, 0 };
                if (cfg.channel == 2) key = { (key[0] - key[1]) * .5f, 0 };
                for (int ch = 0; ch < 2; ++ch) key[ch] = rt.key[ch].process(key[ch], cfg.key);
                const float value = std::max(std::abs(key[0]), std::abs(key[1]));
                const float hold = value >= rt.envelope ? 0.f : detectorFall;
                rt.envelope = value + hold * (rt.envelope - value);
                if (tick == 0 || sample == 0)
                {
                    const float db = juce::Decibels::gainToDecibels(rt.envelope, -100.f);
                    const float target = cfg.enabled && !cfg.spectral && cfg.shape < 3
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
                if constexpr (prism) if (!cfg.enabled) continue;
                if (cfg.channel >= 3)
                {
                    const int c = cfg.channel - 3;
                    wet[c] = rt.audio[c].process(wet[c], rt.filter);
                }
                else if (cfg.channel == 0)
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
            auto aligned = raw;
#if EFFECT_KIND == 19
            if (spectralReady) {
                wet = spectral.process(wet, raw, external, solo >= 0 ? keyListen : raw, rate, bands, aligned);
                if (solo >= 0) keyListen = aligned;
            }
#endif
            for (int ch = 0; ch < channels; ++ch)
            {
                float shaped = wet[ch];
                for (int s = 0; s < 2; ++s) shaped = passes[ch][s].process(shaped, hp[s]);
                for (int s = 0; s < 2; ++s) shaped = passes[ch][2+s].process(shaped, lp[s]);
                if constexpr (prism) {
#if EFFECT_KIND == 19
                    if (colourMode != 0) {
                        const float drive = colourMode == 1 ? 1.25f : 2.5f;
                        shaped = std::tanh(drive*shaped)/drive;
                    }
#endif
                }
                const float result = output * (solo >= 0 ? keyListen[ch] : aligned[ch] + mix * (shaped - aligned[ch]));
                dst[ch][sample] = result;
                peaks[ch] = std::max(peaks[ch], std::abs(result));
            }
            const float left = dst[0][sample], right = stereo ? dst[1][sample] : left;
            const std::array<float, 4> spectrumInputs{.5f * (left + right), left, right, .5f * (left - right)};
            display.push(spectrumSource == 4 ? .5f*(raw[0]+raw[1]) : spectrumInputs[spectrumSource]);
            tick = (tick + 1) % 16;
        }

        for (int i = 0; i < dynamicBands; ++i)
        {
            float movement = running[i].gain;
#if EFFECT_KIND == 19
            if (bands[i].spectral) movement = spectralReady ? spectral.activity[i] : 0;
#endif
            display.activity[i].store(movement, std::memory_order_relaxed);
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
        bool enabled, external, wide, spectral;
        Biquad key;
    };
    struct Running
    {
        std::array<BiquadMemory, 2> key{}, audio{};
        float envelope = 0, gain = 0, lastGain = -100;
        Biquad filter;
    };
    float coefficient(float ms) const { return float(std::exp(-16000. / (rate * ms))); }

    static constexpr std::array<const char*, 10> globalKeys{"mix", "output", "autoRelease", "hp", "hpFreq", "lp", "lpFreq", "hpSlope", "lpSlope", "spectrumSource"};
#if EFFECT_KIND == 19
    PrismSpectral spectral;
#endif
    std::atomic<float>* engine{}; std::atomic<float>* character{};
    State state;
    DynamicDisplay display;
    std::array<ParametersByBand, dynamicBands> controls{};
    std::array<std::atomic<float>*, 10> globals{};
    std::array<Running, dynamicBands> running{};
    std::array<std::array<BiquadMemory, 4>, 2> passes{};
    double rate = 48000;
    int tick = 0;
};
