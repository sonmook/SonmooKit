class DynamicEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    DynamicEditor(juce::AudioProcessor& processor, State& parameters, DynamicDisplay& data)
        : AudioProcessorEditor(processor), state(parameters), display(data)
    {
        for (int i = 0; i < dynamicBands; ++i)
        {
            tabs[i].setButtonText(juce::String(i + 1));
            tabs[i].setColour(juce::TextButton::buttonColourId, surface);
            tabs[i].setColour(juce::TextButton::buttonOnColourId, colours[i % colours.size()].darker(.5f));
            tabs[i].onClick = [this, i] { select(i); };
            if constexpr (prism) tabs[i].setBounds(31 + (i % 12) * 38, 376 + (i / 12) * 24, 36, 22);
            else tabs[i].setBounds(39 + i * 74, 383, 64, 28);
            addAndMakeVisible(tabs[i]);
        }
        configureToggle(hp, "HP", {38, 435, 49, 24});
        configureToggle(lp, "LP", {345, 435, 49, 24});
        configureToggle(autoRelease, "Adaptive release", {710, 435, 146, 24});
        spectrumSource.addItemList(spectrumChoices(), 1);
        spectrumSource.setBounds(696, 23, 98, 27);
        spectrumSource.setTooltip("Spectrum source");
        addAndMakeVisible(spectrumSource);
        spectrumLink = std::make_unique<State::ComboBoxAttachment>(state, "spectrumSource", spectrumSource);
        configureToggle(spectrumButton, "Spectrum", {808, 24, 109, 26});
        spectrumButton.setToggleState(true, juce::dontSendNotification);
        spectrumButton.onClick = [this] { display.listening.store(spectrumButton.getToggleState()); };
        configureToggle(enabled, "Band active", {38, 478, 112, 25});
        configureToggle(solo, "Listen to key", {751, 478, 136, 25});
        solo.onClick = [this] { display.listenBand.store(solo.getToggleState() ? selected : -1); };
        hpLink = std::make_unique<State::ButtonAttachment>(state, "hp", hp);
        lpLink = std::make_unique<State::ButtonAttachment>(state, "lp", lp);
        autoLink = std::make_unique<State::ButtonAttachment>(state, "autoRelease", autoRelease);
        control(hpFreq, "HP cutoff", {93, 434, 112, 28}, false);
        control(lpFreq, "LP cutoff", {400, 434, 115, 28}, false);
        for (int i = 0; i < 2; ++i)
        {
            slopes[i].addItemList({"6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct"}, 1);
            slopes[i].setBounds(i == 0 ? 208 : 515, 435, 100, 26);
            addAndMakeVisible(slopes[i]);
            slopeLinks[i] = std::make_unique<State::ComboBoxAttachment>(state, i == 0 ? "hpSlope" : "lpSlope", slopes[i]);
        }
        control(mix, "Mix", {538, 380, 133, 34}, false);
        control(output, "Output", {725, 380, 146, 34}, false);
        hpFreqLink = std::make_unique<State::SliderAttachment>(state, "hpFreq", hpFreq);
        lpFreqLink = std::make_unique<State::SliderAttachment>(state, "lpFreq", lpFreq);
        mixLink = std::make_unique<State::SliderAttachment>(state, "mix", mix);
        outputLink = std::make_unique<State::SliderAttachment>(state, "output", output);
        const std::array<juce::StringArray, 4> choices{
            shapes(), placements(),
            juce::StringArray{"Internal", "External"}, detectionModes()};
        for (int i = 0; i < 4; ++i)
        {
            menus[i].addItemList(choices[i], 1);
            menus[i].setColour(juce::ComboBox::backgroundColourId, background);
            menus[i].setColour(juce::ComboBox::outlineColourId, line);
            menus[i].setBounds(40 + i * 175, 658, 155, 28);
            addAndMakeVisible(menus[i]);
        }
        for (int i = 0; i < 7; ++i)
            control(knobs[i], labels[i], {37 + i * 123, 540, 112, 88}, true);
        if constexpr (prism) {
            engine.addItemList({"Spectral ready","Live"},1);
            character.addItemList({"Clean","Soft","Dense"},1);
            engine.setBounds(365,23,130,27); character.setBounds(510,23,100,27);
            addAndMakeVisible(engine); addAndMakeVisible(character);
            engineLink = std::make_unique<State::ComboBoxAttachment>(state,"engine",engine);
            characterLink = std::make_unique<State::ComboBoxAttachment>(state,"character",character);
        }
        select(0);
        spectrum.fill(-100);
        if constexpr (prism) {
            for (int i=0;i<3;++i) {
                matchButtons[i].setButtonText(i==0 ? "Capture source" : i==1 ? "Capture reference" : "Apply match");
                matchButtons[i].setBounds(28+i*145,708,136,28);
                addAndMakeVisible(matchButtons[i]);
            }
            for (int i=0;i<2;++i) matchButtons[i].onClick=[this,i] {
                capturing=i; frames=0; profiles[i].fill(0); captured[i]=false;
                spectrumButton.setToggleState(true,juce::dontSendNotification); display.listening.store(true);
                matchButtons[2].setEnabled(false); matchStatus="Play audio to capture"; repaint();
            };
            matchButtons[2].setEnabled(false); matchButtons[2].onClick=[this]{ applyMatch(); };
        }
        setSize(936, prism ? 750 : 710);
        display.listening.store(true);
        startTimerHz(60);
    }
    ~DynamicEditor() override
    {
        stopTimer();
        endDrag();
        display.listening.store(false);
        display.listenBand.store(-1);
    }

    void paint(juce::Graphics& g) override
    {
        g.setGradientFill({juce::Colour(0xff233351), 0, 0, juce::Colour(0xff101727), 0, 710, false});
        g.fillAll();
        SonmooBrand::draw(g, 24, 8, 22);
        if constexpr (prism) {
            text(g,"ENGINE",365,4,130,16); text(g,"CHARACTER",510,4,100,16); text(g,"ANALYZER",696,4,100,16);
            text(g,matchStatus,473,709,430,25,13);
        }
        if constexpr (prism) text(g, "Double-click: add    /    Shift + drag: sketch    /    Right-click: remove    /    Scroll: Q", 236, 50, 635, 22, 12, dim);
        text(g, JucePlugin_Name, 24, 28, 255, 35, 30, ink);
        g.setColour(juce::Colour(0xff101a2a));
        g.fillRoundedRectangle(20, 75, 896, 286, 10);
        if (g.clipRegionIntersects({20, 75, 896, 286})) graphContent(g);
        if (!sketch.empty()) {
            juce::Path path; path.startNewSubPath(sketch.front());
            for (auto p : sketch) path.lineTo(p);
            g.setColour(ink); g.strokePath(path,juce::PathStrokeType(2));
        }
        g.setColour(juce::Colour(0xff1c293f));
        g.fillRoundedRectangle(20, 373, 896, 97, 10);
        g.fillRoundedRectangle(20, 471, 896, 224, 10);
        text(g, "MIX", 499, 385, 39, 24);
        text(g, "OUT", 683, 385, 41, 24);
        text(g, "BAND " + juce::String(selected + 1), 180, 479, 105, 24, 13, colours[selected % colours.size()]);
        const float change = display.activity[selected].load(std::memory_order_relaxed);
        const float key = juce::Decibels::gainToDecibels(display.detector[selected].load(std::memory_order_relaxed), -90.f);
        text(g, "DYNAMIC  " + juce::String(change, 1) + " dB", 306, 479, 145, 24, 13, ink);
        text(g, "KEY  " + juce::String(key, 1) + " dBFS", 463, 479, 145, 24);
        for (int i = 0; i < 7; ++i) text(g, labels[i], 44 + i * 123, 514, 111, 24);
        for (int i = 0; i < 4; ++i) text(g, menuLabels[i], 42 + i * 175, 634, 150, 21);
        if (val("source") > .5f && !display.externalAvailable.load())
            text(g, "Connect sidechain", 752, 657, 148, 27, 12, colours[2]);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!plot.expanded(8).contains(e.position)) return;
        if constexpr (prism) if (e.mods.isShiftDown() && !e.mods.isPopupMenu()) {
            sketch.clear(); sketch.push_back(e.position); return;
        }
        int target = -1;
        float dist = 20;
        for (int i = 0; i < dynamicBands; ++i)
        {
            if constexpr (prism) if (value(i, "enabled") < .5f) continue;
            const auto d = node(i).getDistanceFrom(e.position);
            if (d < dist) { dist = d; target = i; }
        }
        if (target < 0) return;
        if constexpr (prism) if (e.mods.isPopupMenu()) { write(target, "enabled", 0); return; }
        select(target);
        dragHz = state.getParameter(dynamicId(target, "freq"));
        dragGain = state.getParameter(dynamicId(target, "gain"));
        dragHz->beginChangeGesture();
        dragGain->beginChangeGesture();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (!prism || !plot.contains(e.position) || e.mods.isPopupMenu()) return;
        for (int i = 0; i < dynamicBands; ++i)
            if (value(i,"enabled") > .5f && node(i).getDistanceFrom(e.position) < 16) return;
        auto point=e.position;
        if (e.mods.isAltDown() && spectrumButton.getToggleState()) {
            const double rate=display.sampleRate.load();
            const int bin=juce::jlimit(1,fftSize/2,int(frequency(unit((point.x-plot.getX())/plot.getWidth()))*fftSize/rate));
            const int lo=std::max(1,int(bin*.8)), hi=std::min(fftSize/2,int(bin*1.25)+1);
            const auto peak=std::max_element(spectrum.begin()+lo,spectrum.begin()+hi+1);
            if (*peak>-90) point.x=px(float((peak-spectrum.begin())*rate/fftSize));
        }
        addAt(point);
    }
    void addAt(juce::Point<float> point)
    {
        for (int i = 0; i < dynamicBands; ++i)
        {
            if (value(i,"enabled") > .5f) continue;
            for (auto key : {"shape","channel","source","detect","range","threshold"}) write(i,key,0);
            write(i,"q",1); write(i,"attack",16); write(i,"release",160);
            write(i,"freq",frequency(unit((point.x-plot.getX())/plot.getWidth())));
            write(i,"gain",18-36*unit((point.y-plot.getY())/plot.getHeight()));
            write(i,"enabled",1); select(i); break;
        }
    }
    void sampleProfile()
    {
        if (capturing < 0) return;
        std::array<double,7> energy{};
        const double rate=display.sampleRate.load();
        for (int i=0;i<7;++i) {
            const double bin=matchHz[i]*fftSize/rate;
            const int lo=juce::jlimit(1,fftSize/2,int(bin/std::sqrt(2.)));
            const int hi=juce::jlimit(lo,fftSize/2,int(bin*std::sqrt(2.)));
            for (int k=lo;k<=hi;++k) energy[i]+=double(fftData[k])*fftData[k]/(hi-lo+1);
        }
        if (*std::max_element(energy.begin(),energy.end()) < .0001) return;
        for (int i=0;i<7;++i) profiles[capturing][i]+=energy[i];
        if (++frames<48) return;
        captured[capturing]=true; capturing=-1;
        matchButtons[2].setEnabled(captured[0] && captured[1]);
        matchStatus=captured[0] && captured[1] ? "Ready to match" : "Capture the other signal";
        repaint();
    }
    void applyMatch()
    {
        std::array<double,7> deltas{}; double average=0; int valid=0;
        const double floor=1.e-5*std::max(*std::max_element(profiles[0].begin(),profiles[0].end()),
                                          *std::max_element(profiles[1].begin(),profiles[1].end()));
        for (int i=0;i<7;++i) if (std::min(profiles[0][i],profiles[1][i])>floor) {
            deltas[i]=10*std::log10(profiles[1][i]/profiles[0][i]); average+=deltas[i]; ++valid;
        }
        int added=0;
        for (int i=0;i<7 && valid;++i) {
            if (std::min(profiles[0][i],profiles[1][i])<=floor) continue;
            const float db=float(std::clamp(deltas[i]-average/valid,-9.,9.));
            if (std::abs(db)<1) continue;
            int free=0; while(free<dynamicBands && value(free,"enabled")>.5f) ++free;
            if (free==dynamicBands) break;
            addAt({px(matchHz[i]),py(db)}); ++added;
        }
        matchStatus=juce::String(added)+" match bands added"; matchButtons[2].setEnabled(false); repaint();
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (!prism || !plot.contains(e.position)) return;
        for (int i=0; i<dynamicBands; ++i)
            if (value(i,"enabled")>.5f && node(i).getDistanceFrom(e.position)<18) {
                select(i); write(i,"q",val("q")*std::exp(wheel.deltaY*2)); break;
            }
    }
    void write(int i, const char* key, float v)
    {
        auto* param=state.getParameter(dynamicId(i,key));
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(v));
        param->endChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!sketch.empty()) {
            auto point = plot.getConstrainedPoint(e.position);
            if (point.x >= sketch.back().x + 8) sketch.push_back(point);
            repaint(); return;
        }
        if (!dragHz) return;
        const float hz = frequency(unit((e.position.x - plot.getX()) / plot.getWidth()));
        const float db = 18 - 36 * unit((e.position.y - plot.getY()) / plot.getHeight());
        dragHz->setValueNotifyingHost(dragHz->convertTo0to1(hz));
        dragGain->setValueNotifyingHost(dragGain->convertTo0to1(db));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (sketch.size() >= 2) {
            auto peak = sketch.front();
            int sign = 0;
            for (auto p : sketch) {
                const float height = plot.getCentreY()-p.y;
                const int next = std::abs(height)<10 ? 0 : height>0 ? 1 : -1;
                if (next != sign) { if (sign) addAt(peak); peak=p; sign=next; }
                if (std::abs(height)>std::abs(plot.getCentreY()-peak.y)) peak=p;
            }
            if (sign) addAt(peak);
        }
        sketch.clear(); endDrag(); repaint();
    }

