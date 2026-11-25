#pragma once

namespace {
constexpr auto background = 0xff101522;
constexpr auto panel = 0xff192234;
constexpr auto track = 0xff2c394f;
constexpr auto ink = 0xfff2f5fc;
constexpr auto muted = 0xffaab9ce;
constexpr auto accent = effect == 0 ? 0xff52dace : effect == 1 ? 0xffc2a2ff :
                        effect == 3 ? 0xff9ce1a5 : effect == 4 ? 0xfff3a1ac : 0xffffbe77;

struct Activity
{
    std::atomic<float> input{0}, output{0}, reduction{0}, detection{0}, echoTimeMs{0};
};

class DialStyle final : public juce::LookAndFeel_V4
{
public:
    DialStyle()
    {
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(ink));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(panel));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(track));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float position, float start, float end, juce::Slider&) override
    {
        const juce::Rectangle<float> bounds{float(x), float(y), float(w), float(h)};
        const auto centre = bounds.getCentre();
        if constexpr (effect == 4)
        {
            if (w > 150 && h > 150)
            {
                drawDriveDial(g, centre, std::min(w, h) * 0.5f, position, start, end);
                return;
            }
        }
        const float radius = std::min(w, h) * 0.5f - 6.0f;
        const float angle = start + position * (end - start);
        const auto arc = [&] (float stop, juce::Colour colour)
        {
            juce::Path p;
            p.addCentredArc(centre.x, centre.y, radius, radius, 0, start, stop, true);
            g.setColour(colour);
            g.strokePath(p, juce::PathStrokeType(4, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        };
        arc(end, juce::Colour(track));
        if (position > 0) arc(angle, juce::Colour(accent));
        const float face = radius - 8;
        const juce::Rectangle<float> circle(centre.x - face, centre.y - face, face * 2, face * 2);
        g.setGradientFill({juce::Colour(0xff33415a), circle.getTopLeft(),
                           juce::Colour(0xff1c273a), circle.getBottomRight(), false});
        g.fillEllipse(circle);
        g.setColour(juce::Colour(ink));
        const auto tip = centre.getPointOnCircumference(face - 5, angle);
        const auto base = centre.getPointOnCircumference(face - 14, angle);
        g.drawLine({base, tip}, 2.6f);
    }

private:
    static void drawDriveDial(juce::Graphics& g, juce::Point<float> centre, float space,
                              float position, float start, float end)
    {
        const float angle = start + position * (end - start);
        const float radius = space - 22;
        const auto circle = [centre] (float r)
        {
            return juce::Rectangle<float>(centre.x - r, centre.y - r, r * 2, r * 2);
        };

        const auto ring = [&] (float stop, juce::Colour colour)
        {
            juce::Path path;
            path.addCentredArc(centre.x, centre.y, radius, radius, 0, start, stop, true);
            g.setColour(colour);
            g.strokePath(path, juce::PathStrokeType(4, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        };
        ring(end, juce::Colour(0xff53392f));
        if (position > 0) ring(angle, juce::Colour(accent));

        for (int mark = 0; mark <= 10; ++mark)
        {
            const auto tickAngle = start + (end - start) * (float(mark) / 10.0f);
            const auto tickStart = centre.getPointOnCircumference(radius + 5, tickAngle);
            const auto tickEnd = centre.getPointOnCircumference(radius + 10, tickAngle);
            g.setColour(juce::Colour(mark <= position * 10 ? accent : muted));
            g.drawLine(juce::Line<float>(tickStart, tickEnd), 1.2f);
            if (mark == 0 || mark == 5 || mark == 10)
            {
                const auto label = centre.getPointOnCircumference(radius + 17, tickAngle);
                g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
                g.drawText(juce::String(mark), int(label.x) - 12, int(label.y) - 7,
                           24, 14, juce::Justification::centred);
            }
        }

        auto rim = circle(radius - 9);
        g.setColour(juce::Colour(0xff08090b));
        g.fillEllipse(rim.translated(0, 3).expanded(2));
        g.setGradientFill({juce::Colour(0xffb88968), rim.getTopLeft(),
                           juce::Colour(0xff564133), rim.getBottomRight(), false});
        g.fillEllipse(rim);
        const auto face = circle(radius - 13);
        g.setGradientFill({juce::Colour(0xff535159), face.getTopLeft(),
                           juce::Colour(0xff232128), face.getBottomRight(), false});
        g.fillEllipse(face);
        g.setColour(juce::Colour(0xff716b71));
        g.drawEllipse(face.reduced(3), 1);
        g.setColour(juce::Colour(accent));
        const auto a = centre.getPointOnCircumference(radius - 20, angle);
        const auto b = centre.getPointOnCircumference(radius - 34, angle);
        g.drawLine(juce::Line<float>(a, b), 3.5f);
    }
};

class SonmooEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    SonmooEditor(juce::AudioProcessor& processor, State& parameters, Activity& activity)
        : AudioProcessorEditor(processor), state(parameters), levels(activity)
    {
        if constexpr (effect == 0)
        {
            dial("time", "Time", " ms");
            dial("feedback", "Feedback", " %");
            dial("mix", "Mix", " %");
            dial("lowcut", "Low Cut", " Hz", 0);
            dial("highcut", "High Cut", " Hz", 0);
            dial("modDepth", "Mod depth", " %");
            dial("modRate", "Mod rate", " Hz", 2);
            dial("outTrim", "Output", " dB");
            addToggle(preRead, preReadAttachment, "lofi", "LoFi");
            const auto* sync = static_cast<juce::AudioParameterChoice*>(state.getParameter("sync"));
            division.addItemList(sync->choices, 1);
            tap.setButtonText("Tap");
            tap.onClick = [this]
            {
                const auto now = juce::Time::getMillisecondCounterHiRes();
                const auto interval = now - lastTap;
                if (lastTap > 0 && interval >= 1 && interval <= 3500)
                {
                    if (state.getRawParameterValue("sync")->load() == 0)
                        controls[0]->slider.setValue(interval, juce::sendNotificationSync);
                    else
                    {
                        constexpr double beats[]{.25, .5, .75, 1., 2., 4.};
                        const int index = division.getSelectedItemIndex() - 1;
                        const double activeMs = levels.echoTimeMs.load();
                        if (index >= 0 && index < 6 && activeMs > 0)
                        {
                            const double wanted = interval * beats[index] / activeMs;
                            auto nearest = std::min_element(std::begin(beats), std::end(beats),
                                [wanted](double a, double b) { return std::abs(a-wanted) < std::abs(b-wanted); });
                            division.setSelectedItemIndex(1 + int(nearest - beats), juce::sendNotificationSync);
                        }
                    }
                }
                lastTap = now;
            };
            addAndMakeVisible(tap);
            division.setTooltip("Sync delay time to project tempo");
            division.setColour(juce::ComboBox::backgroundColourId, juce::Colour(panel));
            division.setColour(juce::ComboBox::outlineColourId, juce::Colour(track));
            addAndMakeVisible(division);
            syncAttachment = std::make_unique<State::ComboBoxAttachment>(state, "sync", division);
            ping.setButtonText("Ping-pong");
            ping.setColour(juce::ToggleButton::textColourId, juce::Colour(ink));
            ping.setColour(juce::ToggleButton::tickColourId, juce::Colour(accent));
            addAndMakeVisible(ping);
            pingAttachment = std::make_unique<State::ButtonAttachment>(state, "ping", ping);
        }
        else if constexpr (effect == 1)
        {
            dial("threshold", "Threshold", " dB");
            addToggle(ping, pingAttachment, "arc", "Adaptive release");
            dial("ratio", "Ratio", ":1", 2);
            dial("attack", "Attack", " ms");
            dial("release", "Release", " ms");
            dial("knee", "Knee", " dB");
            dial("makeup", "Makeup", " dB");
            dial("mix", "Mix", " %");
        }
        else if constexpr (effect == 2)
        {
            dial("threshold", "Threshold", " dB");
            dial("ceiling", "Ceiling", " dBFS");
            dial("release", "Release", " ms", 2);
            addToggle(ping, pingAttachment, "arc", "Adaptive release");
        }
        else if constexpr (effect == 3)
        {
            dial("threshold", "Threshold", " dB");
            dial("range", "Range", " dB");
            dial("width", "Detection Width", " %");
            dial("mode", "Wide to Split", " %");
            history.fill(-80);
            addToggle(ping, pingAttachment, "monitor", "Audition sibilance");
            addToggle(preRead, preReadAttachment, "lookahead", "12 ms lookahead");
        }
        else
        {
            dial("drive", "Drive", "");
            dial("tone", "Tone", " %");
            dial("trim", "Output Trim", " dB");
            dial("mix", "Mix", " %");
        }
        if constexpr (effect == 4)
        {
            controls.front()->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 25);
            setSize(356, 482);
        }
        else if constexpr (effect == 0)
            setSize(548, 490);
        else if constexpr (effect == 1 || effect == 2)
        {
            const int count = effect == 1 ? 1 : 2;
            for (int i = 0; i < count; ++i)
            {
                auto& slider = controls[static_cast<size_t>(i)]->slider;
                slider.setSliderStyle(juce::Slider::LinearVertical);
                slider.setColour(juce::Slider::trackColourId, juce::Colour(accent));
                slider.setColour(juce::Slider::thumbColourId, juce::Colour(ink));
            }
            setSize(effect == 1 ? 558 : 438, effect == 1 ? 380 : 327);
        }
        else
            setSize(552, 472);
        startTimerHz(30);
    }

    ~SonmooEditor() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        if constexpr (effect == 4)
        {
            paintDrive(g);
            return;
        }
        paintShell(g);
        if constexpr (effect == 0) paintEcho(g);
        if constexpr (effect == 1 || effect == 2) paintDynamics(g);
        if constexpr (effect == 3) paintSibilance(g);
    }

    void resized() override
    {
        if constexpr (effect == 4)
        {
            controls[0]->label.setBounds(0, 83, getWidth(), 19);
            controls[0]->slider.setBounds(57, 105, 242, 211);
            for (int i = 1; i < 4; ++i)
            {
                const int x = 16 + (i - 1) * 108;
                controls[i]->label.setBounds(x, 372, 108, 19);
                controls[i]->slider.setBounds(x + 10, 392, 88, 79);
            }
            return;
        }
        if constexpr (effect == 0)
        {
            placeDial(0, 30, 149, 116, 126);
            for (int i = 1; i < 5; ++i)
                placeDial(i, 169 + ((i - 1) % 2) * 114, 112 + ((i - 1) / 2) * 104, 108, 88);
            division.setBounds(31, 337, 118, 26);
            for (int i = 0; i < 3; ++i) placeDial(5+i, 30+i*135, 376, 110, 90);
            preRead.setBounds(434, 412, 94, 28);
            tap.setBounds(170, 337, 90, 26);
            ping.setBounds(296, 337, 104, 26);
        }
        if constexpr (effect == 1)
        {
            ping.setBounds(156, 338, 160, 25);
            placeDial(0, 31, 102, 91, 235);
            for (int i = 1; i < 7; ++i)
                placeDial(i, 156 + ((i - 1) % 3) * 82, 100 + ((i - 1) / 3) * 119, 80, 100);
        }
        if constexpr (effect == 2)
        {
            ping.setBounds(205, 289, 156, 24);
            placeDial(0, 23, 94, 87, 203);
            placeDial(1, 115, 94, 87, 203);
            placeDial(2, 212, 135, 88, 117);
        }
        if constexpr (effect == 3)
        {
            for (int i = 0; i < 4; ++i)
                placeDial(i, 23 + i * 126, 323, 124, 94);
            ping.setBounds(32, 433, 164, 26);
            preRead.setBounds(210, 433, 152, 26);
        }
    }

private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<State::SliderAttachment> attachment;
        ~Control() { slider.setLookAndFeel(nullptr); }
    };

    void dial(const char* id, const char* name, const char* suffix, int decimals = 1)
    {
        auto control = std::make_unique<Control>();
        auto& slider = control->slider;
        slider.setLookAndFeel(&style);
        slider.setTitle(name);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 86, 20);
        addAndMakeVisible(slider);
        control->attachment = std::make_unique<State::SliderAttachment>(state, id, slider);
        slider.textFromValueFunction = [decimals, unit = juce::String(suffix)] (double value)
        {
            return juce::String(value, decimals) + unit;
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.getDoubleValue();
        };
        slider.updateText();
        control->label.setText(name, juce::dontSendNotification);
        control->label.setJustificationType(juce::Justification::centred);
        control->label.setColour(juce::Label::textColourId, juce::Colour(muted));
        addAndMakeVisible(control->label);
        controls.push_back(std::move(control));
    }

    void placeDial(int slot, int x, int y, int width, int height)
    {
        auto& control = *controls[static_cast<size_t>(slot)];
        control.label.setBounds(x, y, width, 20);
        control.slider.setBounds(x + 3, y + 24, width - 6, height - 24);
    }

    void paintShell(juce::Graphics& g)
    {
        g.setGradientFill({juce::Colour(panel).interpolatedWith(juce::Colour(accent), .08f),
                           0, 0, juce::Colour(background), 0, float(getHeight()), false});
        g.fillAll();
        SonmooBrand::draw(g, 24, 12, 22);
        g.setColour(juce::Colour(ink));
        g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
        g.drawText(getAudioProcessor()->getName(), 24, 33, 390, 36, juce::Justification::centredLeft);
        g.setColour(juce::Colour(track));
        g.drawHorizontalLine(77, 24, float(getWidth() - 24));
    }

    void paintEcho(juce::Graphics& g)
    {
        g.setColour(juce::Colour(0xff091a1d));
        g.fillRoundedRectangle(26, 98, 124, 46, 7);
        g.setColour(juce::Colour(accent));
        g.setFont(juce::FontOptions(21.0f, juce::Font::bold));
        const bool sync = state.getRawParameterValue("sync")->load() > 0;
        const float ms = sync ? levels.echoTimeMs.load(std::memory_order_relaxed)
                              : state.getRawParameterValue("time")->load();
        const juce::String readout = ms > 0 ? juce::String(ms, 0) + " ms" : juce::String("-- ms");
        g.drawText(readout, 29, 99, 118, 44, juce::Justification::centred);
        verticalMeter(g, 411, 113, 50, 167, "IN", shownInput, false);
        verticalMeter(g, 470, 113, 50, 167, "OUT", shownOutput, false);
        g.setColour(juce::Colour(muted));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Tempo sync", 31, 314, 118, 19, juce::Justification::centredLeft);
    }

    void paintDynamics(juce::Graphics& g)
    {
        constexpr bool compressor = effect == 1;
        const int x = compressor ? 414 : 306;
        const int width = compressor ? 42 : 40;
        const int height = compressor ? 178 : 148;
        verticalMeter(g, x, 122, width, height, "IN", shownInput, false);
        verticalMeter(g, x + width, 122, width, height, compressor && state.getRawParameterValue("ratio")->load() < 1.f ? "EXP" : "GR", shownReduction, true);
        verticalMeter(g, x + width * 2, 122, width, height, "OUT", shownOutput, false);
    }

    void verticalMeter(juce::Graphics& g, int x, int y, int w, int h,
                       const char* name, float db, bool reduction)
    {
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(juce::Colour(muted));
        g.drawText(name, x, y - 24, w, 19, juce::Justification::centred);
        const juce::Rectangle<float> rail(float(x + (w - 14) / 2), float(y), 14, float(h));
        g.setColour(juce::Colour(track));
        g.fillRoundedRectangle(rail, 4);
        const float amount = juce::jlimit(0.0f, 1.0f, reduction ? db / 24 : (db + 60) / 60);
        const float filled = rail.getHeight() * amount;
        g.setColour(juce::Colour(!reduction && db > 0 ? 0xffff7e84 : accent));
        if (filled > 0)
            g.fillRoundedRectangle(reduction ? rail.withHeight(filled)
                                             : rail.withY(rail.getBottom() - filled).withHeight(filled), 4);
        g.setColour(juce::Colour(ink));
        const auto value = !reduction && db <= -60 ? juce::String("-inf") : juce::String(db, 1);
        g.drawText(value, x, y + h + 6, w, 16, juce::Justification::centred);
    }

    void paintSibilance(juce::Graphics& g)
    {
        g.setColour(juce::Colour(panel));
        g.fillRoundedRectangle(24, 87, float(getWidth() - 48), 94, 9);
        meter(g, 99, "INPUT", shownInput, false);
        meter(g, 119, "OUTPUT", shownOutput, false);
        meter(g, 139, "REDUCTION", shownReduction, true);
        meter(g, 159, "DETECTION", shownDetection, false);
        drawDetection(g);
    }

    void paintDrive(juce::Graphics& g)
    {
        g.setGradientFill({juce::Colour(0xff33292e), 0, 0,
                           juce::Colour(0xff141319), 0, 355, false});
        g.fillAll();
        g.setColour(juce::Colour(0xff564247));
        g.drawRoundedRectangle(9, 9, 338, 464, 13, 1);
        g.setColour(juce::Colour(accent));
        g.fillRoundedRectangle(28, 31, 4, 37, 2);
        SonmooBrand::draw(g, 43, 20, 23);
        g.setColour(juce::Colour(ink));
        g.setFont(juce::FontOptions(31.0f, juce::Font::bold));
        g.drawText("DriveTone", 43, 41, 260, 38, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff211c23));
        g.fillRoundedRectangle(20, 320, 316, 42, 7);
        meter(g, 328, "IN", shownInput, false);
        meter(g, 347, "OUT", shownOutput, false);
        g.setColour(juce::Colour(0xff564247));
        g.drawHorizontalLine(367, 23.0f, 333.0f);
    }

    void addToggle(juce::ToggleButton& button, std::unique_ptr<State::ButtonAttachment>& attachment,
                   const char* id, const char* label)
    {
        button.setButtonText(label);
        button.setColour(juce::ToggleButton::textColourId, juce::Colour(ink));
        button.setColour(juce::ToggleButton::tickColourId, juce::Colour(accent));
        addAndMakeVisible(button);
        attachment = std::make_unique<State::ButtonAttachment>(state, id, button);
    }

    void drawDetection(juce::Graphics& g)
    {
        const juce::Rectangle<float> area(24, 192, float(getWidth() - 48), 118);
        g.setColour(juce::Colour(panel));
        g.fillRoundedRectangle(area, 8);
        const auto y = [&] (float db)
        {
            return area.getBottom() - 5 - juce::jlimit(0.0f, 1.0f, (db + 80) / 80) * (area.getHeight() - 10);
        };
        g.setColour(juce::Colour(track).brighter(.5f));
        const float line = y(state.getRawParameterValue("threshold")->load());
        g.drawHorizontalLine(int(line), area.getX() + 7, area.getRight() - 7);
        juce::Path curve;
        for (size_t i = 0; i < history.size(); ++i)
        {
            const float x = area.getX() + 8 + float(i) * (area.getWidth() - 16) / float(history.size() - 1);
            const float point = y(history[(historyIndex + i) % history.size()]);
            if (i == 0) curve.startNewSubPath(x, point);
            else curve.lineTo(x, point);
        }
        g.setColour(juce::Colour(accent));
        g.strokePath(curve, juce::PathStrokeType(1.6f));
    }

    void meter(juce::Graphics& g, int y, const char* label, float db, bool reduction)
    {
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(juce::Colour(muted));
        g.drawText(label, 37, y - 3, 79, 15, juce::Justification::centredLeft);
        const juce::Rectangle<float> area{120, float(y), float(getWidth() - 220), 8};
        g.setColour(juce::Colour(track));
        g.fillRoundedRectangle(area, 4);
        const float amount = reduction ? juce::jlimit(0.0f, 1.0f, db / 24)
                                       : juce::jlimit(0.0f, 1.0f, (db + 60) / 60);
        g.setColour(juce::Colour(!reduction && db > 0 ? 0xffff7e84 : accent));
        if (amount > 0) g.fillRoundedRectangle(area.withWidth(area.getWidth() * amount), 4);
        const auto value = reduction ? juce::String(db, 1) + " dB"
            : db <= -60 ? juce::String("-inf") : juce::String(db, 1) + " dBFS";
        g.setColour(juce::Colour(ink));
        g.drawText(value, getWidth() - 94, y - 3, 58, 15, juce::Justification::centredRight);
    }

    void timerCallback() override
    {
        const auto db = [] (float peak) { return juce::Decibels::gainToDecibels(peak, -60.0f); };
        shownInput = std::max(shownInput - 1.5f, db(levels.input.exchange(0)));
        shownOutput = std::max(shownOutput - 1.5f, db(levels.output.exchange(0)));
        shownReduction = std::max(shownReduction - 0.7f, levels.reduction.exchange(0));
        if constexpr (effect == 3)
        {
            const float detected = juce::Decibels::gainToDecibels(levels.detection.exchange(0), -80.0f);
            shownDetection = std::max(shownDetection - 1.5f, detected);
            history[historyIndex] = detected;
            historyIndex = (historyIndex + 1) % history.size();
            repaint(24, 87, getWidth() - 48, 224);
        }
        else if constexpr (effect == 4)
            repaint(20, 320, 316, 42);
        else
            repaint();
    }

    State& state;
    Activity& levels;
    DialStyle style;
    std::vector<std::unique_ptr<Control>> controls;
    juce::TextButton tap;
    double lastTap = 0;
    juce::ComboBox division;
    juce::ToggleButton ping, preRead;
    std::unique_ptr<State::ComboBoxAttachment> syncAttachment;
    std::unique_ptr<State::ButtonAttachment> pingAttachment, preReadAttachment;
    std::array<float, 120> history{};
    size_t historyIndex = 0;
    float shownDetection = -60;
    float shownInput = -60, shownOutput = -60, shownReduction = 0;
};
}
