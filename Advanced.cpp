#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>

namespace {
constexpr bool isEqualiser = EFFECT_KIND == 5;
constexpr int count = isEqualiser ? 10 : 4;
constexpr int fftOrder = 12;
constexpr int fftSize = 1 << fftOrder;
constexpr double pi = juce::MathConstants<double>::pi;
const juce::Colour background(0xff121828), surface(0xff1c2940), line(0xff394c69),
                   ink(0xfff5f7ff), dim(0xffaebed8);
const std::array<juce::Colour, 10> colours{
    juce::Colour(0xff54e2cf), juce::Colour(0xfff1a6b5), juce::Colour(0xffefc47d),
    juce::Colour(0xffbaacfe), juce::Colour(0xff9ce7aa), juce::Colour(0xff8dc5ff),
    juce::Colour(0xffe8aaff), juce::Colour(0xfff2ad81), juce::Colour(0xfff1e9a2),
    juce::Colour(0xffa8dce6)};
using State = juce::AudioProcessorValueTreeState;

juce::String id(int i, const char* field)
{
    return (isEqualiser ? juce::String("band") : juce::String("voice")) + juce::String(i + 1) + "_" + field;
}

struct Parameters
{
    std::atomic<float>* on{};
    std::atomic<float>* a{};
    std::atomic<float>* b{};
    std::atomic<float>* c{};
    std::atomic<float>* mode{};
    std::atomic<float>* feedback{};
    std::atomic<float>* depth{};
    std::atomic<float>* speed{};

    static float get(const std::atomic<float>* parameter)
    {
        return parameter->load(std::memory_order_relaxed);
    }
};

State::ParameterLayout layout()
{
    State::ParameterLayout result;
    const auto add = [&] (const juce::String& key, const juce::String& label,
                          float lo, float hi, float step, float value, float centre = 0)
    {
        juce::NormalisableRange<float> range(lo, hi, step);
        if (centre != 0) range.setSkewForCentre(centre);
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{key, 1}, label, range, value));
    };
    const auto toggle = [&] (const juce::String& key, const juce::String& label, bool value)
    {
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{key, 1}, label, value));
    };
    for (int i = 0; i < count; ++i)
    {
        const auto prefix = juce::String(isEqualiser ? "Band " : "Voice ") + juce::String(i + 1) + " ";
        toggle(id(i, "on"), prefix + "Enabled", isEqualiser || i < 2);
        if constexpr (isEqualiser)
        {
            constexpr std::array<float, 10> centres{31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
            add(id(i, "freq"), prefix + "Frequency", 16, 21357, 0.1f, centres[i], 1000);
            add(id(i, "gain"), prefix + "Gain", -18, 18, 0.1f, 0);
            add(id(i, "q"), prefix + "Q", .5f, 100, .01f, 1, 2);
            result.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{id(i, "type"), 1}, prefix + "Type",
                juce::StringArray{"Bell", "Low Shelf", "High Shelf", "Low Pass", "High Pass", "Proportional Q"}, 0));
        }
        else
        {
            constexpr std::array<float, 4> delays{9.4f, 16, 23.7f, 21};
            add(id(i, "delay"), prefix + "Delay", 0, 100, .1f, delays[i]);
            add(id(i, "detune"), prefix + "Detune", -100, 100, 1, i % 2 == 0 ? -6 : 6);
            add(id(i, "pan"), prefix + "Pan", -45, 45, 1, i % 2 == 0 ? -45 : 45);
            add(id(i, "gain"), prefix + "Gain", -48, 0, .1f, i < 2 ? -9 : -12);
            add(id(i, "feedback"), prefix + "Feedback", 0, 95, .1f, 0);
            add(id(i, "depth"), prefix + "Mod Depth", -200, 200, 1, 0);
            add(id(i, "rate"), prefix + "Mod Rate", .1f, 400, .1f, 1, 3);
            toggle(id(i, "octave"), prefix + "Octave Down", false);
        }
    }
    add("output", "Output Gain", -24, 12, .1f, 0);
    if constexpr (!isEqualiser)
    {
        toggle("direct", "Direct Signal", true);
        toggle("align", "Align Direct", false);
        toggle("lowRange", "Extended Pitch Range", false);
        toggle("lowShelfOn", "Low Shelf Enabled", true);
        toggle("highShelfOn", "High Shelf Enabled", true);
        add("lowShelfGain", "Low Shelf Gain", -24, 12, .1f, 0);
        add("lowShelfFreq", "Low Shelf Frequency", 16, 21357, 1, 101, 1000);
        add("highShelfGain", "High Shelf Gain", -24, 12, .1f, 0);
        add("highShelfFreq", "High Shelf Frequency", 16, 21357, 1, 1306, 1000);
        add("directGain", "Direct Gain", -48, 0, .1f, 0);
        add("directPan", "Direct Pan", -45, 45, 1, 0);
        add("fbCut", "Feedback High Pass", 16, 4000, 1, 80, 250);
        add("wetLow", "Voice Low Cut", 16, 1000, 1, 70, 150);
        add("wetHigh", "Voice High Cut", 1000, 21357, 1, 15000, 8000);
    }
    return result;
}

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

