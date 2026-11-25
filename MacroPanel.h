class MacroDial final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float fraction,
                          float from,float to,Slider& slider) override
    {
        const float radius=jmin(w,h)*.45f;
        const Point<float> centre(x+w*.5f,y+h*.5f);
        const float angle=from+fraction*(to-from);
        const auto tint=slider.findColour(Slider::rotarySliderFillColourId);
        for(int tick=0;tick<=20;++tick){
            float a=from+(to-from)*tick/20.f;
            auto outer=centre.getPointOnCircumference(radius,a);
            auto inner=centre.getPointOnCircumference(radius-(tick%2?3.f:6.f),a);
            g.setColour(a<=angle?tint:Colour(0xff52616b));g.drawLine({outer,inner},1.4f);
        }
        const auto disk=Rectangle<float>(radius*1.58f,radius*1.58f).withCentre(centre);
        g.setColour(Colour(0xff0a1015));g.fillEllipse(disk.expanded(4).translated(0,2));
        g.setGradientFill({Colour(0xff566671),disk.getTopLeft(),Colour(0xff18252e),disk.getBottomRight(),false});
        g.fillEllipse(disk);
        g.setColour(Colour(0xff77838a));g.drawEllipse(disk.reduced(1),1.f);
        g.setColour(Colour(0xff27353e));g.fillEllipse(disk.reduced(8));
        g.setColour(tint);g.drawLine({centre.getPointOnCircumference(radius*.49f,angle),
            centre.getPointOnCircumference(radius*.70f,angle)},4.f);
    }
};
class MacroPanel final : public AudioProcessorEditor, private Timer {
public:
    explicit MacroPanel(Macro& m) : AudioProcessorEditor(m), p(m)
    {
        dial.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        dial.setTextBoxStyle(Slider::TextBoxBelow,false,74,26);
        dial.setColour(Slider::rotarySliderFillColourId,accent());
        dial.setColour(Slider::rotarySliderOutlineColourId,Colour(0xff303e46));
        dial.setColour(Slider::textBoxBackgroundColourId,Colour(0xff17252d));
        dial.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);
        dial.setDoubleClickReturnValue(true,initial);
        dial.setLookAndFeel(&dialStyle);
        dial.setTitle(label); addAndMakeVisible(dial); dial.setBounds(58,102,220,202);
        link=std::make_unique<State::SliderAttachment>(p.state,"amount",dial);
        if constexpr(model==Press) menu(0,"input","Input");
        if constexpr(model==Sweep) menu(0,"res","Resonance");
        if constexpr(model==Dip){menu(0,"rate","Rate");menu(1,"offset","Offset");}
        setSize(336,model==Press||model==Sweep||model==Dip?424:366);
        startTimerHz(30);
    }
    ~MacroPanel() override {stopTimer();dial.setLookAndFeel(nullptr);}
    void paint(Graphics& g) override
    {
        g.setGradientFill({Colour(0xff20313b),0,0,Colour(0xff0e151c),336,380,false});g.fillAll();
        SonmooBrand::draw(g,18,12,21);
        g.setColour(Colours::white);g.setFont(FontOptions(30.f,Font::bold));
        g.drawFittedText(p.getName(),12,39,312,39,Justification::centred,1,0.75f);
        g.setColour(accent());g.setFont(FontOptions(14.f,Font::bold));g.drawText(label,20,82,296,20,Justification::centred);
        for(int i=0;i<2;++i)if(menus[i].isVisible()){
            g.setColour(Colour(0xffbdcbd3));g.setFont(13.f);
            g.drawText(menus[i].getTitle(),menus[i].getX(),314,menus[i].getWidth(),17,Justification::centredLeft);
        }
        int y=getHeight()-30;
        g.setColour(Colour(0xffb5c6cf));g.setFont(12.f);g.drawText("OUT",18,y-5,32,16,Justification::centredLeft);
        g.setColour(Colour(0xff32414b));g.fillRoundedRectangle(55.f,float(y),201.f,6.f,3.f);
        g.setColour(accent());g.fillRoundedRectangle(55.f,float(y),201*jlimit(0.f,1.f,(db+60)/60),6.f,3.f);
        g.drawText(db<=-60?String("-inf"):String(db,1)+" dB",259,y-6,60,18,Justification::centredRight);
    }
private:
    Colour accent() const {constexpr uint32 colours[]{0xffffd47b,0xffe79c78,0xff84d4ae,0xffec9693,0xffa3bcff,0xff74d9df,0xffbba3f5};return Colour(colours[model-Air]);}
    void menu(int i,const char* id,const char* title){
        auto& box=menus[i];
        auto* param=dynamic_cast<AudioParameterChoice*>(p.state.getParameter(id));
        box.addItemList(param->choices,1);box.setTitle(title);
        box.setBounds(model==Dip?22+151*i:60,336,model==Dip?140:216,28);
        addAndMakeVisible(box);choices[i]=std::make_unique<State::ComboBoxAttachment>(p.state,id,box);
    }
    void timerCallback() override {
        db=jmax(-60.f,jmax(db-1.2f,Decibels::gainToDecibels(p.meter.exchange(0.f),-60.f)));
        repaint(0,getHeight()-40,getWidth(),40);
    }
    Macro& p; MacroDial dialStyle; Slider dial; std::array<ComboBox,2> menus;
    std::unique_ptr<State::SliderAttachment> link;
    std::array<std::unique_ptr<State::ComboBoxAttachment>,2> choices;
    float db=-60;
};
