class VoiceEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    VoiceEditor(juce::AudioProcessor& proc, State& controls, DisplayData& data)
        : AudioProcessorEditor(proc), state(controls), display(data)
    {
        for (int i = 0; i < 4; ++i)
        {
            const int y = 394 + i * 50;
            toggle(id(i, "on"), "V" + juce::String(i + 1), 27, y, 63);
            slider(id(i, "gain"), "Gain", " dB", 99, y, 82);
            slider(id(i, "pan"), "Pan", "", 187, y, 82);
            slider(id(i, "delay"), "Delay", " ms", 275, y, 82);
            slider(id(i, "feedback"), "Feedback", " %", 363, y, 82);
            toggle(id(i, "octave"), "-8ve", 454, y, 64);
            slider(id(i, "detune"), "Detune", " ct", 521, y, 82);
            slider(id(i, "depth"), "Depth", " ct", 609, y, 82);
            slider(id(i, "rate"), "Rate", " Hz", 697, y, 82);
        }
        toggle("direct", "Direct", 28, 291, 66);
        slider("directGain", "Dry gain", " dB", 98, 291, 77);
        slider("directPan", "Dry pan", "", 180, 291, 75);
        slider("fbCut", "FB cut", " Hz", 260, 291, 80);
        slider("wetLow", "Low cut", " Hz", 345, 291, 77);
        slider("wetHigh", "High cut", " Hz", 427, 291, 83);
        slider("lowShelfGain", "Low shelf", " dB", 520, 291, 81);
        slider("lowShelfFreq", "Low freq", " Hz", 605, 291, 88);
        slider("highShelfGain", "High shelf", " dB", 698, 291, 81);
        slider("highShelfFreq", "High freq", " Hz", 784, 291, 89);
        slider("output", "Output", " dB", 818, 523, 87);
        toggle("align", "Align dry", 486, 22, 100);
        toggle("lowRange", "Extended range", 601, 22, 137);
        toggle("lowShelfOn", "L", 644, 81, 44);
        toggle("highShelfOn", "H", 870, 81, 44);
        reset.setButtonText("Reset modulation");
        reset.setBounds(785, 20, 132, 28);
        reset.onClick = [this] { display.resetModulation.store(true, std::memory_order_release); };
        addAndMakeVisible(reset);
        setSize(938, 600);
        startTimerHz(24);
    }

    ~VoiceEditor() override
    {
        stopTimer();
        finishDrag();
    }

    void paint(juce::Graphics& g) override
    {
        g.setGradientFill({juce::Colour(0xff252135), 0, 0, juce::Colour(0xff0e1721), 0, 600, false});
        g.fillAll();
        SonmooBrand::draw(g, 24, 9, 22);
        g.setColour(ink);
        g.setFont(juce::FontOptions(29.f, juce::Font::bold));
        g.drawText("VoiceStack", 23, 29, 300, 37, juce::Justification::centredLeft);
        drawPosition(g);
        drawTiming(g);
        drawShelf(g);
        g.setColour(juce::Colour(0xff242c3c));
        g.fillRoundedRectangle(18, 255, 902, 76, 9);
        for (int i = 0; i < 4; ++i)
        {
            g.setColour(i % 2 ? juce::Colour(0xff202838) : juce::Colour(0xff1a2230));
            g.fillRoundedRectangle(18, 383.f + i * 50, 772, 46, 7);
            g.setColour(colours[i].withAlpha(value(id(i, "on")) > .5f ? 1.f : .35f));
            g.fillRoundedRectangle(19, 392.f + i * 50, 3, 28, 1.5f);
        }
        const std::array<const char*, 9> headers{"Voice", "Gain", "Pan", "Delay", "Feedback", "Octave", "Detune", "Mod depth", "Mod rate"};
        const std::array<int, 9> x{27, 99, 187, 275, 363, 451, 521, 609, 697};
        for (size_t i = 0; i < headers.size(); ++i) caption(g, headers[i], x[i], 352, 82);
        const std::array<const char*, 10> global{"", "Dry gain", "Dry pan", "FB cut", "Low cut", "High cut", "Low shelf", "Low freq", "High shelf", "High freq"};
        const std::array<int, 10> gx{28, 98, 180, 260, 345, 427, 520, 605, 698, 784};
        for (size_t i = 1; i < global.size(); ++i) caption(g, global[i], gx[i], 265, 82);
        drawOutput(g);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const auto point = event.position;
        float closest = 15;
        for (int i = 0; i < 4; ++i)
        {
            findNode(point, position(i), i, 1, closest);
            findNode(point, timing(i), i, 2, closest);
        }
        for (int i = 0; i < 2; ++i)
            findNode(point, shelfNode(i), i, 3, closest);
        if (graph == 0) return;
        if (graph == 1)
        {
            first = state.getParameter(id(index, "pan"));
            second = state.getParameter(id(index, "gain"));
        }
        else if (graph == 2)
        {
            first = state.getParameter(id(index, "delay"));
            second = state.getParameter(id(index, "detune"));
        }
        else
        {
            first = state.getParameter(shelfId(index, "Freq"));
            second = state.getParameter(shelfId(index, "Gain"));
        }
        first->beginChangeGesture();
        second->beginChangeGesture();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!first) return;
        const auto p = event.position;
        float x, y;
        if (graph == 1)
        {
            const auto delta = p - origin;
            x = 180 * std::atan2(delta.x, std::max(1.f, -delta.y)) / float(pi);
            y = (delta.getDistanceFromOrigin() - 29) * 48 / 86 - 48;
        }
        else
        {
            const auto a = graph == 2 ? timingArea : shelfArea;
            const float fx = std::clamp((p.x - a.getX()) / a.getWidth(), 0.f, 1.f);
            const float fy = std::clamp((p.y - a.getY()) / a.getHeight(), 0.f, 1.f);
            x = graph == 2 ? fx * 100 : 16 * std::pow(21357.f / 16, fx);
            y = graph == 2 ? 100 - 200 * fy : 12 - 36 * fy;
        }
        move(first, x);
        move(second, y);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { finishDrag(); }