Biquad filter(double rate, double f, double gain, double q, int type)
{
    const double w = 2 * pi * std::clamp(f, 1.0, rate * .485) / rate;
    const double s = std::sin(w), c = std::cos(w);
    const double a = std::pow(10., gain / 40);
    if (type == 5) q *= 1 + std::abs(gain) / 12;
    const double alpha = s / (2 * q);
    double b0, b1, b2, a0, a1, a2;
    if (type == 1 || type == 2)
    {
        const double k = std::sqrt(2 * a) * s;
        if (type == 1)
        {
            b0 = a * ((a + 1) - (a - 1) * c + k);
            b1 = 2 * a * ((a - 1) - (a + 1) * c);
            b2 = a * ((a + 1) - (a - 1) * c - k);
            a0 = (a + 1) + (a - 1) * c + k;
            a1 = -2 * ((a - 1) + (a + 1) * c);
            a2 = (a + 1) + (a - 1) * c - k;
        }
        else
        {
            b0 = a * ((a + 1) + (a - 1) * c + k);
            b1 = -2 * a * ((a - 1) + (a + 1) * c);
            b2 = a * ((a + 1) + (a - 1) * c - k);
            a0 = (a + 1) - (a - 1) * c + k;
            a1 = 2 * ((a - 1) - (a + 1) * c);
            a2 = (a + 1) - (a - 1) * c - k;
        }
    }
    else if (type == 3 || type == 4)
    {
        const bool low = type == 3;
        b0 = (1 + (low ? -c : c)) * .5;
        b1 = (low ? 2 : -2) * b0;
        b2 = b0;
        a0 = 1 + alpha;
        a1 = -2 * c;
        a2 = 1 - alpha;
    }
    else
    {
        b0 = 1 + alpha * a;
        b1 = -2 * c;
        b2 = 1 - alpha * a;
        a0 = 1 + alpha / a;
        a1 = b1;
        a2 = 1 - alpha / a;
    }
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

struct Voice
{
    std::vector<float> samples;
    double grainPhase = 0, lfoPhase = 0;
    float feedback = 0, feedbackLow = 0, wetLow = 0, wetHigh = 0;
};

struct DisplayData
{
    std::array<float, fftSize> frame{};
    std::atomic<bool> ready{false}, listening{false}, resetModulation{false};
    std::atomic<double> sampleRate{48000};
    std::atomic<float> peak{0};
    int received = 0;

    void push(float sample)
    {
        if (!listening.load(std::memory_order_relaxed) || ready.load(std::memory_order_acquire)) return;
        frame[static_cast<size_t>(received++)] = sample;
        if (received == fftSize)
        {
            received = 0;
            ready.store(true, std::memory_order_release);
        }
    }
};

#include "VoiceEditor.h"

class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    Editor(juce::AudioProcessor& processor, State& stateToEdit, DisplayData& displayData)
        : AudioProcessorEditor(processor), state(stateToEdit), display(displayData)
    {
        for (int i = 0; i < count; ++i)
        {
            tabs[i].setButtonText(juce::String(i + 1));
            tabs[i].setColour(juce::TextButton::buttonColourId, surface);
            tabs[i].setColour(juce::TextButton::buttonOnColourId, colours[i].darker(.5f));
            tabs[i].onClick = [this, i] { select(i); };
            addAndMakeVisible(tabs[i]);
        }
        addAndMakeVisible(enabled);
        if constexpr (isEqualiser)
        {
            type.addItemList({"Bell", "Low Shelf", "High Shelf", "Low Pass", "High Pass", "Proportional Q"}, 1);
            addAndMakeVisible(type);
            global("output", "Output", " dB");
        }
        else
        {
            addAndMakeVisible(octave);
            direct.setButtonText("Direct");
            addAndMakeVisible(direct);
            directAttachment = std::make_unique<State::ButtonAttachment>(state, "direct", direct);
            global("directGain", "Direct Gain", " dB");
            global("directPan", "Direct Pan", " deg");
            global("fbCut", "FB High Pass", " Hz");
            global("wetLow", "Voice Low Cut", " Hz");
            global("wetHigh", "Voice High Cut", " Hz");
            global("output", "Output", " dB");
        }
        select(0);
        setSize(720, isEqualiser ? 478 : 553);
        spectrum.fill(-100);
        if constexpr (isEqualiser) display.listening.store(true, std::memory_order_relaxed);
        startTimerHz(24);
    }

    ~Editor() override
    {
        stopTimer();
        finishDrag();
        if constexpr (isEqualiser) display.listening.store(false, std::memory_order_relaxed);
    }

    void paint(juce::Graphics& g) override
    {
        g.setGradientFill({surface, 0, 0, background, 0, float(getHeight()), false});
        g.fillAll();
        g.setColour(colours[0]);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("SonmooKit", 24, 11, 180, 20, juce::Justification::centredLeft);
        g.setColour(ink);
        g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
        g.drawText(getAudioProcessor()->getName(), 24, 28, 385, 38, juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(12.0f));
        const auto db = juce::Decibels::gainToDecibels(heldPeak, -90.0f);
        const auto text = db <= -90 ? juce::String("OUT  -inf") : "OUT  " + juce::String(db, 1) + " dBFS";
        g.drawText(text, getWidth() - 179, 35, 151, 25, juce::Justification::centredRight);
        g.setColour(juce::Colour(0xff0d1423));
        g.fillRoundedRectangle(plotBounds().expanded(11), 9);
        if constexpr (isEqualiser) drawEqualiser(g);
        else drawVoices(g);
    }

    void resized() override
    {
        for (int i = 0; i < count; ++i) tabs[i].setBounds(24 + i * 42, 343, 36, 25);
        enabled.setBounds(451, 343, 111, 25);
        if constexpr (isEqualiser) type.setBounds(562, 343, 134, 25);
        else
        {
            octave.setBounds(559, 343, 139, 25);
            direct.setBounds(24, 507, 63, 23);
        }
        const int width = isEqualiser ? 133 : 94;
        for (size_t i = 0; i < fields.size(); ++i)
        {
            place(*fields[i], 24 + int(i) * width, 388, width - 10);
        }
        if constexpr (isEqualiser)
            place(*globals.front(), 24 + int(fields.size()) * width, 388, width - 10);
        else
            for (size_t i = 0; i < globals.size(); ++i)
                place(*globals[i], 99 + int(i) * 99, 483, 91);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (!plotBounds().contains(event.position)) return;
        float closest = 22;
        int target = -1;
        for (int i = 0; i < count; ++i)
        {
            const float distance = node(i).getDistanceFrom(event.position);
            if (distance < closest) { closest = distance; target = i; }
        }
        if (target < 0) return;
        select(target);
        dragX = state.getParameter(id(target, isEqualiser ? "freq" : "delay"));
        dragY = state.getParameter(id(target, isEqualiser ? "gain" : "pan"));
        dragX->beginChangeGesture();
        dragY->beginChangeGesture();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!dragX) return;
        const auto area = plotBounds();
        const float x = std::clamp((event.position.x - area.getX()) / area.getWidth(), 0.f, 1.f);
        const float y = std::clamp((event.position.y - area.getY()) / area.getHeight(), 0.f, 1.f);
        const float a = isEqualiser ? frequency(x) : x * 100;
        const float b = isEqualiser ? 18 - 36 * y : 45 - 90 * y;
        dragX->setValueNotifyingHost(dragX->convertTo0to1(a));
        dragY->setValueNotifyingHost(dragY->convertTo0to1(b));
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { finishDrag(); }

private:
    struct Field
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<State::SliderAttachment> attachment;
    };

    void finishDrag()
    {
        if (dragX)
        {
            dragX->endChangeGesture();
            dragY->endChangeGesture();
            dragX = dragY = nullptr;
        }
    }

    void place(Field& field, int x, int y, int w)
    {
        field.label.setBounds(x, y, w, 21);
        field.slider.setBounds(x, y + 23, w, 30);
    }

    std::unique_ptr<Field> control(const juce::String& key, const char* label, const char* unit)
    {
        auto field = std::make_unique<Field>();
        field->label.setText(label, juce::dontSendNotification);
        field->label.setColour(juce::Label::textColourId, dim);
        field->slider.setSliderStyle(juce::Slider::LinearHorizontal);
        field->slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 80, 20);
        field->slider.setTitle(label);
        field->slider.setColour(juce::Slider::trackColourId, colours[selected]);
        field->slider.setColour(juce::Slider::thumbColourId, ink);
        field->slider.setColour(juce::Slider::textBoxTextColourId, ink);
        field->slider.setColour(juce::Slider::textBoxBackgroundColourId, surface);
        field->slider.setColour(juce::Slider::textBoxOutlineColourId, line);
        addAndMakeVisible(field->label);
        addAndMakeVisible(field->slider);
        field->attachment = std::make_unique<State::SliderAttachment>(state, key, field->slider);
        field->slider.textFromValueFunction = [unit = juce::String(unit)] (double value)
        {
            return juce::String(value, unit == " Hz" && value >= 100 ? 0 : 1) + unit;
        };
        field->slider.valueFromTextFunction = [] (const juce::String& value) { return value.getDoubleValue(); };
        field->slider.updateText();
        return field;
    }

    void global(const char* key, const char* label, const char* unit)
    {
        globals.push_back(control(key, label, unit));
    }

    void select(int i)
    {
        selected = i;
        for (int j = 0; j < count; ++j) tabs[j].setToggleState(i == j, juce::dontSendNotification);
        fields.clear();
        onAttachment.reset();
        typeAttachment.reset();
        octaveAttachment.reset();
        enabled.setButtonText((isEqualiser ? juce::String("Band ") : juce::String("Voice ")) + juce::String(i + 1));
        onAttachment = std::make_unique<State::ButtonAttachment>(state, id(i, "on"), enabled);
        const auto add = [&] (const char* key, const char* label, const char* unit)
        {
            fields.push_back(control(id(i, key), label, unit));
        };
        if constexpr (isEqualiser)
        {
            add("freq", "Frequency", " Hz");
            add("gain", "Gain", " dB");
            add("q", "Q", "");
            typeAttachment = std::make_unique<State::ComboBoxAttachment>(state, id(i, "type"), type);
        }
        else
        {
            add("delay", "Delay", " ms");
            add("detune", "Detune", " ct");
            add("pan", "Pan", " deg");
            add("gain", "Gain", " dB");
            add("feedback", "Feedback", " %");
            add("depth", "Mod Depth", " ct");
            add("rate", "Mod Rate", " Hz");
            octave.setButtonText("Octave Down");
            octaveAttachment = std::make_unique<State::ButtonAttachment>(state, id(i, "octave"), octave);
        }
        resized();
        repaint();
    }

    float value(int i, const char* field) const { return Parameters::get(state.getRawParameterValue(id(i, field))); }
    static float frequency(float fraction) { return 16 * std::pow(21357.0f / 16, fraction); }
    static float fraction(float hz) { return std::log(hz / 16) / std::log(21357.0f / 16); }
    juce::Rectangle<float> plotBounds() const { return {38, 91, 644, 222}; }

    juce::Point<float> node(int i) const
    {
        const auto area = plotBounds();
        const float x = isEqualiser ? fraction(value(i, "freq")) : value(i, "delay") / 100;
        const float y = isEqualiser ? .5f - value(i, "gain") / 36 : .5f - value(i, "pan") / 90;
        return {area.getX() + x * area.getWidth(), area.getY() + y * area.getHeight()};
    }

    void grid(juce::Graphics& g, float x, const juce::String& label)
    {
        const auto area = plotBounds();
        g.setColour(line);
        g.drawVerticalLine(int(x), area.getY(), area.getBottom());
        g.setColour(dim);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(label, int(x) - 18, int(area.getBottom()) + 1, 36, 13, juce::Justification::centred);
    }

    void drawNodes(juce::Graphics& g)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto point = node(i);
            const auto tint = colours[i].withAlpha(value(i, "on") > .5f ? 1.f : .35f);
            const float radius = i == selected ? 10.0f : 8.0f;
            g.setColour(tint);
            g.fillEllipse(point.x - radius, point.y - radius, radius * 2, radius * 2);
            g.setColour(background);
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(juce::String(i + 1), int(point.x) - 10, int(point.y) - 9, 20, 18, juce::Justification::centred);
        }
    }

    void drawEqualiser(juce::Graphics& g)
    {
        const auto area = plotBounds();
        for (const float hz : {31.f, 63.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f})
        {
            const auto label = hz >= 1000 ? juce::String(int(hz / 1000)) + "k" : juce::String(int(hz));
            grid(g, area.getX() + fraction(hz) * area.getWidth(), label);
        }
        g.setColour(line);
        for (int i = 1; i < 6; ++i)
            g.drawHorizontalLine(int(area.getY() + i * area.getHeight() / 6), area.getX(), area.getRight());
        const double rate = display.sampleRate.load(std::memory_order_relaxed);
        std::array<Biquad, count> filters{};
        for (int i = 0; i < count; ++i)
            if (value(i, "on") > .5f)
                filters[i] = filter(rate, value(i, "freq"), value(i, "gain"), value(i, "q"), int(value(i, "type")));
        juce::Path curve, trace;
        for (int i = 0; i <= 320; ++i)
        {
            const float offset = float(i) / 320;
            const float hz = frequency(offset);
            const float x = area.getX() + offset * area.getWidth();
            const auto z = std::polar(1.0, -2 * pi * std::min(double(hz), rate * .499) / rate);
            const auto z2 = z * z;
            double db = 0;
            for (const auto& f : filters)
                db += 20 * std::log10(std::max(1e-10, std::abs((f.b0 + f.b1 * z + f.b2 * z2) /
                                                                  (1.0 + f.a1 * z + f.a2 * z2))));
            const float y = area.getY() + area.getHeight() * std::clamp(.5f - float(db) / 36, 0.f, 1.f);
            const int bin = std::clamp(juce::roundToInt(hz * fftSize / rate), 1, fftSize / 2);
            const float sy = area.getBottom() - area.getHeight() * std::clamp((spectrum[bin] + 100) / 100, 0.f, 1.f);
            if (i == 0) { curve.startNewSubPath(x, y); trace.startNewSubPath(x, sy); }
            else { curve.lineTo(x, y); trace.lineTo(x, sy); }
        }
        g.setColour(dim.withAlpha(.5f));
        g.strokePath(trace, juce::PathStrokeType(1));
        g.setColour(colours[selected]);
        g.strokePath(curve, juce::PathStrokeType(2));
        drawNodes(g);
    }

    void drawVoices(juce::Graphics& g)
    {
        const auto area = plotBounds();
        for (int ms = 0; ms <= 100; ms += 20)
            grid(g, area.getX() + ms * area.getWidth() / 100, juce::String(ms) + " ms");
        g.setColour(line);
        g.drawHorizontalLine(int(area.getCentreY()), area.getX(), area.getRight());
        g.setColour(dim);
        g.setFont(juce::FontOptions(11.f, juce::Font::bold));
        g.drawText("RIGHT", int(area.getX()) + 8, 94, 64, 16, juce::Justification::centredLeft);
        g.drawText("LEFT", int(area.getX()) + 8, 291, 64, 16, juce::Justification::centredLeft);
        drawNodes(g);
    }

    void timerCallback() override
    {
        heldPeak = std::max(heldPeak * .85f, display.peak.exchange(0, std::memory_order_relaxed));
        if constexpr (isEqualiser)
        {
            if (display.ready.load(std::memory_order_acquire))
            {
                fftBuffer.fill(0);
                std::copy(display.frame.begin(), display.frame.end(), fftBuffer.begin());
                display.ready.store(false, std::memory_order_release);
                window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);
                fft.performFrequencyOnlyForwardTransform(fftBuffer.data(), true);
                for (size_t i = 0; i < spectrum.size(); ++i)
                    spectrum[i] = std::max(spectrum[i] - 2, juce::Decibels::gainToDecibels(
                        fftBuffer[i] * 4 / fftSize, -100.0f));
            }
        }
        repaint();
    }

    State& state;
    DisplayData& display;
    std::array<juce::TextButton, count> tabs;
    juce::ToggleButton enabled, octave, direct;
    juce::ComboBox type;
    std::vector<std::unique_ptr<Field>> fields, globals;
    std::unique_ptr<State::ButtonAttachment> onAttachment, octaveAttachment, directAttachment;
    std::unique_ptr<State::ComboBoxAttachment> typeAttachment;
    juce::RangedAudioParameter* dragX{};
    juce::RangedAudioParameter* dragY{};
    int selected = 0;
    float heldPeak = 0;
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> window{fftSize, juce::dsp::WindowingFunction<float>::hann, false};
    std::array<float, fftSize * 2> fftBuffer{};
    std::array<float, fftSize / 2 + 1> spectrum{};
};

