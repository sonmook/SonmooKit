#pragma once
class ArcPanel final : public juce::AudioProcessorEditor,private juce::Timer
{
    using Colour=juce::Colour;
    using Rect=juce::Rectangle<int>;
    using Point=juce::Point<float>;
    State& state;StereoArc& arc;
    std::array<juce::Slider,4> sliders;std::array<std::unique_ptr<State::SliderAttachment>,4> sliderLinks;
    std::array<juce::ComboBox,2> formats;std::array<std::unique_ptr<State::ComboBoxAttachment>,2> formatLinks;
    std::array<juce::TextButton,3> routing;std::array<std::unique_ptr<State::ButtonAttachment>,3> routingLinks;
    juce::TextButton reset{"Reset image"},clip{"Clear peak"};
    const Rect graph{200,80,426,226};
    static constexpr auto mint=0xff71e3c8, light=0xffecf2eb, muted=0xffa9bab8;
    const std::array<const char*,4> ids{"gain","width","asym","rotation"};
    juce::RangedAudioParameter* param(int i){return state.getParameter(ids[i]);}
    float value(int i) const{return state.getRawParameterValue(ids[i])->load();}
    void change(int i,float v){auto* p=param(i);p->setValueNotifyingHost(juce::jlimit(0.f,1.f,p->convertTo0to1(v)));}
    bool dragging=false, shape=false;
    float initialX=0,initialY=0;
    void endDrag()
    {
        if(dragging){param(shape?2:3)->endChangeGesture();param(shape?1:0)->endChangeGesture();dragging=false;}
    }
    void timerCallback() override
    {
        clip.setColour(juce::TextButton::buttonColourId,Colour(arc.clipped.load()?0xffa64236:0xff2b3b3b));
        repaint();
    }
public:
    ArcPanel(juce::AudioProcessor& proc,State& s,StereoArc& dsp):AudioProcessorEditor(proc),state(s),arc(dsp)
    {
        const std::array<Rect,4> areas{{{22,112,72,170},{99,112,72,170},{286,354,337,28},{286,393,337,28}}};
        const char* names[]{"Gain","Width","Asymmetry","Rotation"};
        for(int i=0;i<4;++i)
        {
            auto& f=sliders[i];addAndMakeVisible(f);f.setBounds(areas[i]);f.setTitle(names[i]);
            f.setSliderStyle(i<2?juce::Slider::LinearVertical:juce::Slider::LinearHorizontal);
            f.setTextBoxStyle(i<2?juce::Slider::TextBoxBelow:juce::Slider::TextBoxRight,false,58,23);
            f.setColour(juce::Slider::trackColourId,Colour(0xff408b80));f.setColour(juce::Slider::thumbColourId,Colour(mint));
            f.setColour(juce::Slider::textBoxOutlineColourId,Colour(0xff435855));
            sliderLinks[i]=std::make_unique<State::SliderAttachment>(state,ids[i],f);
            f.setNumDecimalPlacesToDisplay(i==1?2:1);f.setDoubleClickReturnValue(true,i==1?1.:0.);
        }
        for(int i=0;i<2;++i)
        {
            auto& box=formats[i];addAndMakeVisible(box);box.addItemList({"L/R","M/S"},1);
            box.setTitle(i?"Meter format":"Input format");box.setBounds(i?Rect{666,354,90,28}:Rect{27,326,133,28});
            formatLinks[i]=std::make_unique<State::ComboBoxAttachment>(state,i?"meter":"input",box);
        }
        const char* taps[]{"Inv 1","Inv 2","Swap"};const char* keys[]{"invert1","invert2","swap"};
        for(int i=0;i<3;++i){auto& b=routing[i];addAndMakeVisible(b);b.setButtonText(taps[i]);b.setClickingTogglesState(true);
            b.setBounds(18+53*i,381,50,28);b.setColour(juce::TextButton::buttonOnColourId,Colour(0xff247c68));
            routingLinks[i]=std::make_unique<State::ButtonAttachment>(state,keys[i],b);}
        addAndMakeVisible(reset);reset.setBounds(506,45,117,25);
        reset.onClick=[this]{for(int i=1;i<4;++i){param(i)->beginChangeGesture();change(i,i==1?1.f:0.f);param(i)->endChangeGesture();}};
        addAndMakeVisible(clip);clip.setBounds(664,393,94,28);clip.onClick=[this]{arc.clipped=false;};
        setSize(782,444);startTimerHz(36);
    }
    ~ArcPanel() override {stopTimer();endDrag();}
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(!graph.contains(e.getPosition()))return;
        shape=e.mods.isAltDown();initialX=value(shape?2:3);initialY=value(shape?1:0);
        param(shape?2:3)->beginChangeGesture();param(shape?1:0)->beginChangeGesture();dragging=true;
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(!dragging)return;
        change(shape?2:3,initialX+e.getDistanceFromDragStartX()*(shape?180.f:90.f)/graph.getWidth());
        change(shape?1:0,initialY-e.getDistanceFromDragStartY()*(shape?3.f:24.f)/graph.getHeight());
    }
    void mouseUp(const juce::MouseEvent&) override {endDrag();}
    void paint(juce::Graphics& g) override
    {
        using namespace juce;
        auto label=[&](String s,Rect r,float size,uint32_t col,Justification align=Justification::centred){
            g.setColour(Colour(col));g.setFont(FontOptions(size,Font::bold));g.drawText(s,r,align);};
        g.fillAll(Colour(0xff132020));g.setColour(Colour(0xff1c2b2c));g.fillRoundedRectangle(12,44,166,385,9);
        g.fillRoundedRectangle(644,44,126,385,9);SonmooBrand::draw(g,21,8,23);
        label("StereoArc",{578,4,181,32},24,light,Justification::centredRight);
        label("INPUT",{18,49,152,22},13,muted);label("GAIN  dB",{21,84,74,22},13,light);label("WIDTH",{99,84,73,22},13,light);
        label("FORMAT",{24,300,140,19},12,muted);label("INPUT CHANNELS",{19,357,152,20},11,muted);
        label("STEREO IMAGE",{202,45,185,23},14,light,Justification::centredLeft);
        g.setColour(Colour(0xff0c1518));g.fillRoundedRectangle(graph.toFloat(),8);
        const Point origin(413,286);constexpr float radius=177;
        g.setColour(Colour(0xff324b4c));
        for(float f:{.25f,.5f,.75f,1.f}){
            Path path;path.addCentredArc(origin.x,origin.y,radius*f,radius*f,0,-MathConstants<float>::halfPi,MathConstants<float>::halfPi,true);
            g.strokePath(path,PathStrokeType(1));}
        for(int angle=-90;angle<=90;angle+=30)g.drawLine({origin,origin.getPointOnCircumference(radius,degreesToRadians((float)angle))});
        const StereoArc::Image image(value(1),value(0),value(2),value(3));
        auto point=[&](float l,float r){
            auto out=image.map(l,r);
            float mid=out[0]+out[1],side=out[1]-out[0];
            if(mid<0){mid=-mid;side=-side;}
            float db=Decibels::gainToDecibels(std::hypot(out[0],out[1]),-36.f);
            return origin.getPointOnCircumference(radius*jlimit(0.f,1.f,(db+36.f)/45.f),std::atan2(side,mid));
        };
        auto left=point(1,0),right=point(0,1),centre=point(.70710678f,.70710678f);
        Path fan;fan.startNewSubPath(origin);fan.lineTo(left);fan.lineTo(right);fan.closeSubPath();
        g.setColour(Colour(0x4434d3b3));g.fillPath(fan);g.setColour(Colour(mint));g.strokePath(fan,PathStrokeType(1.8f));
        g.setColour(Colour(0xffebc788));g.drawLine({origin,centre},2);g.fillEllipse(centre.x-4,centre.y-4,8,8);
        for(auto p:{left,right}){g.setColour(Colour(mint));g.fillEllipse(p.x-4,p.y-4,8,8);}
        label("L",{205,276,25,23},12,muted);label("R",{594,276,25,23},12,muted);
        label("Drag: rotation / gain     |     Alt + drag: asymmetry / width",{200,311,426,23},13,muted);
        label("Asymmetry",{197,354,88,28},14,light);label("Rotation",{197,393,88,28},14,light);
        label("OUTPUT",{649,49,116,22},13,muted);
        const bool ms=formats[1].getSelectedId()==2;
        for(int i=0;i<2;++i){
            const float db=Decibels::gainToDecibels(arc.peaks[(ms?2:0)+i].load(),-60.f);
            int x=674+i*49;
            label(ms?(i?"S":"M"):(i?"R":"L"),{x-10,86,38,23},14,light);
            g.setColour(Colour(0xff0b1516));g.fillRoundedRectangle((float)x,116,17,175,4);
            float h=jlimit(0.f,1.f,(db+60.f)/60.f)*175;
            g.setColour(Colour(db>0?0xfff18370:mint));g.fillRect((float)x,291-h,17.f,h);
            label(db<=-60?String("-inf"):String(db,1),{x-15,294,48,20},12,light);}
        label("METER FORMAT",{651,329,112,20},11,muted);
    }
};
