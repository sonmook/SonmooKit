class CurveEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    CurveEditor(juce::AudioProcessor& processor, State& parameters, DisplayData& data)
        : AudioProcessorEditor(processor), state(parameters), display(data)
    {
        for (int i = 0; i < 10; ++i)
        {
            const int y = 371 + i * 31;
            row[i].setButtonText(juce::String(i + 1));
            row[i].setColour(juce::TextButton::buttonColourId, surface);
            row[i].setColour(juce::TextButton::buttonOnColourId, colours[i].darker(.55f));
            row[i].setBounds(161, y, 35, 25);
            row[i].onClick = [this, i] { select(i); };
            addAndMakeVisible(row[i]);
            button(id(i, "on"), "On", {207, y, 61, 25});
            slider(id(i, "gain"), "Gain", " dB", {281, y, 125, 25}, colours[i]);
            slider(id(i, "freq"), "Frequency", " Hz", {424, y, 127, 25}, colours[i]);
            slider(id(i, "q"), "Q", "", {567, y, 120, 25}, colours[i]);
            auto& choice = types[i];
            choice.addItemList({"Bell", "Low Shelf", "High Shelf", "Low Pass", "High Pass", "Proportional Q"}, 1);
            choice.setTitle("Band " + juce::String(i + 1) + " filter type");
            choice.setBounds(706, y, 133, 25);
            choice.setColour(juce::ComboBox::backgroundColourId, background);
            choice.setColour(juce::ComboBox::outlineColourId, line);
            addAndMakeVisible(choice);
            typeBinding[i] = std::make_unique<State::ComboBoxAttachment>(state, id(i, "type"), choice);
        }
        slider("inputL", "Left input gain", "", {29, 410, 52, 231}, colours[0], true);
        slider("inputR", "Right input gain", "", {86, 410, 52, 231}, colours[0], true);
        button("phaseL", "Pol L", {28, 377, 57, 24});
        button("phaseR", "Pol R", {85, 377, 60, 24});
        slider("output", "Output gain", "", {862, 410, 63, 231}, colours[0], true);
        flat.setButtonText("Flat gains");
        flat.setTooltip("Set the gain of all ten bands to 0 dB");
        flat.setBounds(913, 22, 105, 28);
        flat.onClick = [this]
        {
            for (int i = 0; i < 10; ++i)
                change(*state.getParameter(id(i, "gain")), 0);
        };
        addAndMakeVisible(flat);
        showSpectrum.setButtonText("Spectrum");
        showSpectrum.setToggleState(true, juce::dontSendNotification);
        showSpectrum.setBounds(801, 22, 101, 28);
        showSpectrum.onClick = [this]
        {
            display.listening.store(showSpectrum.getToggleState(), std::memory_order_relaxed);
            spectrum.fill(-100);
            repaint();
        };
        addAndMakeVisible(showSpectrum);
        select(0);
        setSize(1040, 708);
        spectrum.fill(-100);
        display.listening.store(true, std::memory_order_relaxed);
        startTimerHz(24);
    }

    ~CurveEditor() override
    {
        stopTimer();
        finishDrag();
        display.listening.store(false, std::memory_order_relaxed);
    }

    void paint(juce::Graphics& g) override
    {
        g.setGradientFill({juce::Colour(0xff182e3a), 0, 0, juce::Colour(0xff101820), 0, 708, false});
        g.fillAll();
        caption(g, "SONMOOKIT", 24, 11, 160, 18, 12, colours[0]);
        caption(g, "CurveTen", 24, 27, 340, 40, 31, ink);
        g.setColour(juce::Colour(0xff0b151c));
        g.fillRoundedRectangle(18, 75, 1004, 245, 9);
        drawGraph(g);
        panel(g, {18, 334, 130, 354});
        panel(g, {154, 334, 691, 354});
        panel(g, {851, 334, 171, 354});
        for (int i = 0; i < 10; ++i)
        {
            g.setColour(i == selected ? juce::Colour(0xff29444e)
                                      : (i % 2 ? juce::Colour(0xff22313b) : juce::Colour(0xff1b2932)));
            g.fillRoundedRectangle(158, 368.f + 31 * i, 682, 30, 5);
            g.setColour(colours[i].withAlpha(value(i, "on") > .5f ? 1.f : .3f));
            g.fillRoundedRectangle(157, 373.f + 31 * i, 3, 21, 1.5f);
        }
        caption(g, "INPUT", 30, 341, 105, 22);
        caption(g, "BAND", 162, 341, 80, 22);
        caption(g, "GAIN", 283, 341, 99, 22);
        caption(g, "FREQUENCY", 424, 341, 115, 22);
        caption(g, "Q", 569, 341, 100, 22);
        caption(g, "FILTER TYPE", 707, 341, 122, 22);
        caption(g, "OUTPUT", 863, 341, 138, 22);
        caption(g, "L", 52, 648, 22, 18);
        caption(g, "R", 107, 648, 22, 18);
        meter(g, 946, 399, peaks[0], "L");
        meter(g, 981, 399, peaks[1], "R");
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int i = nearest(e.position);
        if (i < 0) return;
        select(i);
        if (e.mods.isPopupMenu())
        {
            types[i].showPopup();
            return;
        }
        start = e.position;
        if (e.mods.isAltDown())
        {
            dragQ = state.getParameter(id(i, "q"));
            initialQ = dragQ->getValue();
            dragQ->beginChangeGesture();
        }
        else
        {
            dragFrequency = state.getParameter(id(i, "freq"));
            dragGain = state.getParameter(id(i, "gain"));
            dragFrequency->beginChangeGesture();
            dragGain->beginChangeGesture();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragQ)
        {
            dragQ->setValueNotifyingHost(juce::jlimit(0.f, 1.f, initialQ + (e.position.x - start.x) / 240));
        }
        else if (dragFrequency)
        {
            const auto d = e.position - start;
            const bool horizontal = std::abs(d.x) >= std::abs(d.y);
            if (!e.mods.isCtrlDown() || horizontal)
            {
                const float x = unit((e.position.x - graph.getX()) / graph.getWidth());
                dragFrequency->setValueNotifyingHost(dragFrequency->convertTo0to1(hertz(x)));
            }
            if (!e.mods.isCtrlDown() || !horizontal)
            {
                const float y = unit((e.position.y - graph.getY()) / graph.getHeight());
                dragGain->setValueNotifyingHost(dragGain->convertTo0to1(18 - 36 * y));
            }
        }
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { finishDrag(); }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        const int i = nearest(e.position);
        if (i < 0) return;
        finishDrag();
        change(*state.getParameter(id(i, "on")), value(i, "on") > .5f ? 0.f : 1.f);
    }