class Processor final : public juce::AudioProcessor
{
public:
    Processor() : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
                  state(*this, nullptr, "PARAMETERS", layout())
    {
        for (int i = 0; i < count; ++i)
        {
            auto& p = params[i];
            const auto get = [&] (const char* key) { return state.getRawParameterValue(id(i, key)); };
            p.on = get("on");
            if constexpr (isEqualiser)
            {
                p.a = get("freq"); p.b = get("gain"); p.c = get("q"); p.mode = get("type");
            }
            else
            {
                p.a = get("delay"); p.b = get("detune"); p.c = get("pan"); p.mode = get("octave");
                p.feedback = get("feedback"); p.depth = get("depth"); p.speed = get("rate");
                voiceGain[i] = get("gain");
            }
        }
        output = state.getRawParameterValue("output");
        if constexpr (!isEqualiser)
        {
            directEnable = state.getRawParameterValue("direct");
            directGain = state.getRawParameterValue("directGain");
            directPan = state.getRawParameterValue("directPan");
            feedbackCut = state.getRawParameterValue("fbCut");
            lowCut = state.getRawParameterValue("wetLow");
            highCut = state.getRawParameterValue("wetHigh");
            align = state.getRawParameterValue("align");
            lowRange = state.getRawParameterValue("lowRange");
            shelfOn = {state.getRawParameterValue("lowShelfOn"), state.getRawParameterValue("highShelfOn")};
            shelfGain = {state.getRawParameterValue("lowShelfGain"), state.getRawParameterValue("highShelfGain")};
            shelfFreq = {state.getRawParameterValue("lowShelfFreq"), state.getRawParameterValue("highShelfFreq")};
        }
    }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return isEqualiser ? 0.25 : 32.0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override
    {
        if constexpr (isEqualiser) return new Editor(*this, state, display);
        return new VoiceEditor(*this, state, display);
    }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {
        const auto in = layout.getMainInputChannelSet(), out = layout.getMainOutputChannelSet();
        using Set = juce::AudioChannelSet;
        return (in == out && (in == Set::mono() || in == Set::stereo())) ||
               (!isEqualiser && in == Set::mono() && out == Set::stereo());
    }

