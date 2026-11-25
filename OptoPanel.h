#pragma once
class OptoSkin final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider&) override
    {
        const float r=juce::jmax(1.f,.5f*std::min(w,h)-9.f);
        const juce::Point<float> centre(x+w*.5f,y+h*.5f);
        const auto disk=juce::Rectangle<float>(2*r,2*r).withCentre(centre);
        for(int i=0;i<=20;++i)
        {
            const float angle=start+(end-start)*i/20;
            g.setColour(juce::Colour(0xff454d49));
            g.drawLine({centre.getPointOnCircumference(r,angle),centre.getPointOnCircumference(r+(i%2?3:6),angle)},1.4f);
        }
        g.setColour(juce::Colour(0xff646760));g.fillEllipse(disk.translated(1,3));
        g.setGradientFill({juce::Colour(0xff535957),disk.getTopLeft(),juce::Colour(0xff111919),disk.getBottomRight(),false});
        g.fillEllipse(disk);g.setColour(juce::Colour(0xffaab1a5));g.drawEllipse(disk.reduced(1),1);
        const float angle=start+(end-start)*position;
        g.setColour(juce::Colour(0xff87edce));
        g.drawLine({centre.getPointOnCircumference(r*.32f,angle),centre.getPointOnCircumference(r*.86f,angle)},r>35.f ? 4.f : 3.f);
    }
};
class OptoPanel final : public juce::AudioProcessorEditor,private juce::Timer
{
public:
    OptoPanel(juce::AudioProcessor& proc,State& s,OptoLevel& dsp):AudioProcessorEditor(proc),state(s),engine(dsp)
    {
        setLookAndFeel(&skin);
        const char* ids[]{"gain","peak","focus","mix","trim"};
        const char* names[]{"Gain","Peak Reduction","HiFreq","Mix (%)","Trim (dB)"};
        const juce::Rectangle<int> areas[]{{72,105,160,150},{604,105,160,150},{24,328,120,84},{554,328,120,84},{683,328,120,84}};
        for(int i=0;i<5;++i)
        {
            auto& knob=dials[i];addAndMakeVisible(knob);knob.setBounds(areas[i]);knob.setTitle(names[i]);
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow,false,66,20);
            knob.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff263330));
            links[i]=std::make_unique<State::SliderAttachment>(state,ids[i],knob);
            knob.setNumDecimalPlacesToDisplay(i<2?2:1);
        }
        auto menu=[&](int i,const char* id,juce::StringArray labels,juce::Rectangle<int> bounds)
        {
            auto& box=menus[i];box.addItemList(labels,1);addAndMakeVisible(box);box.setTitle(id);box.setBounds(bounds);
            box.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff253331));
            picks[i]=std::make_unique<State::ComboBoxAttachment>(state,id,box);
        };
        menu(0,"mode",{"Compress","Limit"},{174,343,110,30});
        menu(1,"analog",{"Off","50 Hz","60 Hz"},{307,343,91,30});
        menu(2,"meter",{"Input","GR","Output"},{435,234,96,26});
        automatic.setButtonText("Auto makeup");automatic.setColour(juce::ToggleButton::textColourId,juce::Colour(0xff152522));
        automatic.setColour(juce::ToggleButton::tickColourId,juce::Colour(0xff17765f));
        addAndMakeVisible(automatic);automatic.setBounds(414,342,128,31);
        autoLink=std::make_unique<State::ButtonAttachment>(state,"auto",automatic);
        setSize(836,430);startTimerHz(40);
    }
    ~OptoPanel() override {stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override
    {
        using namespace juce;
        auto label=[&](String text,int x,int y,int w,int h,float size,Colour ink)
        {g.setColour(ink);g.setFont(FontOptions(size,Font::bold));g.drawText(text,x,y,w,h,Justification::centred);};
        const Colour ink(0xff273431),white(0xffeff2e8);
        g.setGradientFill({Colour(0xffe5e6d9),0,52,Colour(0xffa3aaa0),0,430,false});g.fillAll();
        g.setColour(Colour(0xff182522));g.fillRect(0,0,836,51);SonmooBrand::draw(g,24,13,24);
        label("OptoLevel 2A",555,5,256,39,27,white);
        label("GAIN",72,72,160,27,20,ink);label("PEAK REDUCTION",582,72,204,27,20,ink);
        g.setColour(Colour(0xff34403c));g.fillRoundedRectangle(280,70,276,157,8);
        g.setGradientFill({Colour(0xffffedbf),0,80,Colour(0xffd4b57c),0,220,false});
        g.fillRoundedRectangle(286,76,264,145,4);
        const Point<float> pivot(418,211);
        const int mode=menus[2].getSelectedId();
        const bool gr=mode==2;
        const std::array<int,7> ticks=gr?std::array<int,7>{-24,-16,-10,-6,-3,-1,0}:std::array<int,7>{-20,-10,-7,-5,-3,0,3};
        const float level=gr ? -engine.gr.load() :
            Decibels::gainToDecibels((mode==1?engine.inVU:engine.outVU).load()*1.5707963f,-80.f)+18.f;
        float fraction=0;
        for(int i=0;i<7;++i)
        {
            const float a=-.96f+1.92f*i/6.f;
            auto t=pivot.getPointOnCircumference(119,a);
            label(String(ticks[i]),(int)t.x-14,(int)t.y-7,28,15,12,ink);
            g.setColour(ink);g.drawLine({pivot.getPointOnCircumference(96,a),pivot.getPointOnCircumference(106,a)},1.6f);
            if(i<6)fraction+=jlimit(0.f,1.f,(level-ticks[i])/(ticks[i+1]-ticks[i]))/6.f;
        }
        label(gr?"GAIN REDUCTION":"VU LEVEL",288,234,142,26,13,ink);
        g.setColour(Colour(0xffa44835));g.drawLine({pivot,pivot.getPointOnCircumference(106,-.96f+1.92f*fraction)},2.4f);
        g.setColour(ink);g.fillEllipse(413,206,10,10);
        label("OPTICAL LEVELING AMPLIFIER",248,263,340,20,13,ink);
        g.setColour(Colour(0xff808c83));g.drawHorizontalLine(289,24,812);
        label("HiFreq",24,297,120,20,13,ink);label("MODE",174,297,110,20,13,ink);
        label("ANALOG",307,297,91,20,13,ink);label("MIX  %",554,297,120,20,13,ink);label("TRIM  dB",683,297,120,20,13,ink);
    }
private:
    void timerCallback() override {repaint(280,70,276,190);}
    State& state;OptoLevel& engine;OptoSkin skin;
    std::array<juce::Slider,5> dials;std::array<juce::ComboBox,3> menus;juce::ToggleButton automatic;
    std::array<std::unique_ptr<State::SliderAttachment>,5> links;
    std::array<std::unique_ptr<State::ComboBoxAttachment>,3> picks;
    std::unique_ptr<State::ButtonAttachment> autoLink;
};
