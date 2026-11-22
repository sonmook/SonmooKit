class FetEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    FetEditor(juce::AudioProcessor& processor, State& parameters, Activity& levels)
        : AudioProcessorEditor(processor), state(parameters), activity(levels)
    {
        dial("input", "Input", {35, 97, 154, 151});
        dial("output", "Output", {202, 97, 154, 151});
        dial("attack", "Attack", {383, 92, 75, 70});
        dial("release", "Release", {383, 188, 75, 70});
        dial("mix", "Mix", {646, 294, 85, 66});
        dial("trim", "Trim", {746, 294, 85, 66});
        choice("ratio", {"4:1", "8:1", "12:1", "20:1", "All"}, 482, 91, 58, 32, true);
        choice("meter", {"GR", "IN", "OUT"}, 780, 92, 56, 34, true);
        choice("analog", {"Off", "50 Hz", "60 Hz"}, 198, 323, 51, 26, false);
        choice("character", {"Smooth", "Bright"}, 385, 323, 69, 26, false);
        toggle("makeup", "Auto makeup", {34, 321, 125, 27});
        toggle("bypassComp", "Comp off", {769, 210, 91, 27});
        setSize(860, 378);
        startTimerHz(45);
    }

    ~FetEditor() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        g.setGradientFill({juce::Colour(0xff353747), 0, 0, juce::Colour(0xff121420), 0, 378, false});
        g.fillAll();
        g.setColour(juce::Colour(0xff202334));
        g.fillRoundedRectangle(21, 59, 818, 217, 10);
        g.fillRoundedRectangle(21, 284, 818, 79, 10);
        SonmooBrand::draw(g, 26, 17, 23);
        label(g, "FETStrike", 590, 12, 242, 33, 27, juce::Justification::centredRight);
        label(g, "INPUT", 35, 72, 154, 22);
        label(g, "OUTPUT", 202, 72, 154, 22);
        label(g, "ATTACK", 383, 70, 75, 21);
        label(g, "RELEASE", 383, 168, 75, 20);
        label(g, "RATIO", 474, 69, 75, 21);
        label(g, "METER", 770, 69, 75, 21);
        label(g, "ANALOG", 198, 297, 148, 22);
        label(g, "CHARACTER", 385, 297, 138, 22);
        label(g, "MIX", 645, 280, 86, 21);
        label(g, "TRIM", 744, 280, 88, 21);
        drawMeter(g);
    }