    void prepareToPlay(double rate, int) override
    {
        sampleRate = rate;
        display.sampleRate.store(rate, std::memory_order_relaxed);
        if constexpr (!isEqualiser)
        {
            const auto length = size_t(rate * .20) + 4;
            for (auto& v : voices) v.samples.resize(length);
            for (auto& channel : directLine) channel.resize(length);
        }
        reset();
    }
    void releaseResources() override {}
    void reset() override
    {
        memory = {};
        shelfMemory = {};
        for (auto& channel : directLine)
            std::fill(channel.begin(), channel.end(), 0);
        cursor = 0;
        for (auto& v : voices)
        {
            std::fill(v.samples.begin(), v.samples.end(), 0);
            v.grainPhase = v.lfoPhase = 0;
            v.feedback = v.feedbackLow = v.wetLow = v.wetHigh = 0;
        }
    }

    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals guard;
        if constexpr (isEqualiser) equalise(audio);
        else doubleVoices(audio);
        float peak = 0;
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            peak = std::max(peak, audio.getMagnitude(ch, 0, audio.getNumSamples()));
        float last = display.peak.load(std::memory_order_relaxed);
        while (peak > last && !display.peak.compare_exchange_weak(last, peak, std::memory_order_relaxed)) {}
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
    void equalise(juce::AudioBuffer<float>& audio)
    {
        using P = Parameters;
        const float gain = juce::Decibels::decibelsToGain(P::get(output));
        std::array<Biquad, count> coefficients;
        for (int i = 0; i < count; ++i)
        {
            const auto& p = params[i];
            coefficients[i] = P::get(p.on) > .5f
                ? filter(sampleRate, P::get(p.a), P::get(p.b), P::get(p.c), int(P::get(p.mode))) : Biquad{};
        }
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        {
            auto* samples = audio.getWritePointer(ch);
            for (int n = 0; n < audio.getNumSamples(); ++n)
            {
                double x = samples[n];
                for (int i = 0; i < count; ++i)
                {
                    const auto& c = coefficients[i];
                    auto& m = memory[ch][i];
                    const double y = c.b0 * x + m[0];
                    m[0] = c.b1 * x - c.a1 * y + m[1];
                    m[1] = c.b2 * x - c.a2 * y;
                    x = y;
                }
                samples[n] = float(x * gain);
            }
        }
        for (int n = 0; n < audio.getNumSamples(); ++n)
        {
            const float left = audio.getSample(0, n);
            const float right = audio.getNumChannels() > 1 ? audio.getSample(1, n) : left;
            display.push(std::abs(left) >= std::abs(right) ? left : right);
        }
    }

