class PitchSkin final : public LookAndFeel_V4 {
public:
    PitchSkin()
    {
        setColour(TextButton::buttonColourId,Colour(0xff263337));setColour(TextButton::buttonOnColourId,Colour(0xff137868));
        setColour(ComboBox::backgroundColourId,Colour(0xff182226));setColour(Slider::textBoxBackgroundColourId,Colour(0xff1e272a));
        setColour(Slider::textBoxTextColourId,Colours::white);setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);
        setColour(Slider::trackColourId,Colour(0xff4be0c4));setColour(Slider::thumbColourId,Colour(0xffd9e3dd));
    }
    void drawRotarySlider(Graphics& g,int x,int y,int width,int height,float position,float start,float end,Slider&) override
    {
        auto r=jmin(width,height)*.43f; Point<float> c{x+width*.5f,y+height*.5f};
        auto disc=Rectangle<float>(2*r,2*r).withCentre(c);
        g.setColour(Colour(0xff172024));g.fillEllipse(disc.expanded(3));
        for(int i=0;i<=24;++i)
        {
            float a=start+(end-start)*i/24;Point<float> d{std::sin(a),-std::cos(a)};
            g.setColour(i/24.f<=position?Colour(0xff5bf2cc):Colour(0xff637274));
            g.drawLine({c+d*(r-1),c+d*(r-5)},1.7f);
        }
        ColourGradient grad(Colour(0xfff3efd9),c.x-r,c.y-r,Colour(0xff798482),c.x+r,c.y+r,false);
        grad.addColour(.46,Colour(0xffb9bfad));grad.addColour(.54,Colour(0xffe1dfcc));g.setGradientFill(grad);g.fillEllipse(disc.reduced(8));
        float a=start+(end-start)*position;Point<float> d{std::sin(a),-std::cos(a)};
        g.setColour(Colour(0xff182222));g.drawLine({c+d*(r*.18f),c+d*(r-10)},3);
    }
};

class PitchPanel final : public AudioProcessorEditor,private Timer {
    PitchPilot& p; PitchSkin skin;
    using SA=State::SliderAttachment;using BA=State::ButtonAttachment;using CA=State::ComboBoxAttachment;
    std::array<Slider,7> knobs;
    std::array<std::unique_ptr<SA>,7> sliderBindings;
    std::array<TextButton,4> switches;std::array<std::unique_ptr<BA>,4> switchBindings;
    ComboBox scale,root;std::unique_ptr<CA> scaleBinding,rootBinding;
    std::array<TextButton,7> registers;
    TextButton edit{"Edit scale"},listen{"Reference tone"},target{"Hold target"},reset{"Reset keys"};
    TextButton group{"Group octaves"},vib{"Vibrato"};
    Slider toneLevel;
    std::unique_ptr<BA> groupBinding,vibBinding;
    std::unique_ptr<SA> toneBinding;
    int mode=0;
    static bool black(int n){const int k=n%12;return k==1||k==3||k==6||k==8||k==10;}
    static Rectangle<float> keyBox(int note)
    {
        int whites=0;
        for(int n=24;n<note;++n) if(!black(n)) ++whites;
        float w=912.f/35;
        if(black(note))return {24+whites*w-w*.30f,321,w*.60f,60};
        return {24+whites*w,321,w-1,96};
    }
    int hit(Point<float> pt) const
    {
        for(bool b:{true,false})for(int n=24;n<84;++n)if(black(n)==b && keyBox(n).contains(pt))return n;
        return -1;
    }
    void setParam(String id,float v)
    {
        auto* param=p.state.getParameter(id);param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(v));param->endChangeGesture();
    }
    void setMode(int m)
    {
        mode=m;edit.setToggleState(m==0,dontSendNotification);listen.setToggleState(m==1,dontSendNotification);
        target.setToggleState(m==2,dontSendNotification);p.keyboardTarget=-1;p.audition=-1;
    }
    void timerCallback() override
    {
        for(int i=0;i<7;++i)registers[i].setToggleState(i==(int)p.value(Range),dontSendNotification);
        knobs[Vibrato].setEnabled(p.value(PitchPilot::VibratoOn)>.5f);
        knobs[Transition].setEnabled(p.value(Linked)<.5f);repaint();
    }
