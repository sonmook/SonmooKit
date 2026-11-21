#pragma once

namespace {
constexpr auto background = 0xff101522;
constexpr auto panel = 0xff192234;
constexpr auto track = 0xff2c394f;
constexpr auto ink = 0xfff2f5fc;
constexpr auto muted = 0xffaab9ce;
constexpr auto accent = effect == 0 ? 0xff52dace : effect == 1 ? 0xffc2a2ff :
                        effect == 3 ? 0xff9ce1a5 : effect == 4 ? 0xfff3a1ac : 0xffffbe77;
constexpr bool hasReduction = effect == 1 || effect == 2 || effect == 3;

struct Activity
{
    std::atomic<float> input{0}, output{0}, reduction{0}, detection{0};
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
            const auto* sync = static_cast<juce::AudioParameterChoice*>(state.getParameter("sync"));
            division.addItemList(sync->choices, 1);
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
            dial("ratio", "Ratio", ":1");
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
            dial("release", "Release", " ms");
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
        else
            setSize(540, effect == 0 ? 432 : effect == 1 ? 397 : effect == 3 ? 411 : 282);
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
        g.fillAll(juce::Colour(background));
        g.setColour(juce::Colour(accent));
        g.fillRoundedRectangle(24, 24, 4, 43, 2);
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText("SonmooKit", 40, 19, 300, 21, juce::Justification::centredLeft);
        g.setColour(juce::Colour(ink));
        g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        g.drawText(getAudioProcessor()->getName(), 40, 37, 420, 35, juce::Justification::centredLeft);
        g.setColour(juce::Colour(panel));
        g.fillRoundedRectangle(24, 83, float(getWidth() - 48), float(meterHeight()), 10);
        meter(g, 95, "INPUT", shownInput, false);
        meter(g, 116, "OUTPUT", shownOutput, false);
        if constexpr (hasReduction) meter(g, 137, "REDUCTION", shownReduction, true);
        if constexpr (effect == 3)
        {
            meter(g, 158, "DETECTION", shownDetection, false);
            drawDetection(g);
        }
        if constexpr (effect == 0)
        {
            g.setColour(juce::Colour(muted));
            g.setFont(juce::FontOptions(13.0f));
            g.drawText("Tempo sync", 31, 390, 78, 26, juce::Justification::centredLeft);
        }
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
        const int columns = effect == 0 || effect == 2 ? 3 : 4;
        const int top = effect == 3 ? 254 : hasReduction ? 174 : 153;
        const int width = (getWidth() - 48) / columns;
        const auto count = static_cast<int>(controls.size());
        for (int i = 0; i < count; ++i)
        {
            auto& control = *controls[static_cast<size_t>(i)];
            const int inRow = std::min(columns, count - (i / columns) * columns);
            const int x = (getWidth() - inRow * width) / 2 + (i % columns) * width;
            const int y = top + (i / columns) * 110;
            control.label.setBounds(x, y, width, 20);
            control.slider.setBounds(x + 7, y + 20, width - 14, 82);
        }
        if constexpr (effect == 0)
        {
            division.setBounds(113, 390, 125, 26);
            ping.setBounds(260, 390, 115, 26);
        }
        if constexpr (effect == 3)
        {
            ping.setBounds(35, 368, 164, 26);
            preRead.setBounds(211, 368, 152, 26);
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

    void paintDrive(juce::Graphics& g)
    {
        g.setGradientFill({juce::Colour(0xff33292e), 0, 0,
                           juce::Colour(0xff141319), 0, 355, false});
        g.fillAll();
        g.setColour(juce::Colour(0xff564247));
        g.drawRoundedRectangle(9, 9, 338, 464, 13, 1);
        g.setColour(juce::Colour(accent));
        g.fillRoundedRectangle(28, 31, 4, 37, 2);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("SonmooKit", 43, 24, 240, 21, juce::Justification::centredLeft);
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

    static constexpr int meterHeight() { return effect == 3 ? 99 : hasReduction ? 78 : 57; }

    void drawDetection(juce::Graphics& g)
    {
        const juce::Rectangle<float> area(24, 190, float(getWidth() - 48), 52);
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
            repaint(24, 83, getWidth() - 48, 160);
        }
        else if constexpr (effect == 4)
            repaint(20, 320, 316, 42);
        else
            repaint(24, 83, getWidth() - 48, meterHeight());
    }

    State& state;
    Activity& levels;
    DialStyle style;
    std::vector<std::unique_ptr<Control>> controls;
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