private:
    struct Fader
    {
        juce::Slider widget;
        std::unique_ptr<State::SliderAttachment> attachment;
    };
    struct Switch
    {
        juce::ToggleButton widget;
        std::unique_ptr<State::ButtonAttachment> attachment;
    };

    void slider(const juce::String& key, const char* label, const char* suffix, juce::Rectangle<int> bounds,
                juce::Colour tint, bool vertical = false)
    {
        auto f = std::make_unique<Fader>();
        auto& s = f->widget;
        s.setTitle(label);
        s.setSliderStyle(vertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(vertical ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight,
                          false, vertical ? bounds.getWidth() - 3 : 59, 20);
        s.setColour(juce::Slider::trackColourId, tint);
        s.setColour(juce::Slider::thumbColourId, ink);
        s.setColour(juce::Slider::textBoxTextColourId, ink);
        s.setColour(juce::Slider::textBoxBackgroundColourId, background);
        s.setColour(juce::Slider::textBoxOutlineColourId, line);
        s.setBounds(bounds);
        addAndMakeVisible(s);
        f->attachment = std::make_unique<State::SliderAttachment>(state, key, s);
        s.textFromValueFunction = [unit = juce::String(suffix)] (double v)
        {
            return juce::String(v, unit == " Hz" && v >= 100 ? 0 : 1) + unit;
        };
        s.valueFromTextFunction = [] (const juce::String& v) { return v.getDoubleValue(); };
        s.updateText();
        faders.push_back(std::move(f));
    }

    void button(const juce::String& key, const char* label, juce::Rectangle<int> bounds)
    {
        auto b = std::make_unique<Switch>();
        b->widget.setButtonText(label);
        b->widget.setColour(juce::ToggleButton::textColourId, dim);
        b->widget.setColour(juce::ToggleButton::tickColourId, colours[0]);
        b->widget.setBounds(bounds);
        addAndMakeVisible(b->widget);
        b->attachment = std::make_unique<State::ButtonAttachment>(state, key, b->widget);
        switches.push_back(std::move(b));
    }

    void select(int i)
    {
        selected = i;
        for (int j = 0; j < 10; ++j) row[j].setToggleState(j == i, juce::dontSendNotification);
        repaint();
    }

    static void change(juce::RangedAudioParameter& p, float value)
    {
        p.beginChangeGesture();
        p.setValueNotifyingHost(p.convertTo0to1(value));
        p.endChangeGesture();
    }

    void finishDrag()
    {
        for (auto* p : {dragFrequency, dragGain, dragQ}) if (p) p->endChangeGesture();
        dragFrequency = dragGain = dragQ = nullptr;
    }

    float value(int i, const char* key) const { return Parameters::get(state.getRawParameterValue(id(i, key))); }
    static float unit(float v) { return juce::jlimit(0.f, 1.f, v); }
    static float hertz(float x) { return 16 * std::pow(21357.f / 16, x); }
    float xAt(float hz) const { return graph.getX() + graph.getWidth() * std::log(hz / 16) / std::log(21357.f / 16); }
    float yAt(float db) const { return graph.getY() + graph.getHeight() * unit(.5f - db / 36); }

    juce::Point<float> node(int i) const
    {
        return {xAt(value(i, "freq")), yAt(value(i, "gain"))};
    }

    int nearest(juce::Point<float> p) const
    {
        if (!graph.expanded(12).contains(p)) return -1;
        int result = -1;
        float limit = 17;
        for (int i = 0; i < 10; ++i)
        {
            const auto d = p.getDistanceFrom(node(i));
            if (d < limit) { result = i; limit = d; }
        }
        return result;
    }

    static void caption(juce::Graphics& g, const juce::String& text, int x, int y, int w, int h,
                        float size = 11, juce::Colour tint = dim)
    {
        g.setColour(tint);
        g.setFont(juce::FontOptions(size, juce::Font::bold));
        g.drawText(text, x, y, w, h, juce::Justification::centredLeft);
    }

    static void panel(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour(juce::Colour(0xff17242c));
        g.fillRoundedRectangle(area, 8);
    }

    void drawGraph(juce::Graphics& g)
    {
        for (const float hz : {31.f, 63.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f})
        {
            const int x = int(xAt(hz));
            g.setColour(line);
            g.drawVerticalLine(x, graph.getY(), graph.getBottom());
            caption(g, hz >= 1000 ? juce::String(int(hz / 1000)) + "k" : juce::String(int(hz)), x - 9, 294, 35, 17);
        }
        for (int db = -18; db <= 18; db += 6)
        {
            const int y = int(yAt(float(db)));
            g.setColour(db == 0 ? line.brighter(.5f) : line);
            g.drawHorizontalLine(y, graph.getX(), graph.getRight());
            caption(g, juce::String(db), 29, y - 7, 25, 14);
        }
        const auto rate = display.sampleRate.load(std::memory_order_relaxed);
        std::array<Biquad, 10> filters{};
        for (int i = 0; i < 10; ++i)
            if (value(i, "on") > .5f)
                filters[i] = filter(rate, value(i, "freq"), value(i, "gain"), value(i, "q"), int(value(i, "type")));
        juce::Path curve, trace;
        for (int i = 0; i <= 430; ++i)
        {
            const float t = float(i) / 430;
            const float hz = hertz(t);
            const float x = graph.getX() + graph.getWidth() * t;
            const auto z = std::polar(1., -2 * pi * std::min(double(hz), rate * .499) / rate);
            const auto z2 = z * z;
            double db = 0;
            for (const auto& f : filters)
                db += 20 * std::log10(std::max(1e-10,
                    std::abs((f.b0 + f.b1 * z + f.b2 * z2) / (1. + f.a1 * z + f.a2 * z2))));
            const int bin = std::clamp(juce::roundToInt(hz * fftSize / rate), 1, fftSize / 2);
            const float sy = graph.getBottom() - graph.getHeight() * unit((spectrum[bin] + 100) / 100);
            if (i == 0) { curve.startNewSubPath(x, yAt(float(db))); trace.startNewSubPath(x, sy); }
            else { curve.lineTo(x, yAt(float(db))); trace.lineTo(x, sy); }
        }
        if (showSpectrum.getToggleState())
        {
            g.setColour(dim.withAlpha(.6f));
            g.strokePath(trace, juce::PathStrokeType(1.f));
        }
        g.setColour(colours[0]);
        g.strokePath(curve, juce::PathStrokeType(2.f));
        for (int i = 0; i < 10; ++i)
        {
            const auto p = node(i);
            const float r = i == selected ? 11.f : 9.f;
            g.setColour(background);
            g.fillEllipse(p.x - r, p.y - r, r * 2, r * 2);
            const auto tint = colours[i].withAlpha(value(i, "on") > .5f ? 1.f : .4f);
            g.setColour(tint);
            g.drawEllipse(p.x - r, p.y - r, r * 2, r * 2, i == selected ? 2.f : 1.25f);
            caption(g, juce::String(i + 1), int(p.x) - (i == 9 ? 6 : 3), int(p.y) - 8, 17, 16, 11, tint);
        }
    }

    void meter(juce::Graphics& g, int x, int y, float peak, const char* name)
    {
        const float db = juce::Decibels::gainToDecibels(peak, -60.f);
        const juce::Rectangle<float> bar(float(x), float(y), 17, 230);
        g.setColour(line);
        g.fillRoundedRectangle(bar, 4);
        const float h = bar.getHeight() * unit((db + 60) / 60);
        if (h > 0)
        {
            g.setColour(db > 0 ? juce::Colour(0xfff89089) : colours[0]);
            g.fillRoundedRectangle(bar.withY(bar.getBottom() - h).withHeight(h), 4);
        }
        caption(g, name, x + 5, 647, 20, 17);
        caption(g, db <= -60 ? juce::String("-inf") : juce::String(db, 1), x - 4, 665, 35, 16);
    }

    void timerCallback() override
    {
        for (int ch = 0; ch < 2; ++ch)
            peaks[ch] = std::max(peaks[ch] * .85f, display.channelPeak[ch].exchange(0, std::memory_order_relaxed));
        if (display.ready.load(std::memory_order_acquire))
        {
            std::copy(display.frame.begin(), display.frame.end(), fftBuffer.begin());
            std::fill(fftBuffer.begin() + fftSize, fftBuffer.end(), 0);
            display.ready.store(false, std::memory_order_release);
            window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform(fftBuffer.data(), true);
            for (size_t i = 0; i < spectrum.size(); ++i)
                spectrum[i] = std::max(spectrum[i] - 2, juce::Decibels::gainToDecibels(fftBuffer[i] * 4 / fftSize, -100.f));
        }
        repaint();
    }

    State& state;
    DisplayData& display;
    std::array<juce::TextButton, 10> row;
    std::array<juce::ComboBox, 10> types;
    std::array<std::unique_ptr<State::ComboBoxAttachment>, 10> typeBinding;
    std::vector<std::unique_ptr<Fader>> faders;
    std::vector<std::unique_ptr<Switch>> switches;
    juce::TextButton flat;
    juce::ToggleButton showSpectrum;
    juce::RangedAudioParameter *dragFrequency{}, *dragGain{}, *dragQ{};
    juce::Point<float> start;
    const juce::Rectangle<float> graph{58, 96, 941, 195};
    int selected = 0;
    float initialQ = 0;
    std::array<float, 2> peaks{};
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> window{fftSize, juce::dsp::WindowingFunction<float>::hann, false};
    std::array<float, fftSize * 2> fftBuffer{};
    std::array<float, fftSize / 2 + 1> spectrum{};
};