public:
    explicit PitchPanel(PitchPilot& proc):AudioProcessorEditor(proc),p(proc)
    {
        setLookAndFeel(&skin);
        const char* ids[]{"speed","transition","cents","hold","amount","vibrato","reference"};
        const Rectangle<int> boxes[]{{28,95,113,138},{280,95,113,138},{145,134,63,97},{211,134,63,97},
                                     {816,89,120,150},{123,254,181,26},{804,477,124,30}};
        for(int i=0;i<7;++i)
        {
            auto& s=knobs[i];addAndMakeVisible(s);s.setBounds(boxes[i]);
            s.setSliderStyle(i<5?Slider::RotaryHorizontalVerticalDrag:i==5?Slider::LinearHorizontal:Slider::IncDecButtons);
            s.setTextBoxStyle(i<5?Slider::TextBoxBelow:Slider::TextBoxRight,false,i==Reference?72:50,21);
            sliderBindings[i]=std::make_unique<SA>(p.state,ids[i],s);
        }
        const char* labels[]{"Correction on","Formant preserve","MIDI target","Link times"};
        const char* bools[]{"enabled","formant","midi","link"};
        const Rectangle<int> sb[]{{810,56,127,27},{806,249,132,33},{780,550,132,34},{154,77,107,26}};
        for(int i=0;i<4;++i)
        {
            auto& b=switches[i];b.setButtonText(labels[i]);b.setClickingTogglesState(true);
            addAndMakeVisible(b);b.setBounds(sb[i]);switchBindings[i]=std::make_unique<BA>(p.state,bools[i],b);
        }
        for(int i=0;i<7;++i)
        {
            auto& b=registers[i];b.setButtonText(ranges[i]);addAndMakeVisible(b);b.setBounds(25+i*104,477,100,30);
            b.onClick=[this,i]{setParam("range",(float)i);};
        }
        auto combo=[&](ComboBox& b,StringArray items,Rectangle<int> bounds)
        {b.addItemList(items,1);addAndMakeVisible(b);b.setBounds(bounds);};
        combo(scale,scales,{116,550,156,34});combo(root,roots,{26,550,78,34});
        scaleBinding=std::make_unique<CA>(p.state,"scale",scale);rootBinding=std::make_unique<CA>(p.state,"root",root);
        int x=301;
        for(auto* b:{&edit,&listen,&target,&reset}){addAndMakeVisible(*b);b->setBounds(x,550,109,34);x+=112;}
        edit.onClick=[this]{setMode(0);};listen.onClick=[this]{setMode(1);};target.onClick=[this]{setMode(2);};
        reset.onClick=[this]{
            for(int k=0;k<12;++k)setParam("key"+String(k),0);
            for(int n=24;n<84;++n)setParam("note"+String(n),0);
            for(int i=0;i<72;++i)setParam("route"+String(i),0);
        };
        for(auto* button:{&group,&vib}){addAndMakeVisible(*button);button->setClickingTogglesState(true);}
        group.setBounds(780,419,155,26);vib.setBounds(30,254,83,26);
        groupBinding=std::make_unique<BA>(p.state,"group",group);
        vibBinding=std::make_unique<BA>(p.state,"vibratoOn",vib);
        addAndMakeVisible(toneLevel);toneLevel.setBounds(305,254,86,26);
        toneLevel.setSliderStyle(Slider::IncDecButtons);toneLevel.setTextBoxStyle(Slider::TextBoxRight,false,47,20);
        toneBinding=std::make_unique<SA>(p.state,"toneLevel",toneLevel);
        setMode(0);
        setSize(960,611);timerCallback();startTimerHz(30);
    }
    ~PitchPanel() override {stopTimer();p.audition=-1;p.keyboardTarget=-1;setLookAndFeel(nullptr);}
    void mouseDown(const MouseEvent& e) override
    {
        int n=hit(e.position);if(n<0)return;
        if(mode==0)
        {
            const bool grouped=p.value(PitchPilot::Group)>.5f;
            const String scope=grouped ? "key"+String(n%12) : "note"+String(n);
            const String routeID="route"+String(grouped ? n%12 : 12+n-24);
            if(e.mods.isPopupMenu())
            {
                PopupMenu menu;menu.addItem(1,"Allow");menu.addItem(2,"Exclude");
                menu.addItem(3,"Pitch up");menu.addItem(4,"Pitch down");menu.addItem(5,"Bypass tuning");
                menu.showMenuAsync(PopupMenu::Options().withTargetComponent(this),
                    [safe=Component::SafePointer<PitchPanel>(this),scope,routeID](int item){
                        if(safe!=nullptr && item>0){
                            safe->setParam(scope,item==2 ? 2.f : 1.f);
                            safe->setParam(routeID,(float)jmax(0,item-2));
                        }
                    });
            }
            else
            {
                const bool on=p.allowed(n);setParam(routeID,0);setParam(scope,on?2.f:1.f);
            }
        }
        if(mode==1)p.audition=n;
        if(mode==2)p.keyboardTarget=n;
    }
    void mouseDrag(const MouseEvent& e) override
    {int n=hit(e.position);if(mode==1)p.audition=n;if(mode==2)p.keyboardTarget=n;}
    void mouseUp(const MouseEvent&) override {p.audition=-1;p.keyboardTarget=-1;}
    void paint(Graphics& g) override
    {
        const Colour ink(0xff20292b),silver(0xffc2c8c5),mint(0xff61efd0),muted(0xffafbfbd);
        auto label=[&](String s,int x,int y,int w,int h,float size,Colour c,Justification align=Justification::centred)
        {g.setColour(c);g.setFont(FontOptions(size,Font::bold));g.drawText(s,x,y,w,h,align);};
        g.fillAll(Colour(0xff1a2428));SonmooBrand::draw(g,21,8,24);
        label("SonmooTune",157,5,150,29,23,Colours::white,Justification::centredLeft);
        label("VOCAL PITCH CORRECTION",676,9,260,20,13,muted,Justification::centredRight);
        ColourGradient face(Colour(0xffe0e1d9),0,43,silver,0,294,false);face.addColour(.56,Colour(0xffa7afad));
        g.setGradientFill(face);g.fillRoundedRectangle(16,44,928,249,7);
        label("SPEED",32,65,100,23,16,ink);label("TRANSITION (ms)",269,65,134,23,14,ink);
        label("TOLERANCE",146,104,127,20,13,ink);label("CENTS",146,125,65,16,11,ink);label("TIME (ms)",209,125,65,16,11,ink);
        label("ms",32,231,100,15,11,ink);label("%",839,236,75,14,11,ink);
        label("TONE dB",314,235,72,17,10,ink);
        g.setColour(Colour(0xff0d1518));g.fillRoundedRectangle(405,54,390,228,8);
        const int detected=p.displayNote.load()<0?-1:roundToInt(p.displayNote.load());
        const int aimed=p.targetNote.load();const float cents=p.deviation.load();
        auto noteName=[](int n)->String{return n<0?String("--"):roots[n%12]+String(n/12-1);};
        constexpr float cx=600,cy=193;
        for(int i=-20;i<=20;++i)
        {
            const float a=i*.083f;
            Point<float>d{std::sin(a),-std::cos(a)};
            bool lit=aimed>=0 && std::abs(i-jlimit(-20,20,roundToInt(cents/5)))<=1;
            g.setColour(lit?mint:i==0?Colour(0xff829291):Colour(0xff344348));
            g.drawLine({Point<float>{cx,cy}+d*69.f,Point<float>{cx,cy}+d*98.f},5.8f);
        }
        label("TARGET  /  CORRECTION",523,62,155,19,12,muted);label(noteName(aimed),520,146,160,54,42,mint);
        label(aimed<0?"Waiting for voice":String(roundToInt(cents))+" ct   /   Input "+noteName(detected),488,223,224,28,16,Colours::white);
        auto meter=[&](int x,float v,String title)
        {
            label(title,x-14,70,42,17,11,muted);
            int count=jlimit(0,24,roundToInt((Decibels::gainToDecibels(v,-48.f)+48)*.5f));
            for(int i=0;i<24;++i){g.setColour(i<count?(i>20?Colour(0xffeabb76):mint):Colour(0xff2b373a));g.fillRect(x,249-i*6,12,4);}
        };
        meter(423,p.inputLevel.load(),"IN");meter(764,p.outputLevel.load(),"OUT");
        for(bool b:{false,true})for(int n=24;n<84;++n)if(black(n)==b)
        {
            auto r=keyBox(n);bool on=p.allowed(n);
            Colour key=b?(on?Colour(0xff253035):Colour(0xff445156)):(on?Colour(0xffe8e8dc):Colour(0xff929d9c));
            if(n==aimed || n==p.audition.load())key=mint;
            g.setColour(key);g.fillRoundedRectangle(r,2);
            const int rule=p.route(n);
            if(rule) label(rule==1?"+":rule==2?"-":"B",(int)r.getX(),(int)r.getY()+4,(int)r.getWidth(),16,12,b?Colours::white:ink);
            if(n==detected){g.setColour(Colour(0xffea9684));g.fillEllipse(r.getCentreX()-3,r.getBottom()-16,6,6);}
            if(n%12==0)label(noteName(n),(int)r.getX(),393,(int)r.getWidth(),20,11,ink);
        }
        label("KEYBOARD",24,296,110,23,12,muted,Justification::centredLeft);
        label("TARGET",720,296,85,23,11,mint);label("INPUT",819,296,80,23,11,Colour(0xffea9684));
        label(mode==0?(p.value(PitchPilot::Group)>.5f?"Click to toggle all octaves. Right-click for direction or bypass.":"Click to toggle this note. Right-click for direction or bypass."):mode==1?"Hold a key to hear its reference pitch.":"Hold a key to tune the vocal to that note.",24,421,880,20,13,muted,Justification::centredLeft);
        label("VOCAL RANGE",25,450,200,22,13,muted,Justification::centredLeft);
        label("REFERENCE (Hz)",804,450,124,22,13,muted);
        label("ROOT",26,524,78,20,12,muted,Justification::centredLeft);label("SCALE",116,524,156,20,12,muted,Justification::centredLeft);
        label("KEYBOARD MODE",301,524,175,20,12,muted,Justification::centredLeft);
    }
};
