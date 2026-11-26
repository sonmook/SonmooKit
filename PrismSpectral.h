// Weighted overlap-add with an independent envelope for each band and FFT bin.
class PrismSpectral
{
public:
    static constexpr int size = 1024, hop = size / 4, bins = size / 2 + 1;
    using Pair = std::array<float, 2>;
    PrismSpectral()
    {
        for (int i = 0; i < size; ++i)
            window[i] = float(std::sin(pi * (i + .5) / size));
    }
    std::array<float,24> activity{};
    void reset()
    {
        history = {}; sum = {}; dryDelay = {}; envelopes = {}; position = 0;
    }
    template <class Bands>
    Pair process(Pair wet, Pair dry, Pair key, Pair dryOrSolo, double sr, const Bands& bands, Pair& alignedDry)
    {
        Pair out{};
        for (int c = 0; c < 2; ++c)
        {
            alignedDry[c] = dryDelay[c][position];
            dryDelay[c][position] = dryOrSolo[c];
            out[c] = sum[c][position];
            sum[c][position] = 0;
            history[c][position] = wet[c];
            history[c+2][position] = dry[c];
            history[c+4][position] = key[c];
        }
        position = (position + 1) % size;
        if (position % hop == 0) frame(sr, bands);
        return out;
    }
private:
    using Complex = std::complex<float>;
    template <class Bands>
    void frame(double sr, const Bands& bands)
    {
        bool internal = false, external = false;
        for (const auto& b : bands)
            if (b.enabled && b.spectral && b.range != 0)
                (b.external ? external : internal) = true;
        for (int c = 0; c < 6; ++c)
        {
            if ((c >= 4 && !external) || (c >= 2 && c < 4 && !internal)) continue;
            for (int i = 0; i < size; ++i)
                scratch[i] = history[c][(position+i)%size] * window[i];
            fft.perform(scratch.data(), spectra[c].data(), false);
        }
        for (size_t i = 0; i < bands.size(); ++i)
        {
            const auto& b = bands[i];
            auto& env = envelopes[i];
            activity[i]=0;
            if (!b.enabled || !b.spectral || b.range == 0) { env.fill(0); continue; }
            const int source = b.external ? 4 : 2;
            const float attack = std::pow(b.attack, float(hop)/16);
            const float release = std::pow(b.release, float(hop)/16);
            for (int k = 0; k < bins; ++k)
            {
                const auto l = spectra[source][k], r = spectra[source+1][k];
                float magnitude = std::max(std::abs(l), std::abs(r));
                if (b.channel == 1) magnitude = std::abs((l+r)*.5f);
                if (b.channel == 2) magnitude = std::abs((l-r)*.5f);
                if (b.channel == 3) magnitude = std::abs(l);
                if (b.channel == 4) magnitude = std::abs(r);
                const float hz = float(k * sr / size);
                const float distance = float(std::log2(std::max(1.f,hz)/b.hz)) * b.q;
                const float weight = std::exp(-distance*distance);
                const float db = juce::Decibels::gainToDecibels(magnitude * float(pi/size), -120.f);
                const float target = std::copysign(std::min(std::abs(b.range), std::max(0.f, db-b.threshold)), b.range)*weight;
                const float smoothing = std::abs(target)>std::abs(env[k]) ? attack : release;
                env[k] = target + smoothing*(env[k]-target);
                if (std::abs(env[k])>std::abs(activity[i])) activity[i]=env[k];
                const float gain = juce::Decibels::decibelsToGain(env[k]);
                auto& a = spectra[0][k]; auto& d = spectra[1][k];
                if (b.channel == 1 || b.channel == 2)
                {
                    const auto change = (b.channel == 1 ? a+d : a-d) * (.5f*(gain-1));
                    a += change; d += b.channel == 1 ? change : -change;
                }
                else { if (b.channel != 4) a *= gain; if (b.channel != 3) d *= gain; }
            }
        }
        for (int c = 0; c < 2; ++c)
        {
            auto& spec = spectra[c];
            for (int k = 1; k < size/2; ++k) spec[size-k] = std::conj(spec[k]);
            fft.perform(spec.data(), scratch.data(), true);
            for (int i = 0; i < size; ++i)
                sum[c][(position+i)%size] += scratch[i].real()*window[i]*.5f;
        }
    }
    juce::dsp::FFT fft{10};
    int position = 0;
    std::array<float,size> window{};
    std::array<std::array<float,size>,6> history{};
    std::array<std::array<float,size>,2> sum{}, dryDelay{};
    std::array<std::array<float,bins>,24> envelopes{};
    std::array<std::array<Complex,size>,6> spectra{};
    std::array<Complex,size> scratch{};
};