private:
    struct Dial
    {
        juce::Slider control;
        std::unique_ptr<State::SliderAttachment> link;
    };
    struct Toggle
    {
        juce::ToggleButton control;
        std::unique_ptr<State::ButtonAttachment> link;
    };
    struct Selector
    {
        juce::RangedAudioParameter* parameter{};
        int value = 0;
        juce::TextButton button;
    };
    static constexpr auto green = 0xff83dfce;
    static constexpr auto paper = 0xffecd8af;
    static constexpr auto darkInk = 0xff322335;

    void dial(const char* id, const char* name, juce::Rectangle<int> area)
    {
        auto item = std::make_unique<Dial>();
        auto& s = item->control;
        s.setTitle(name);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 22);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(green));
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff46495d));
        s.setColour(juce::Slider::thumbColourId, juce::Colour(0xffecf6ff));
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff171925));
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff41465a));
        s.setBounds(area);
        addAndMakeVisible(s);
        item->link = std::make_unique<State::SliderAttachment>(state, id, s);
        const bool time = juce::String(id) == "attack" || juce::String(id) == "release";
        s.textFromValueFunction = [time, percent = juce::String(id) == "mix"] (double x)
        {
            return juce::String(x, 1) + (time ? "" : percent ? " %" : " dB");
        };
        s.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue(); };
        s.updateText();
        dials.push_back(std::move(item));
    }

    void toggle(const char* id, const char* name, juce::Rectangle<int> area)
    {
        auto item = std::make_unique<Toggle>();
        item->control.setButtonText(name);
        item->control.setColour(juce::ToggleButton::tickColourId, juce::Colour(green));
        item->control.setBounds(area);
        addAndMakeVisible(item->control);
        item->link = std::make_unique<State::ButtonAttachment>(state, id, item->control);
        toggles.push_back(std::move(item));
    }

    void choice(const char* id, const juce::StringArray& labels, int x, int y, int w, int h, bool vertical)
    {
        auto* parameter = state.getParameter(id);
        for (int i = 0; i < labels.size(); ++i)
        {
            auto item = std::make_unique<Selector>();
            item->parameter = parameter;
            item->value = i;
            item->button.setButtonText(labels[i]);
            item->button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2c3144));
            item->button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff317d73));
            item->button.setBounds(x + (vertical ? 0 : i * (w + 4)), y + (vertical ? i * (h + 2) : 0), w, h);
            item->button.onClick = [parameter, i]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(float(i)));
                parameter->endChangeGesture();
            };
            addAndMakeVisible(item->button);
            selectors.push_back(std::move(item));
        }
    }

    static void label(juce::Graphics& g, const juce::String& text, int x, int y, int w, int h,
                      float size = 11.5f, juce::Justification pos = juce::Justification::centred)
    {
        g.setColour(juce::Colour(0xffe8eef9));
        g.setFont(juce::FontOptions(size, juce::Font::bold));
        g.drawText(text, x, y, w, h, pos);
    }

    void drawMeter(juce::Graphics& g)
    {
        const juce::Rectangle<float> area(555, 97, 204, 147);
        g.setColour(juce::Colour(0xff0f1118));
        g.fillRoundedRectangle(area.expanded(4), 11);
        g.setGradientFill({juce::Colour(paper), area.getTopLeft(),
                           juce::Colour(0xffc7ad85), area.getBottomRight(), false});
        g.fillRoundedRectangle(area, 8);
        const juce::Point<float> pivot(657, 222);
        const float leftAngle = -.92f, rightAngle = .92f;
        const auto angle = [=] (float pos) { return leftAngle + pos * (rightAngle - leftAngle); };
        const int mode = int(state.getRawParameterValue("meter")->load(std::memory_order_relaxed));
        const std::array<int, 7> gr{-30, -20, -12, -8, -4, -2, 0}, level{-36, -24, -18, -12, -6, 0, 6};
        for (int i = 0; i <= 18; ++i)
        {
            const auto a = angle(i / 18.f);
            const bool major = i % 3 == 0;
            auto p1 = pivot.getPointOnCircumference(92, a);
            auto p2 = pivot.getPointOnCircumference(major ? 80.f : 85.f, a);
            g.setColour(juce::Colour(i >= 15 && mode != 0 ? 0xffa84634 : darkInk));
            g.drawLine(juce::Line<float>(p1, p2), major ? 1.6f : .8f);
            if (major)
            {
                const auto t = pivot.getPointOnCircumference(105, a);
                g.setFont(juce::FontOptions(10.f, juce::Font::bold));
                g.drawText(juce::String((mode == 0 ? gr : level)[i / 3]), int(t.x) - 12, int(t.y) - 5,
                           24, 11, juce::Justification::centred);
            }
        }
        const float current = mode == 0 ? -shownGr : mode == 1 ? shownIn : shownOut;
        const auto& stops = mode == 0 ? gr : level;
        float position = 1.f;
        for (int i = 0; i < 6; ++i)
            if (current <= stops[i + 1])
            {
                position = (i + juce::jlimit(0.f, 1.f, (current - stops[i]) / (stops[i + 1] - stops[i]))) / 6;
                break;
            }
        g.setColour(juce::Colour(0xff793747));
        g.drawLine(juce::Line<float>(pivot, pivot.getPointOnCircumference(87, angle(position))), 2.2f);
        g.setColour(juce::Colour(darkInk));
        g.fillEllipse(pivot.x - 4, pivot.y - 4, 8, 8);
        g.setFont(juce::FontOptions(13.f, juce::Font::bold));
        const juce::String reading = mode == 0 ? juce::String(current, 1) + " dB GR"
            : current <= -60 ? juce::String("-inf dBFS") : juce::String(current, 1) + " dBFS";
        g.drawText(reading, 567, 188, 180, 19, juce::Justification::centred);
    }

    void timerCallback() override
    {
        const auto db = [] (float x) { return juce::Decibels::gainToDecibels(x, -60.f); };
        shownIn = std::max(shownIn - 2.0f, db(activity.input.exchange(0, std::memory_order_relaxed)));
        shownOut = std::max(shownOut - 2.0f, db(activity.output.exchange(0, std::memory_order_relaxed)));
        shownGr = std::max(shownGr - .6f, activity.reduction.exchange(0, std::memory_order_relaxed));
        for (auto& s : selectors)
            s->button.setToggleState(juce::roundToInt(s->parameter->convertFrom0to1(s->parameter->getValue())) == s->value,
                                     juce::dontSendNotification);
        repaint(549, 91, 216, 159);
    }

    State& state;
    Activity& activity;
    std::vector<std::unique_ptr<Dial>> dials;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<std::unique_ptr<Selector>> selectors;
    float shownIn = -60, shownOut = -60, shownGr = 0;
};