private:
    struct SliderBinding
    {
        juce::Slider component;
        std::unique_ptr<State::SliderAttachment> attachment;
    };
    struct ToggleBinding
    {
        juce::ToggleButton component;
        std::unique_ptr<State::ButtonAttachment> attachment;
    };

    void slider(const juce::String& key, const char* name, const char* suffix, int x, int y, int width)
    {
        auto item = std::make_unique<SliderBinding>();
        auto& s = item->component;
        s.setTitle(name);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxAbove, false, width - 8, 19);
        s.setColour(juce::Slider::trackColourId, juce::Colour(0xffa897dd));
        s.setColour(juce::Slider::thumbColourId, ink);
        s.setColour(juce::Slider::textBoxBackgroundColourId, background);
        s.setColour(juce::Slider::textBoxOutlineColourId, line);
        s.setColour(juce::Slider::textBoxTextColourId, ink);
        s.setBounds(x, y, width, 32);
        addAndMakeVisible(s);
        item->attachment = std::make_unique<State::SliderAttachment>(state, key, s);
        s.textFromValueFunction = [unit = juce::String(suffix)] (double v)
        {
            return juce::String(v, unit == " Hz" && v >= 100 ? 0 : 1) + unit;
        };
        s.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
        s.updateText();
        sliders.push_back(std::move(item));
    }

    void toggle(const juce::String& key, const juce::String& name, int x, int y, int width)
    {
        auto item = std::make_unique<ToggleBinding>();
        item->component.setButtonText(name);
        item->component.setColour(juce::ToggleButton::textColourId, ink);
        item->component.setBounds(x, y, width, 27);
        addAndMakeVisible(item->component);
        item->attachment = std::make_unique<State::ButtonAttachment>(state, key, item->component);
        toggles.push_back(std::move(item));
    }

    float value(const juce::String& key) const { return Parameters::get(state.getRawParameterValue(key)); }
    static juce::String shelfId(int i, const char* field)
    {
        return juce::String(i == 0 ? "lowShelf" : "highShelf") + field;
    }

    void findNode(juce::Point<float> p, juce::Point<float> target, int i, int group, float& closest)
    {
        const auto distance = p.getDistanceFrom(target);
        if (distance >= closest) return;
        closest = distance;
        graph = group;
        index = i;
    }

    static void move(juce::RangedAudioParameter* parameter, float v)
    {
        const auto range = parameter->getNormalisableRange();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(std::clamp(v, range.start, range.end)));
    }

    void finishDrag()
    {
        if (first)
        {
            first->endChangeGesture();
            second->endChangeGesture();
        }
        first = second = nullptr;
        graph = 0;
    }

    static void caption(juce::Graphics& g, const juce::String& text, int x, int y, int width)
    {
        g.setColour(dim);
        g.setFont(juce::FontOptions(11.f, juce::Font::bold));
        g.drawText(text, x, y, width, 17, juce::Justification::centred);
    }

    void panel(juce::Graphics& g, juce::Rectangle<float> area, const char* title)
    {
        g.setColour(juce::Colour(0xff111823));
        g.fillRoundedRectangle(area, 9);
        caption(g, title, int(area.getX()), int(area.getY()) + 6, int(area.getWidth()));
    }

    juce::Point<float> position(int i) const
    {
        const float angle = value(id(i, "pan")) * float(pi / 180);
        const float radius = 29 + (value(id(i, "gain")) + 48) * (86.f / 48);
        return origin.getPointOnCircumference(radius, angle);
    }

    juce::Point<float> timing(int i) const
    {
        return {timingArea.getX() + timingArea.getWidth() * value(id(i, "delay")) / 100,
                timingArea.getY() + timingArea.getHeight() * (.5f - value(id(i, "detune")) / 200)};
    }

    float shelfX(float hz) const
    {
        return shelfArea.getX() + shelfArea.getWidth() * std::log(hz / 16) / std::log(21357.f / 16);
    }

    float shelfY(float gain) const
    {
        return shelfArea.getY() + shelfArea.getHeight() * std::clamp((12 - gain) / 36, 0.f, 1.f);
    }

    juce::Point<float> shelfNode(int i) const
    {
        return {shelfX(value(shelfId(i, "Freq"))), shelfY(value(shelfId(i, "Gain")))};
    }

    void marker(juce::Graphics& g, juce::Point<float> p, juce::Colour tint, const juce::String& label)
    {
        g.setColour(tint.withAlpha(.25f));
        g.fillEllipse(p.x - 12, p.y - 12, 24, 24);
        g.setColour(tint);
        g.fillEllipse(p.x - 8, p.y - 8, 16, 16);
        g.setColour(background);
        g.setFont(juce::FontOptions(10.f, juce::Font::bold));
        g.drawText(label, int(p.x) - 8, int(p.y) - 8, 16, 16, juce::Justification::centred);
    }

    void drawPosition(juce::Graphics& g)
    {
        panel(g, {18, 76, 288, 165}, "Stereo position / gain");
        g.setColour(line);
        for (float r : {30.f, 58.f, 87.f, 115.f})
        {
            juce::Path arc;
            arc.addCentredArc(origin.x, origin.y, r, r, 0, -1.12f, 1.12f, true);
            g.strokePath(arc, juce::PathStrokeType(1));
        }
        for (const float angle : {-float(pi / 4), 0.f, float(pi / 4)})
            g.drawLine(juce::Line<float>(origin, origin.getPointOnCircumference(117, angle)), 1.f);
        for (int i = 0; i < 4; ++i)
            marker(g, position(i), colours[i].withAlpha(value(id(i, "on")) > .5f ? 1.f : .35f), juce::String(i + 1));
        caption(g, "L", 32, 208, 34);
        caption(g, "R", 260, 208, 34);
    }

    void drawTiming(juce::Graphics& g)
    {
        panel(g, {324, 76, 284, 165}, "Delay / detune");
        for (int i = 0; i <= 5; ++i)
        {
            const int x = int(timingArea.getX() + i * timingArea.getWidth() / 5);
            g.setColour(line);
            g.drawVerticalLine(x, timingArea.getY(), timingArea.getBottom());
            caption(g, juce::String(i * 20), x - 15, 211, 30);
        }
        g.setColour(line.brighter(.35f));
        g.drawHorizontalLine(int(timingArea.getCentreY()), timingArea.getX(), timingArea.getRight());
        for (int i = 0; i < 4; ++i)
            marker(g, timing(i), colours[i].withAlpha(value(id(i, "on")) > .5f ? 1.f : .35f), juce::String(i + 1));
    }

    void drawShelf(juce::Graphics& g)
    {
        panel(g, {626, 76, 294, 165}, "Voice EQ");
        for (const float hz : {62.f, 250.f, 1000.f, 4000.f, 16000.f})
        {
            const int x = int(shelfX(hz));
            g.setColour(line);
            g.drawVerticalLine(x, shelfArea.getY(), shelfArea.getBottom());
            caption(g, hz < 1000 ? juce::String(int(hz)) : juce::String(int(hz / 1000)) + "k", x - 14, 211, 28);
        }
        g.setColour(line.brighter(.35f));
        g.drawHorizontalLine(int(shelfY(0)), shelfArea.getX(), shelfArea.getRight());
        const double sr = display.sampleRate.load(std::memory_order_relaxed);
        const auto low = value("lowShelfOn") > .5f ? filter(sr, value("lowShelfFreq"), value("lowShelfGain"), 1, 1) : Biquad{};
        const auto high = value("highShelfOn") > .5f ? filter(sr, value("highShelfFreq"), value("highShelfGain"), 1, 2) : Biquad{};
        juce::Path response;
        for (int k = 0; k <= 128; ++k)
        {
            const float t = float(k) / 128;
            const double hz = 16 * std::pow(21357. / 16, t);
            const auto z = std::polar(1.0, -2 * pi * std::min(hz, sr * .499) / sr);
            const auto mag = [&] (const Biquad& b)
            {
                return std::abs((b.b0 + b.b1 * z + b.b2 * z * z) / (1. + b.a1 * z + b.a2 * z * z));
            };
            const float gain = float(20 * std::log10(std::max(1e-8, mag(low) * mag(high))));
            const float x = shelfArea.getX() + shelfArea.getWidth() * t;
            if (k == 0) response.startNewSubPath(x, shelfY(gain));
            else response.lineTo(x, shelfY(gain));
        }
        g.setColour(juce::Colour(0xffbca5fa));
        g.strokePath(response, juce::PathStrokeType(2));
        marker(g, shelfNode(0), colours[0], "L");
        marker(g, shelfNode(1), colours[2], "H");
    }

    void drawOutput(juce::Graphics& g)
    {
        panel(g, {805, 351, 115, 230}, "Output");
        const auto level = juce::Decibels::gainToDecibels(peak, -60.f);
        const juce::Rectangle<float> bar(852, 386, 20, 108);
        g.setColour(line);
        g.fillRoundedRectangle(bar, 4);
        const float h = bar.getHeight() * juce::jlimit(0.f, 1.f, (level + 60) / 60);
        if (h > 0)
        {
            g.setColour(level > 0 ? juce::Colour(0xfff9a19d) : colours[0]);
            g.fillRoundedRectangle(bar.withY(bar.getBottom() - h).withHeight(h), 4);
        }
        caption(g, level <= -60 ? juce::String("-inf") : juce::String(level, 1) + " dBFS", 817, 498, 90);
    }

    void timerCallback() override
    {
        peak = std::max(peak * .84f, display.peak.exchange(0, std::memory_order_relaxed));
        repaint();
    }

    State& state;
    DisplayData& display;
    std::vector<std::unique_ptr<SliderBinding>> sliders;
    std::vector<std::unique_ptr<ToggleBinding>> toggles;
    juce::TextButton reset;
    const juce::Point<float> origin{162, 221};
    const juce::Rectangle<float> timingArea{349, 111, 234, 94}, shelfArea{651, 111, 242, 94};
    juce::RangedAudioParameter *first{}, *second{};
    int graph = 0, index = 0;
    float peak = 0;
};