private:
    static constexpr std::array<const char*, 7> labels{"Frequency", "Q", "Gain", "Range", "Threshold", "Attack", "Release"};
    static constexpr std::array<const char*, 4> menuKeys{"shape", "channel", "source", "detect"};
    static constexpr std::array<const char*, 4> menuLabels{"Filter shape", "Channel", "Key input", "Detection"};
    static float unit(float x) { return juce::jlimit(0.f, 1.f, x); }

    void configureToggle(juce::ToggleButton& button, const char* name, juce::Rectangle<int> bounds)
    {
        button.setButtonText(name);
        button.setBounds(bounds);
        button.setColour(juce::ToggleButton::textColourId, ink);
        button.setColour(juce::ToggleButton::tickColourId, colours[0]);
        addAndMakeVisible(button);
    }

    void control(juce::Slider& slider, const char* title, juce::Rectangle<int> bounds, bool rotary)
    {
        slider.setTitle(title);
        slider.setBounds(bounds);
        slider.setSliderStyle(rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(rotary ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight, false, 55, 21);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, background);
        slider.setColour(juce::Slider::textBoxOutlineColourId, line);
        slider.setColour(juce::Slider::textBoxTextColourId, ink);
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, line);
        slider.setColour(juce::Slider::trackColourId, colours[0]);
        addAndMakeVisible(slider);
    }

    void select(int i)
    {
        selected = i;
        for (int j = 0; j < dynamicBands; ++j) tabs[j].setToggleState(i == j, juce::dontSendNotification);
        solo.setToggleState(false, juce::dontSendNotification);
        display.listenBand.store(-1);
        activeLink.reset();
        activeLink = std::make_unique<State::ButtonAttachment>(state, dynamicId(i, "enabled"), enabled);
        for (int j = 0; j < 7; ++j)
        {
            knobLinks[j].reset();
            knobs[j].setColour(juce::Slider::rotarySliderFillColourId, colours[i % colours.size()]);
            knobs[j].setColour(juce::Slider::thumbColourId, colours[i % colours.size()]);
            knobLinks[j] = std::make_unique<State::SliderAttachment>(state, dynamicId(i, dynamicKeys[j]), knobs[j]);
        }
        for (int j = 0; j < 4; ++j)
        {
            menuLinks[j].reset();
            menuLinks[j] = std::make_unique<State::ComboBoxAttachment>(state, dynamicId(i, menuKeys[j]), menus[j]);
        }
        repaint();
    }
    void endDrag()
    {
        if (dragHz)
        {
            dragHz->endChangeGesture();
            dragGain->endChangeGesture();
            dragHz = dragGain = nullptr;
        }
    }
    float val(const char* field) const { return value(selected, field); }
    float value(int i, const char* field) const { return dynamicValue(state, dynamicId(i, field)); }
    static float frequency(float t) { return 16 * std::pow(21000.f / 16, t); }
    float px(float hz) const { return plot.getX() + plot.getWidth() * std::log(hz / 16) / std::log(21000.f / 16); }
    float py(float db) const { return plot.getY() + plot.getHeight() * unit(.5f - db / 36); }
    juce::Point<float> node(int i) const { return {px(value(i, "freq")), py(value(i, "gain"))}; }

    static void text(juce::Graphics& g, const juce::String& label, int x, int y, int w, int h,
                     float size = 11, juce::Colour tint = dim)
    {
        g.setColour(tint);
        g.setFont(juce::FontOptions(size, juce::Font::bold));
        g.drawText(label, x, y, w, h, juce::Justification::centredLeft);
    }

    void graphContent(juce::Graphics& g)
    {
        for (const float hz : {32.f, 63.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f})
        {
            const int x = int(px(hz));
            g.setColour(line);
            g.drawVerticalLine(x, plot.getY(), plot.getBottom());
            text(g, hz < 1000 ? juce::String(int(hz)) : juce::String(int(hz / 1000)) + "k", x - 8, 329, 35, 16);
        }
        for (int db = -18; db <= 18; db += 6)
        {
            const int y = int(py(float(db)));
            g.setColour(db == 0 ? line.brighter(.5f) : line);
            g.drawHorizontalLine(y, plot.getX(), plot.getRight());
            text(g, juce::String(db), 29, y - 8, 24, 15);
        }
        const double sr = display.sampleRate.load(std::memory_order_relaxed);
        if (sr != graphRate)
        {
            graphRate = sr;
            for (size_t i = 0; i < plotBins.size(); ++i)
            {
                const float hz = frequency(float(i) / float(plotBins.size() - 1));
                const auto phase = std::polar(1., -2 * pi * std::min(double(hz), sr * .499) / sr);
                const double bin = std::clamp(hz * fftSize / sr, 1., double(fftSize / 2));
                plotBins[i] = {phase, phase * phase, int(bin), float(bin - int(bin))};
            }
        }
        std::array<Biquad, dynamicBands + 4> baseline{}, live{};
        bool hasSide = false;
        for (int i = 0; i < dynamicBands; ++i)
        {
            if (value(i, "enabled") <= .5f) continue;
            const int mode = display.stereo.load(std::memory_order_relaxed) ? int(value(i, "channel")) : 0;
            if (mode == 2) hasSide = true;
            const int view = display.stereo.load(std::memory_order_relaxed) ? std::max(1,int(val("channel"))) : 1;
            if (mode != 0 && mode != view) continue;
            const float hz = value(i, "freq"), q = value(i, "q"), db = value(i, "gain");
            const int shape = int(value(i, "shape"));
            baseline[i] = filter(sr, hz, db, q, shape);
            live[i] = filter(sr, hz, std::clamp(db + (prism && int(value(i,"detect"))==2 ? 0.f : display.activity[i].load(std::memory_order_relaxed)), -36.f, 36.f), q, shape);
        }
        for (int i = 0; i < 2; ++i)
        {
            if (dynamicValue(state, i == 0 ? "hp" : "lp") > .5f)
            {
                const auto stages = cutStages(sr, dynamicValue(state, i == 0 ? "hpFreq" : "lpFreq"),
                    int(dynamicValue(state, i == 0 ? "hpSlope" : "lpSlope")) + 1, i == 0);
                for (int j = 0; j < 2; ++j)
                    baseline[dynamicBands + 2*i + j] = live[dynamicBands + 2*i + j] = stages[j];
            }
        }
        juce::Path staticCurve, dynamicCurve, spectrumCurve;
        for (size_t i = 0; i < plotBins.size(); ++i)
        {
            const auto& point = plotBins[i];
            const float t = float(i) / float(plotBins.size() - 1);
            const float x = plot.getX() + t * plot.getWidth();
            const auto magnitude = [&] (const auto& filters)
            {
                double product = 1;
                for (const auto& f : filters)
                {
                    if (f.b0 == 1 && f.b1 == f.a1 && f.b2 == f.a2) continue;
                    product *= std::norm(f.b0 + f.b1 * point.z + f.b2 * point.z2) /
                               std::norm(1. + f.a1 * point.z + f.a2 * point.z2);
                }
                return float(10 * std::log10(std::max(1e-20, product)));
            };
            const float first = spectrum[point.bin];
            const float next = spectrum[std::min(point.bin + 1, fftSize / 2)];
            const float level = first + point.fraction * (next - first);
            const float sy = plot.getBottom() - plot.getHeight() * unit((level + 100) / 100);
            const auto add = [i, x] (juce::Path& path, float y)
            {
                if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
            };
            add(staticCurve, py(magnitude(baseline)));
            add(dynamicCurve, py(magnitude(live)));
            add(spectrumCurve, sy);
        }
        if (spectrumButton.getToggleState())
        {
            g.setColour(dim.withAlpha(.4f));
            g.strokePath(spectrumCurve, juce::PathStrokeType(1));
        }
        g.setColour(dim.withAlpha(.72f));
        g.strokePath(staticCurve, juce::PathStrokeType(1));
        g.setColour(colours[selected % colours.size()]);
        g.strokePath(dynamicCurve, juce::PathStrokeType(2.3f));
        for (int i = 0; i < dynamicBands; ++i)
        {
            if constexpr (prism) if (value(i,"enabled") < .5f) continue;
            const auto p = node(i);
            const float radius = i == selected ? 13.f : 10.f;
            const auto tint = colours[i % colours.size()].withAlpha(value(i, "enabled") > .5f ? 1.f : .4f);
            g.setColour(background);
            g.fillEllipse(p.x - radius, p.y - radius, 2 * radius, 2 * radius);
            g.setColour(tint);
            g.drawEllipse(p.x - radius, p.y - radius, 2 * radius, 2 * radius, 1.8f);
            text(g, juce::String(i + 1), int(p.x) - (i >= 9 ? 8 : 4), int(p.y) - 8, 16, 16, 12, tint);
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            const juce::Rectangle<float> rail{float(873 + 18 * ch), 105, 11, 219};
            g.setColour(line);
            g.fillRoundedRectangle(rail, 4);
            const float db = juce::Decibels::gainToDecibels(peaks[ch], -60.f);
            const float h = rail.getHeight() * unit((db + 60) / 60);
            if (h > 0)
            {
                g.setColour(db > 0 ? juce::Colour(0xfff69089) : colours[0]);
                g.fillRoundedRectangle(rail.withY(rail.getBottom() - h).withHeight(h), 4);
            }
        }
        if (hasSide && val("channel") < 3)
            text(g, val("channel") == 2 ? "SIDE CURVE" : "MID CURVE", 65, 79, 110, 17);
    }

    void timerCallback() override
    {
        for (int ch = 0; ch < 2; ++ch)
            peaks[ch] = std::max(peaks[ch] * .84f, display.channelPeak[ch].exchange(0, std::memory_order_relaxed));
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float fall = float(std::clamp(now - lastFrameTime, 0., 100.) * .043);
        lastFrameTime = now;
        for (auto& bin : spectrum) bin = std::max(-100.f, bin - fall);
        if (display.ready.load(std::memory_order_acquire))
        {
            std::copy(display.frame.begin(), display.frame.end(), fftData.begin());
            std::fill(fftData.begin() + fftSize, fftData.end(), 0);
            display.ready.store(false, std::memory_order_release);
            window.multiplyWithWindowingTable(fftData.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform(fftData.data(), true);
            if constexpr (prism) sampleProfile();
            for (size_t i = 0; i < spectrum.size(); ++i)
                spectrum[i] = std::max(spectrum[i], juce::Decibels::gainToDecibels(fftData[i] * 4 / fftSize, -100.f));
        }
        repaint(20, 75, 896, 286);
        repaint(300, 478, 316, 25);
    }

    State& state;
    DynamicDisplay& display;
    const juce::Rectangle<float> plot{58, 105, 783, 219};
    std::array<juce::TextButton, dynamicBands> tabs;
    std::array<juce::Slider, 7> knobs;
    std::array<juce::ComboBox, 4> menus;
    juce::ToggleButton hp, lp, autoRelease, spectrumButton, enabled, solo;
    const std::array<float,7> matchHz{80,160,320,640,1280,2560,5120};
    std::array<juce::TextButton,3> matchButtons;
    std::array<std::array<double,7>,2> profiles{};
    std::array<bool,2> captured{};
    int capturing=-1, frames=0;
    juce::String matchStatus="Compare two passages by ear and spectrum";
    std::vector<juce::Point<float>> sketch;
    juce::ComboBox engine, character;
    std::unique_ptr<State::ComboBoxAttachment> engineLink, characterLink;
    juce::ComboBox spectrumSource;
    std::unique_ptr<State::ComboBoxAttachment> spectrumLink;
    std::array<juce::ComboBox, 2> slopes;
    std::array<std::unique_ptr<State::ComboBoxAttachment>, 2> slopeLinks;
    juce::Slider hpFreq, lpFreq, mix, output;
    std::array<std::unique_ptr<State::SliderAttachment>, 7> knobLinks;
    std::array<std::unique_ptr<State::ComboBoxAttachment>, 4> menuLinks;
    std::unique_ptr<State::ButtonAttachment> hpLink, lpLink, autoLink, activeLink;
    std::unique_ptr<State::SliderAttachment> hpFreqLink, lpFreqLink, mixLink, outputLink;
    juce::RangedAudioParameter *dragHz{}, *dragGain{};
    int selected = 0;
    std::array<float, 2> peaks{};
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> window{fftSize, juce::dsp::WindowingFunction<float>::hann, false};
    std::array<float, fftSize * 2> fftData{};
    std::array<float, fftSize / 2 + 1> spectrum{};
    struct PlotBin { std::complex<double> z, z2; int bin; float fraction; };
    std::array<PlotBin, 321> plotBins{};
    double graphRate = 0, lastFrameTime = 0;
};