    float delayTap(const Voice& v, double delay) const
    {
        const auto length = int(v.samples.size());
        double read = cursor - delay;
        if (read < 0) read += length;
        const int first = int(read);
        const int next = (first + 1) % length;
        return v.samples[first] + float(read - first) * (v.samples[next] - v.samples[first]);
    }

    struct VoiceSettings
    {
        double delay, semitones, depth, lfoStep;
        float left, right, feedback;
    };

    void doubleVoices(juce::AudioBuffer<float>& audio)
    {
        using P = Parameters;
        if (voices[0].samples.empty()) return;
        const int length = int(voices[0].samples.size());
        const int inputs = getMainBusNumInputChannels(), outputs = getMainBusNumOutputChannels();
        const bool extended = P::get(lowRange) > .5f;
        const double grain = sampleRate * (extended ? .08 : .04);
        const int dryDelay = P::get(align) > .5f ? juce::roundToInt(grain * .5) : 0;
        if (display.resetModulation.exchange(false, std::memory_order_acq_rel))
            for (auto& v : voices) v.lfoPhase = 0;
        std::array<Biquad, 2> shelves{};
        for (int i = 0; i < 2; ++i)
            if (P::get(shelfOn[i]) > .5f)
                shelves[i] = filter(sampleRate, P::get(shelfFreq[i]), P::get(shelfGain[i]), 1, i + 1);
        const auto applyShelves = [&] (double input, int ch)
        {
            for (int i = 0; i < 2; ++i)
            {
                auto& z = shelfMemory[ch][i];
                const auto& b = shelves[i];
                const double next = b.b0 * input + z[0];
                z[0] = b.b1 * input - b.a1 * next + z[1];
                z[1] = b.b2 * input - b.a2 * next;
                input = next;
            }
            return float(input);
        };
        const float master = juce::Decibels::decibelsToGain(P::get(output));
        const float dry = P::get(directEnable) > .5f ? juce::Decibels::decibelsToGain(P::get(directGain)) : 0;
        const float pan = P::get(directPan) / 45;
        const auto pole = [this] (float hz) { return float(-std::expm1(-2 * pi * hz / sampleRate)); };
        const float fbPole = pole(P::get(feedbackCut)), loPole = pole(P::get(lowCut)), hiPole = pole(P::get(highCut));
        std::array<VoiceSettings, count> settings{};
        for (int i = 0; i < count; ++i)
        {
            const auto& p = params[i];
            const double angle = (P::get(p.c) / 45 + 1) * pi / 4;
            const float gain = P::get(p.on) > .5f ? juce::Decibels::decibelsToGain(P::get(voiceGain[i])) : 0;
            settings[i] = {std::max(1.0, P::get(p.a) * .001 * sampleRate),
                           P::get(p.b) * .01 - (P::get(p.mode) > .5f ? 12 : 0),
                           P::get(p.depth) * .01, P::get(p.speed) / sampleRate,
                           float(gain * std::cos(angle)), float(gain * std::sin(angle)),
                           P::get(p.feedback) * .01f};
        }
        auto channels = audio.getArrayOfWritePointers();
        for (int n = 0; n < audio.getNumSamples(); ++n)
        {
            const float left = channels[0][n];
            const float right = inputs > 1 ? channels[1][n] : left;
            const float mono = (left + right) * .5f;
            const float dryPan = outputs == 1 ? 0.0f : pan;
            directLine[0][cursor] = left;
            directLine[1][cursor] = right;
            const int dryRead = (cursor - dryDelay + length) % length;
            const float directLeft = directLine[0][dryRead] * dry * (1 - std::max(dryPan, 0.f));
            const float directRight = directLine[1][dryRead] * dry * (1 + std::min(dryPan, 0.f));
            float outL = 0, outR = 0;
            float monoWet = 0;
            for (int i = 0; i < count; ++i)
            {
                auto& v = voices[i];
                const auto& s = settings[i];
                v.samples[cursor] = mono + v.feedback * s.feedback;
                float tap;
                if (std::abs(s.semitones) + std::abs(s.depth) < 1e-5)
                    tap = delayTap(v, s.delay + dryDelay);
                else
                {
                    const double ratio = std::exp2((s.semitones + s.depth * std::sin(2 * pi * v.lfoPhase)) / 12);
                    v.grainPhase += (1 - ratio) / grain;
                    v.grainPhase -= std::floor(v.grainPhase);
                    const double other = std::fmod(v.grainPhase + .5, 1.);
                    const float weight = float(.5 - .5 * std::cos(2 * pi * v.grainPhase));
                    tap = weight * delayTap(v, s.delay + v.grainPhase * grain) +
                          (1 - weight) * delayTap(v, s.delay + other * grain);
                }
                v.lfoPhase += s.lfoStep;
                if (v.lfoPhase >= 1) v.lfoPhase -= 1;
                v.feedbackLow += fbPole * (tap - v.feedbackLow);
                v.feedback = std::clamp(tap - v.feedbackLow, -4.f, 4.f);
                v.wetLow += loPole * (tap - v.wetLow);
                v.wetHigh += hiPole * (tap - v.wetLow - v.wetHigh);
                outL += v.wetHigh * s.left;
                outR += v.wetHigh * s.right;
                monoWet += v.wetHigh * std::hypot(s.left, s.right);
            }
            channels[0][n] = master * (directLeft + applyShelves(outputs == 1 ? monoWet : outL, 0));
            if (outputs > 1) channels[1][n] = master * (directRight + applyShelves(outR, 1));
            cursor = (cursor + 1) % length;
        }
    }

    State state;
    DisplayData display;
    std::array<Parameters, count> params{};
    std::array<std::atomic<float>*, count> voiceGain{};
    std::atomic<float>* output{};
    std::atomic<float>* directEnable{};
    std::atomic<float>* directGain{};
    std::atomic<float>* directPan{};
    std::atomic<float>* feedbackCut{};
    std::atomic<float>* lowCut{};
    std::atomic<float>* highCut{};
    std::atomic<float>* align{};
    std::atomic<float>* lowRange{};
    std::array<std::atomic<float>*, 2> shelfOn{}, shelfGain{}, shelfFreq{};
    std::array<std::vector<float>, 2> directLine;
    std::array<std::array<std::array<double, 2>, 2>, 2> shelfMemory{};
    std::array<std::array<std::array<double, 2>, count>, 2> memory{};
    std::array<Voice, count> voices;
    double sampleRate = 48000;
    int cursor = 0;
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Processor(); }
