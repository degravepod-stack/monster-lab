#include "MainComponent.h"

namespace ml
{
//==============================================================================
// Palette — dark lab instrument, amber phosphor, teal for "processed"
//==============================================================================
namespace C
{
    const juce::Colour bg       { 0xff0b0c0f };
    const juce::Colour panel    { 0xff14161b };
    const juce::Colour panel2   { 0xff1a1d23 };
    const juce::Colour line     { 0xff2a2e37 };
    const juce::Colour text     { 0xffd9d4c7 };
    const juce::Colour dim      { 0xff7d7a72 };
    const juce::Colour amber    { 0xffffb34a };
    const juce::Colour teal     { 0xff5fe0c0 };
    const juce::Colour red      { 0xffe2574c };
}

static juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h).withStyle (bold ? "Bold" : "Regular"));
}
static juce::Font monoFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
}

class MonsterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MonsterLookAndFeel()
    {
        setColourScheme ({ C::bg, C::panel, C::panel2, C::line, C::text, C::amber, C::bg, C::amber, C::text });
        setColour (juce::ResizableWindow::backgroundColourId, C::bg);
        setColour (juce::TextButton::buttonColourId, C::panel2);
        setColour (juce::TextButton::buttonOnColourId, C::amber.withAlpha (0.25f));
        setColour (juce::TextButton::textColourOffId, C::text);
        setColour (juce::TextButton::textColourOnId, C::amber);
        setColour (juce::Slider::textBoxTextColourId, C::amber);
        setColour (juce::Slider::textBoxBackgroundColourId, C::bg);
        setColour (juce::Slider::textBoxOutlineColourId, C::line);
        setColour (juce::Slider::trackColourId, C::amber);
        setColour (juce::Slider::backgroundColourId, C::line);
        setColour (juce::Label::textColourId, C::text);
        setColour (juce::Label::textWhenEditingColourId, C::amber);
        setColour (juce::TextEditor::backgroundColourId, C::bg);
        setColour (juce::TextEditor::textColourId, C::amber);
        setColour (juce::TextEditor::highlightColourId, C::amber.withAlpha (0.3f));
        setColour (juce::TextEditor::outlineColourId, C::line);
        setColour (juce::TextEditor::focusedOutlineColourId, C::amber);
        setColour (juce::ComboBox::backgroundColourId, C::bg);
        setColour (juce::ComboBox::textColourId, C::amber);
        setColour (juce::ComboBox::outlineColourId, C::line);
        setColour (juce::ComboBox::arrowColourId, C::amber);
        setColour (juce::PopupMenu::backgroundColourId, C::panel2);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, C::amber.withAlpha (0.3f));
        setColour (juce::ScrollBar::thumbColourId, C::line.brighter (0.3f));
        setColour (juce::AlertWindow::backgroundColourId, C::panel);
        setColour (juce::AlertWindow::textColourId, C::text);
        setColour (juce::AlertWindow::outlineColourId, C::amber);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto base = b.getToggleState() ? C::amber.withAlpha (0.22f) : C::panel2;
        if (over) base = base.brighter (0.08f);
        if (down) base = base.brighter (0.15f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (b.getToggleState() ? C::amber : C::line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return uiFont (juce::jmin (14.0f, h * 0.55f), true); }

    // LED-style switch
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float s = juce::jmin (12.0f, r.getHeight() - 4);
        juce::Rectangle<float> led (r.getX() + 2, r.getCentreY() - s / 2, s, s);
        const bool on = b.getToggleState();
        auto col = b.findColour (juce::ToggleButton::tickColourId);
        if (on)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillEllipse (led.expanded (3));
        }
        g.setColour (on ? col : C::bg);
        g.fillEllipse (led);
        g.setColour (on ? col.brighter (0.3f) : (over ? C::dim : C::line.brighter (0.2f)));
        g.drawEllipse (led, 1.0f);
        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (on ? C::text : C::dim);
            g.setFont (uiFont (juce::jmin (14.0f, r.getHeight() * 0.62f), true));
            g.drawText (b.getButtonText(), r.withTrimmedLeft (s + 8), juce::Justification::centredLeft, true);
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 2.0f, (float) w, 4.0f);
        g.setColour (C::line);
        g.fillRoundedRectangle (r, 2.0f);
        const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
        float zeroX = bipolar ? (float) s.getPositionOfValue (0.0) : (float) x;
        auto active = s.isEnabled() ? s.findColour (juce::Slider::trackColourId) : C::dim;
        g.setColour (active);
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zeroX, pos), r.getY(), juce::jmax (zeroX, pos), r.getBottom()), 2.0f);
        g.setColour (C::text);
        g.fillRoundedRectangle (pos - 2.5f, (float) y + h * 0.5f - 6.0f, 5.0f, 12.0f, 1.5f);
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr) return monoFont (12.0f);
        return uiFont (14.0f);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return monoFont (12.0f); }
    juce::Font getPopupMenuFont() override { return uiFont (14.0f); }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setJustificationType (juce::Justification::centredRight);
        return l;
    }
};

//==============================================================================
// LEFT: preset library
//==============================================================================
class PresetPanel : public juce::Component
{
public:
    explicit PresetPanel (MainComponent& m) : main (m)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        refresh();
    }

    void refresh()
    {
        items.clear();
        juce::String lastCat;
        for (auto& p : builtInPresets())
        {
            if (p.category != lastCat) { addHeader (p.category); lastCat = p.category; }
            addItem (p.name, [this, n = p.name] { main.addLayer (makeBuiltInPreset (n)); });
        }
        addHeader ("MY LAYER PRESETS");
        auto files = userLayerPresetFolder().findChildFiles (juce::File::findFiles, false, "*.mlayer");
        files.sort();
        if (files.isEmpty())
            addNote ("Save a customised layer with its disk button and it will appear here.");
        for (auto& f : files)
            addItem (f.getFileNameWithoutExtension(), [this, f]
            {
                auto v = juce::JSON::parse (f);
                if (v.isObject()) main.addLayer (Layer::fromVar (v));
            });
        addItem ("+ Empty layer", [this] { Layer l; l.name = "Custom"; main.addLayer (l); });
        layoutContent();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (C::panel);
        g.setColour (C::amber);
        g.setFont (monoFont (13.0f, true));
        g.drawText ("PRESET LIBRARY", 12, 8, getWidth() - 24, 20, juce::Justification::centredLeft);
        g.setColour (C::dim);
        g.setFont (uiFont (12.0f));
        g.drawText ("click to add a layer", 12, 26, getWidth() - 24, 16, juce::Justification::centredLeft);
    }
    void resized() override { viewport.setBounds (getLocalBounds().withTrimmedTop (46)); layoutContent(); }

private:
    struct Item
    {
        std::unique_ptr<juce::Component> comp;
        int height;
    };

    struct Header : public juce::Component
    {
        juce::String t;
        void paint (juce::Graphics& g) override
        {
            g.setColour (C::dim);
            g.setFont (monoFont (11.0f, true));
            g.drawText (t, getLocalBounds().withTrimmedLeft (12).withTrimmedTop (8), juce::Justification::centredLeft);
            g.setColour (C::line);
            g.drawHorizontalLine (getHeight() - 1, 12.0f, (float) getWidth() - 8);
        }
    };
    struct Note : public juce::Component
    {
        juce::String t;
        void paint (juce::Graphics& g) override
        {
            g.setColour (C::dim);
            g.setFont (uiFont (12.0f));
            g.drawFittedText (t, getLocalBounds().reduced (12, 2), juce::Justification::topLeft, 3);
        }
    };
    struct PresetButton : public juce::Component
    {
        juce::String t; std::function<void()> onClick; bool over = false;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().reduced (6, 1).toFloat();
            if (over) { g.setColour (C::amber.withAlpha (0.12f)); g.fillRoundedRectangle (r, 3.0f); }
            g.setColour (over ? C::amber : C::text);
            g.setFont (uiFont (14.0f));
            g.drawText ((over ? "+  " : "   ") + t, r.withTrimmedLeft (4), juce::Justification::centredLeft, true);
        }
        void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { over = false; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override  { if (e.mouseWasClicked() && onClick) onClick(); }
    };

    void addHeader (const juce::String& t) { auto h = std::make_unique<Header>(); h->t = t; push (std::move (h), 30); }
    void addNote (const juce::String& t)   { auto h = std::make_unique<Note>(); h->t = t; push (std::move (h), 40); }
    void addItem (const juce::String& t, std::function<void()> f)
    {
        auto b = std::make_unique<PresetButton>(); b->t = t; b->onClick = std::move (f);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        push (std::move (b), 24);
    }
    void push (std::unique_ptr<juce::Component> c, int h) { content.addAndMakeVisible (*c); items.push_back ({ std::move (c), h }); }

    void layoutContent()
    {
        int y = 0, w = juce::jmax (100, viewport.getMaximumVisibleWidth());
        for (auto& i : items) { i.comp->setBounds (0, y, w, i.height); y += i.height; }
        content.setSize (w, y + 10);
    }

    MainComponent& main;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<Item> items;
};

//==============================================================================
// CENTER (top): the layer stack
//==============================================================================
class LayerRow : public juce::Component
{
public:
    LayerRow (MainComponent& m, int idx) : main (m), index (idx)
    {
        auto& L = main.stack[(size_t) index];
        power.setToggleState (L.enabled, juce::dontSendNotification);
        power.setColour (juce::ToggleButton::tickColourId, C::amber);
        power.setTooltip ("Layer on / off (bypass)");
        power.onClick = [this] { main.stack[(size_t) index].enabled = power.getToggleState(); main.parametersChanged(); repaint(); };
        addAndMakeVisible (power);

        name.setText (L.name, juce::dontSendNotification);
        name.setEditable (false, true, false);
        name.setFont (uiFont (15.0f, true));
        name.setTooltip ("Double-click to rename");
        name.onTextChange = [this] { main.stack[(size_t) index].name = name.getText(); };
        name.setInterceptsMouseClicks (false, true);   // single click falls through to select the row
        addAndMakeVisible (name);

        mix.setSliderStyle (juce::Slider::LinearHorizontal);
        mix.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        mix.setRange (0, 100, 1);
        mix.setTextValueSuffix ("%");
        mix.setValue (L.mix, juce::dontSendNotification);
        mix.setDoubleClickReturnValue (true, 100);
        mix.setColour (juce::Slider::trackColourId, C::teal);
        mix.setTooltip ("Wet/dry blend of this layer against the signal entering it");
        mix.onValueChange = [this] { main.stack[(size_t) index].mix = (float) mix.getValue(); main.parametersChanged(); };
        mix.onDragStart = [this] { main.selectLayer (index); };
        addAndMakeVisible (mix);

        const int n = (int) main.stack.size();
        using Fn = std::function<void (MainComponent&, int)>;
        setupButton (up,   juce::String::fromUTF8 ("\xe2\x96\xb2"), "Move layer earlier in the chain", index > 0,
                     Fn ([] (MainComponent& mc, int i) { if (i < 1) return; std::swap (mc.stack[(size_t) i], mc.stack[(size_t) i - 1]); mc.selected = i - 1; mc.structureChanged(); }));
        setupButton (down, juce::String::fromUTF8 ("\xe2\x96\xbc"), "Move layer later in the chain", index < n - 1,
                     Fn ([] (MainComponent& mc, int i) { if (i + 1 >= (int) mc.stack.size()) return; std::swap (mc.stack[(size_t) i], mc.stack[(size_t) i + 1]); mc.selected = i + 1; mc.structureChanged(); }));
        setupButton (dup,  "DUP", "Duplicate layer", true,
                     Fn ([] (MainComponent& mc, int i) { auto c = mc.stack[(size_t) i]; c.name << " copy"; c.seed += 7919;
                                                        mc.stack.insert (mc.stack.begin() + i + 1, c); mc.selected = i + 1; mc.structureChanged(); }));
        setupButton (save, "SAVE", "Save this layer as a reusable preset", true,
                     Fn ([] (MainComponent& mc, int i) { mc.saveLayerPreset (i); }));
        setupButton (del, juce::String::fromUTF8 ("\xe2\x9c\x95"), "Delete layer", true,
                     Fn ([] (MainComponent& mc, int i) { mc.stack.erase (mc.stack.begin() + i);
                                                        if (mc.selected >= (int) mc.stack.size()) mc.selected = (int) mc.stack.size() - 1;
                                                        mc.structureChanged(); }));
    }

    void paint (juce::Graphics& g) override
    {
        const bool sel = main.selected == index;
        auto r = getLocalBounds().toFloat().reduced (2, 2);
        g.setColour (sel ? C::panel2.brighter (0.06f) : C::panel2);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (sel ? C::amber : C::line);
        g.drawRoundedRectangle (r, 4.0f, sel ? 1.5f : 1.0f);
        g.setColour (C::dim);
        g.setFont (monoFont (12.0f, true));
        g.drawText (juce::String (index + 1).paddedLeft ('0', 2), r.removeFromLeft (30).toNearestInt(), juce::Justification::centred);

        auto& L = main.stack[(size_t) index];
        int active = 0;
        for (int e = 0; e < NumEffects; ++e) if (L.fx[(size_t) e].on && L.sectionOn[effectTable()[(size_t) e].section]) ++active;
        g.setFont (uiFont (12.0f));
        g.drawText (juce::String (active) + " fx", infoArea, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 6).withTrimmedLeft (26);
        power.setBounds (r.removeFromLeft (24));
        del.setBounds (r.removeFromRight (28)); r.removeFromRight (3);
        save.setBounds (r.removeFromRight (46)); r.removeFromRight (3);
        dup.setBounds (r.removeFromRight (40)); r.removeFromRight (3);
        down.setBounds (r.removeFromRight (28)); r.removeFromRight (3);
        up.setBounds (r.removeFromRight (28)); r.removeFromRight (8);
        mix.setBounds (r.removeFromRight (juce::jmin (220, r.getWidth() / 2)));
        infoArea = r.removeFromRight (44);
        name.setBounds (r);
    }

    void mouseDown (const juce::MouseEvent&) override { main.selectLayer (index); }

private:
    // Actions run asynchronously: they rebuild the row list, which deletes this row.
    void setupButton (juce::TextButton& b, const juce::String& t, const juce::String& tip, bool enabled,
                      std::function<void (MainComponent&, int)> f)
    {
        b.setButtonText (t);
        b.setTooltip (tip);
        b.setEnabled (enabled);
        b.onClick = [sp = juce::Component::SafePointer<MainComponent> (&main), idx = index, f]
        {
            juce::MessageManager::callAsync ([sp, idx, f]
            {
                if (auto* m = sp.getComponent())
                    if (juce::isPositiveAndBelow (idx, (int) m->stack.size()))
                        f (*m, idx);
            });
        };
        addAndMakeVisible (b);
    }

    MainComponent& main;
    int index;
    juce::ToggleButton power;
    juce::Label name;
    juce::Slider mix;
    juce::TextButton up, down, dup, save, del;
    juce::Rectangle<int> infoArea;
};

class StackPanel : public juce::Component
{
public:
    explicit StackPanel (MainComponent& m) : main (m)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
    }

    void rebuild()
    {
        rows.clear();
        for (int i = 0; i < (int) main.stack.size(); ++i)
        {
            rows.push_back (std::make_unique<LayerRow> (main, i));
            content.addAndMakeVisible (*rows.back());
        }
        resized();
        repaint();
    }
    void repaintRows() { for (auto& r : rows) r->repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (C::panel);
        g.setColour (C::amber);
        g.setFont (monoFont (13.0f, true));
        g.drawText ("EFFECT STACK", 12, 8, 200, 20, juce::Justification::centredLeft);
        g.setColour (C::dim);
        g.setFont (uiFont (12.0f));
        g.drawText ("original  >  layer 1  >  layer 2  >  ...  >  output", 140, 8, getWidth() - 150, 20, juce::Justification::centredLeft);
        if (main.stack.empty())
        {
            g.setColour (C::dim);
            g.setFont (uiFont (15.0f));
            g.drawFittedText ("No layers yet.\nClick a preset on the left (e.g. Demon, then VHS - Worn Tape) to stack effects.",
                              getLocalBounds().withTrimmedTop (40).reduced (20), juce::Justification::centred, 3);
        }
    }
    void resized() override
    {
        viewport.setBounds (getLocalBounds().withTrimmedTop (34).reduced (6, 0));
        int w = viewport.getMaximumVisibleWidth(), y = 0;
        for (auto& r : rows) { r->setBounds (0, y, w, 46); y += 46; }
        content.setSize (w, y);
    }

private:
    MainComponent& main;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<LayerRow>> rows;
};

//==============================================================================
// CENTER (bottom): every effect of the selected layer, in a compact grid
//==============================================================================
class EffectTile : public juce::Component
{
public:
    EffectTile (MainComponent& m, int effectIndex) : main (m), e (effectIndex)
    {
        auto& spec = effectTable()[(size_t) e];
        auto* L = main.selectedLayer();
        toggle.setButtonText (spec.label);
        toggle.setColour (juce::ToggleButton::tickColourId, C::amber);
        toggle.setToggleState (L->fx[(size_t) e].on, juce::dontSendNotification);
        toggle.onClick = [this]
        {
            if (auto* l = main.selectedLayer()) { l->fx[(size_t) e].on = toggle.getToggleState(); main.parametersChanged(); }
            updateEnabledLook();
        };
        addAndMakeVisible (toggle);

        if (isSampleEffect (e)) setupFileChooser();

        for (int i = 0; i < numParams (e); ++i)
        {
            auto& ps = spec.p[i];
            auto& pc = params[i];
            pc.label.setText (ps.label, juce::dontSendNotification);
            pc.label.setFont (uiFont (12.0f));
            pc.label.setColour (juce::Label::textColourId, C::dim);
            addAndMakeVisible (pc.label);

            if ((e == E_Resonance || isSampleEffect (e)) && i == 0)
            {
                pc.combo = std::make_unique<juce::ComboBox>();
                if (e == E_Resonance) for (int t = 0; t < 5; ++t) pc.combo->addItem (resonanceTypeName (t), t + 1);
                else                  for (int t = 0; t < 3; ++t) pc.combo->addItem (sampleModeName (t), t + 1);
                pc.combo->setSelectedId ((int) L->fx[(size_t) e].p[i] + 1, juce::dontSendNotification);
                pc.combo->onChange = [this, i]
                {
                    if (auto* l = main.selectedLayer())
                    {
                        l->fx[(size_t) e].p[i] = (float) (params[i].combo->getSelectedId() - 1);
                        autoEnable();
                        main.parametersChanged();
                    }
                };
                addAndMakeVisible (*pc.combo);
                continue;
            }

            pc.slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            auto& s = *pc.slider;
            s.setRange (ps.min, ps.max, ps.step);
            if (ps.centre > 0) s.setSkewFactorFromMidPoint (ps.centre);
            s.setNumDecimalPlacesToDisplay (ps.step >= 1 ? 0 : (ps.step >= 0.1f ? 1 : 2));
            s.setTextValueSuffix (juce::String (ps.unit).isEmpty() ? juce::String() : " " + juce::String (ps.unit));
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 66, 16);
            s.setValue (L->fx[(size_t) e].p[i], juce::dontSendNotification);
            s.setDoubleClickReturnValue (true, ps.def);
            s.setTooltip (juce::String (spec.label) + " " + ps.label + "  (double-click to reset, click the number to type)");
            s.onValueChange = [this, i]
            {
                if (auto* l = main.selectedLayer())
                {
                    l->fx[(size_t) e].p[i] = (float) params[i].slider->getValue();
                    autoEnable();
                    main.parametersChanged();
                }
            };
            addAndMakeVisible (s);
        }
        updateEnabledLook();
    }

    static int heightFor (int e) { return 26 + 21 * numParams (e) + 6 + (isSampleEffect (e) ? 26 : 0); }

    void setupFileChooser()
    {
        fileBox = std::make_unique<juce::ComboBox>();
        fileBox->setTooltip ("Sound from your Sound Library (Documents/Monster Lab/Sound Library)");
        fileBox->setTextWhenNothingSelected ("Choose a sound...");
        populateFiles();
        fileBox->onChange = [this]
        {
            const int id = fileBox->getSelectedId();
            if (id == kOpenFolderId || id == kRescanId)
            {
                if (id == kOpenFolderId) soundLibraryFolder().startAsProcess();
                juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<EffectTile> (this)]
                                                 { if (auto* t = sp.getComponent()) t->populateFiles(); });
                return;
            }
            if (auto* l = main.selectedLayer())
            {
                l->fx[(size_t) e].file = id == kNoneId ? juce::String() : files[(size_t) (id - kFirstFileId)];
                if (id != kNoneId) autoEnable();
                main.parametersChanged();
            }
        };
        addAndMakeVisible (*fileBox);
    }

    void populateFiles()
    {
        auto lib = soundLibraryFolder();
        auto found = lib.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg");
        found.sort();
        files.clear();
        fileBox->clear (juce::dontSendNotification);
        fileBox->addItem ("(none)", kNoneId);
        juce::String lastFolder = "\x01";
        const juce::String current = main.selectedLayer() ? main.selectedLayer()->fx[(size_t) e].file : juce::String();
        int selectId = kNoneId;
        for (auto& f : found)
        {
            auto rel = f.getRelativePathFrom (lib).replaceCharacter ('\\', '/');
            auto folder = f.getParentDirectory() == lib ? juce::String ("Sound Library") : f.getParentDirectory().getRelativePathFrom (lib).replaceCharacter ('\\', '/');
            if (folder != lastFolder) { fileBox->addSectionHeading (folder); lastFolder = folder; }
            const int id = kFirstFileId + (int) files.size();
            files.push_back (rel);
            fileBox->addItem (f.getFileNameWithoutExtension(), id);
            if (rel == current) selectId = id;
        }
        if (current.isNotEmpty() && selectId == kNoneId)   // chosen file has gone missing or lives elsewhere
        {
            const int id = kFirstFileId + (int) files.size();
            files.push_back (current);
            fileBox->addItem ((resolveSampleFile (current).existsAsFile() ? "" : "MISSING: ") + juce::File::createFileWithoutCheckingPath (current).getFileNameWithoutExtension(), id);
            selectId = id;
        }
        fileBox->addSeparator();
        fileBox->addItem (found.isEmpty() ? "Library is empty - open folder to add sounds..." : "Open Sound Library folder...", kOpenFolderId);
        fileBox->addItem ("Rescan library", kRescanId);
        fileBox->setSelectedId (selectId, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3);
        const bool on = toggle.getToggleState();
        g.setColour (on ? C::panel2.brighter (0.05f) : C::panel2.darker (0.15f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? C::amber.withAlpha (0.55f) : C::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (9, 6);
        toggle.setBounds (r.removeFromTop (20));
        r.removeFromTop (2);
        if (fileBox) { fileBox->setBounds (r.removeFromTop (24).reduced (0, 1)); r.removeFromTop (2); }
        for (int i = 0; i < numParams (e); ++i)
        {
            auto row = r.removeFromTop (21);
            params[i].label.setBounds (row.removeFromLeft (62));
            if (params[i].slider) params[i].slider->setBounds (row);
            if (params[i].combo)  params[i].combo->setBounds (row.reduced (0, 1));
        }
    }

private:
    void autoEnable()
    {
        // touching a control switches the effect on — no hunting for the switch
        if (auto* l = main.selectedLayer())
            if (! l->fx[(size_t) e].on)
            {
                l->fx[(size_t) e].on = true;
                toggle.setToggleState (true, juce::dontSendNotification);
                updateEnabledLook();
            }
    }
    void updateEnabledLook()
    {
        const bool on = toggle.getToggleState();
        for (auto& p : params)
        {
            if (p.slider) p.slider->setColour (juce::Slider::trackColourId, on ? C::amber : C::dim);
            if (p.slider) p.slider->setColour (juce::Slider::textBoxTextColourId, on ? C::amber : C::dim);
        }
        repaint();
    }

    struct ParamControl { juce::Label label; std::unique_ptr<juce::Slider> slider; std::unique_ptr<juce::ComboBox> combo; };
    static constexpr int kNoneId = 1, kOpenFolderId = 2, kRescanId = 3, kFirstFileId = 100;
    MainComponent& main;
    int e;
    std::unique_ptr<juce::ComboBox> fileBox;
    std::vector<juce::String> files;
    juce::ToggleButton toggle;
    ParamControl params[kMaxParams];
};

class EffectGrid : public juce::Component
{
public:
    explicit EffectGrid (MainComponent& m) : main (m)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        onlyActive.setButtonText ("Show active effects only");
        onlyActive.setColour (juce::ToggleButton::tickColourId, C::teal);
        onlyActive.onClick = [this] { rebuild(); };
        addAndMakeVisible (onlyActive);
    }

    void rebuild()
    {
        tiles.clear();
        sections.clear();
        sectionIds.clear();
        resets.clear();
        auto* L = main.selectedLayer();
        if (L != nullptr)
        {
            for (int s : { (int) Voice, (int) Texture, (int) Samples, (int) SciFi, (int) Space, (int) Media })   // processing order
            {
                auto sec = std::make_unique<juce::ToggleButton> (juce::String (sectionName (s)) + " SECTION");
                sec->setColour (juce::ToggleButton::tickColourId, C::teal);
                sec->setToggleState (L->sectionOn[s], juce::dontSendNotification);
                sec->setTooltip ("Switch every effect in this section on/off for this layer (settings are kept)");
                sec->onClick = [this, s, b = sec.get()]
                {
                    if (auto* l = main.selectedLayer()) { l->sectionOn[s] = b->getToggleState(); main.parametersChanged(); }
                    for (auto& t : tiles) if (t.section == s) t.tile->setAlpha (b->getToggleState() ? 1.0f : 0.4f);
                };
                content.addAndMakeVisible (*sec);
                sections.push_back (std::move (sec));
                sectionIds.push_back (s);

                auto rs = std::make_unique<juce::TextButton> ("RESET");
                rs->setTooltip ("Put this section back the way the layer started");
                rs->onClick = [sp = juce::Component::SafePointer<EffectGrid> (this), s]
                {
                    juce::MessageManager::callAsync ([sp, s]   // rebuild() deletes this button
                    {
                        if (auto* g = sp.getComponent())
                            if (auto* l = g->main.selectedLayer()) { l->resetSection (s); g->rebuild(); g->main.parametersChanged(); }
                    });
                };
                content.addAndMakeVisible (*rs);
                resets.push_back (std::move (rs));

                for (int e = 0; e < NumEffects; ++e)
                {
                    if (effectTable()[(size_t) e].section != s) continue;
                    if (onlyActive.getToggleState() && ! L->fx[(size_t) e].on) continue;
                    auto t = std::make_unique<EffectTile> (main, e);
                    t->setAlpha (L->sectionOn[s] ? 1.0f : 0.4f);
                    content.addAndMakeVisible (*t);
                    tiles.push_back ({ std::move (t), s, e });
                }
            }
        }
        resized();
        repaint();
    }

    void scrollToSection (int sectionId)
    {
        for (size_t i = 0; i < sectionIds.size(); ++i)
            if (sectionIds[i] == sectionId && i < sectionY.size()) viewport.setViewPosition (0, sectionY[i]);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (C::panel);
        g.setColour (C::amber);
        g.setFont (monoFont (13.0f, true));
        auto* L = main.selectedLayer();
        g.drawText (L ? "LAYER  >  " + L->name.toUpperCase() : juce::String ("LAYER CONTROLS"), 12, 8, getWidth() / 2, 20, juce::Justification::centredLeft);
        if (L == nullptr)
        {
            g.setColour (C::dim);
            g.setFont (uiFont (15.0f));
            g.drawText ("Select a layer to edit its effects.", getLocalBounds().withTrimmedTop (40), juce::Justification::centred);
        }
    }

    void resized() override
    {
        onlyActive.setBounds (getWidth() - 220, 6, 210, 24);
        viewport.setBounds (getLocalBounds().withTrimmedTop (34).reduced (6, 0));
        const int w = viewport.getMaximumVisibleWidth();
        const int cols = juce::jmax (1, w / 250);
        const int tw = w / cols;
        int y = 0;
        size_t ti = 0;
        sectionY.clear();
        for (int s = 0; s < (int) sections.size(); ++s)
        {
            sectionY.push_back (y);
            sections[(size_t) s]->setBounds (6, y + 6, 200, 22);
            resets[(size_t) s]->setBounds (210, y + 6, 60, 22);
            y += 32;
            int col = 0, rowH = 0;
            while (ti < tiles.size() && tiles[ti].section == sectionIds[(size_t) s])
            {
                int h = EffectTile::heightFor (tiles[ti].effect);
                tiles[ti].tile->setBounds (col * tw, y, tw, h);
                rowH = juce::jmax (rowH, h);
                if (++col == cols) { col = 0; y += rowH; rowH = 0; }
                ++ti;
            }
            y += rowH + 4;
        }
        content.setSize (w, y + 10);
    }

private:
    struct TileRef { std::unique_ptr<EffectTile> tile; int section, effect; };
    MainComponent& main;
    juce::Viewport viewport;
    juce::Component content;
    juce::ToggleButton onlyActive;
    std::vector<std::unique_ptr<juce::ToggleButton>> sections;
    std::vector<std::unique_ptr<juce::TextButton>> resets;
    std::vector<int> sectionIds, sectionY;
    std::vector<TileRef> tiles;
};

//==============================================================================
// RIGHT: audio workspace
//==============================================================================
class Waveform : public juce::Component
{
public:
    explicit Waveform (MainComponent& m) : main (m) {}

    void invalidate() { cacheW = -1; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (C::bg);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (C::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        if (main.original == nullptr)
        {
            g.setColour (C::dim);
            g.setFont (uiFont (16.0f));
            g.drawFittedText ("Drop a voice recording here\n(WAV, AIFF, FLAC)\n\nor click OPEN AUDIO", getLocalBounds(), juce::Justification::centred, 4);
            return;
        }
        rebuildCache();

        const float mid = r.getCentreY(), half = r.getHeight() * 0.45f;
        g.setColour (C::line.withAlpha (0.6f));
        g.drawHorizontalLine ((int) mid, 0, r.getRight());

        auto drawPeaks = [&] (const std::vector<float>& pk, juce::Colour c)
        {
            g.setColour (c);
            for (int x = 0; x < (int) pk.size(); ++x)
                if (pk[(size_t) x] > 0) g.drawVerticalLine (x, mid - pk[(size_t) x] * half, mid + pk[(size_t) x] * half);
        };
        const bool proc = main.playback.isShowingProcessed();
        drawPeaks (procPeaks, proc ? C::amber.withAlpha (0.85f) : C::amber.withAlpha (0.25f));
        drawPeaks (origPeaks, proc ? C::text.withAlpha (0.22f) : C::text.withAlpha (0.8f));

        // original start marker when there's a lead-in
        if (main.processedPreRoll > 0)
        {
            float x0 = timeToX (0.0);
            g.setColour (C::teal.withAlpha (0.6f));
            g.drawVerticalLine ((int) x0, r.getY(), r.getBottom());
            g.setFont (monoFont (11.0f));
            g.drawText ("original starts", (int) x0 + 4, 4, 120, 14, juce::Justification::centredLeft);
        }
        // playhead
        float px = timeToX (main.playback.getPosition());
        g.setColour (C::text);
        g.drawVerticalLine ((int) px, r.getY(), r.getBottom());

        g.setColour (C::dim);
        g.setFont (monoFont (11.0f));
        g.drawText (proc ? "PROCESSED (amber)  /  original (grey)" : "ORIGINAL (white)  /  processed (amber)",
                    6, getHeight() - 18, getWidth() - 12, 14, juce::Justification::centredLeft);
    }

    void mouseDown (const juce::MouseEvent& e) override { seek (e.x); }
    void mouseDrag (const juce::MouseEvent& e) override { seek (e.x); }

private:
    double t0() const { return -main.processedPreRoll / main.sampleRate; }
    double t1() const
    {
        double end = main.original ? main.original->getNumSamples() / main.sampleRate : 1.0;
        if (main.processed) end = juce::jmax (end, (main.processed->getNumSamples() - main.processedPreRoll) / main.sampleRate);
        return end;
    }
    float timeToX (double t) const { return (float) ((t - t0()) / juce::jmax (1e-6, t1() - t0()) * getWidth()); }
    void seek (int x)
    {
        main.playback.setPosition (t0() + (t1() - t0()) * juce::jlimit (0.0, 1.0, x / (double) getWidth()));
        repaint();
    }

    void rebuildCache()
    {
        if (cacheW == getWidth() && cachedOrig == main.original.get() && cachedProc == main.processed.get()) return;
        cacheW = getWidth(); cachedOrig = main.original.get(); cachedProc = main.processed.get();
        auto peaks = [this] (const juce::AudioBuffer<float>* b, int offsetSamples, std::vector<float>& out)
        {
            out.assign ((size_t) juce::jmax (1, cacheW), 0.0f);
            if (b == nullptr) return;
            const double startS = t0() * main.sampleRate, spanS = (t1() - t0()) * main.sampleRate;
            for (int x = 0; x < cacheW; ++x)
            {
                int a = (int) (startS + spanS * x / cacheW) + offsetSamples;
                int z = (int) (startS + spanS * (x + 1) / cacheW) + offsetSamples;
                a = juce::jmax (0, a); z = juce::jmin (b->getNumSamples(), juce::jmax (a + 1, z));
                if (a >= b->getNumSamples()) continue;
                float m = 0;
                for (int ch = 0; ch < b->getNumChannels(); ++ch) m = juce::jmax (m, b->getMagnitude (ch, a, z - a));
                out[(size_t) x] = juce::jmin (1.0f, m);
            }
        };
        peaks (main.original.get(), 0, origPeaks);
        peaks (main.processed.get(), main.processedPreRoll, procPeaks);
    }

    MainComponent& main;
    int cacheW = -1;
    const void* cachedOrig = nullptr;
    const void* cachedProc = nullptr;
    std::vector<float> origPeaks, procPeaks;
};

class AudioPanel : public juce::Component
{
    MainComponent& main;
public:
    explicit AudioPanel (MainComponent& m) : main (m), wave (m)
    {
        addAndMakeVisible (wave);
        auto add = [this] (juce::TextButton& b, const juce::String& t, const juce::String& tip, std::function<void()> f)
        { b.setButtonText (t); b.setTooltip (tip); b.onClick = std::move (f); addAndMakeVisible (b); };

        add (open,   "OPEN AUDIO",      "Load a voice recording (or drag one onto the window)", [this] { main.openAudioDialog(); });
        add (play,   "PLAY",            "Play / pause (space bar)", [this] { main.togglePlay(); });
        add (stop,   "STOP",            "Stop and return to the start", [this] { main.stopPlay(); });
        add (aBtn,   "A  ORIGINAL",     "Listen to the untouched original", [this] { setAB (false); });
        add (bBtn,   "B  PROCESSED",    "Listen to the processed result", [this] { setAB (true); });
        add (exportBtn, "EXPORT", "Write the processed audio as a new file (the original is never changed)", [this] { main.exportDialog(); });
        add (removeBtn, juce::String::fromUTF8 ("\xe2\x9c\x95"), "Remove this audio from Monster Lab (the file on disk is not touched)", [this] { main.clearAudio(); });

        auto setupCombo = [this] (juce::ComboBox& c, const juce::String& tip)
        { c.setTooltip (tip); addAndMakeVisible (c); c.onChange = [this] { exportSettingsChanged(); }; };
        setupCombo (fmtBox, "File format");
        fmtBox.addItem ("WAV", 1); fmtBox.addItem ("AIFF", 2); fmtBox.addItem ("FLAC", 3);
        setupCombo (bitsBox, "Bit depth (24-bit is right for Audition)");
        bitsBox.addItem ("16-bit", 16); bitsBox.addItem ("24-bit", 24); bitsBox.addItem ("32-bit float", 32);
        setupCombo (chanBox, "Mono or stereo. 'Same as original' gives mono for a mono recording.");
        chanBox.addItem ("Same as original", 1); chanBox.addItem ("Mono", 2); chanBox.addItem ("Stereo", 3);
        fmtBox.setSelectedId (main.exportFormat, juce::dontSendNotification);
        bitsBox.setSelectedId (main.exportBits, juce::dontSendNotification);
        chanBox.setSelectedId (main.exportChannels, juce::dontSendNotification);
        exportSettingsChanged (false);
        add (saveChar, "SAVE CHARACTER", "Save the whole layer stack as a character file", [this] { main.saveCharacterDialog(); });
        add (loadChar, "LOAD CHARACTER", "Load a saved character (replaces the current stack)", [this] { main.loadCharacterDialog(); });
        add (settings, "AUDIO OUTPUT", "Choose the audio output device", [this] { main.showAudioSettings(); });
        aBtn.setClickingTogglesState (false);
        bBtn.setClickingTogglesState (false);
        setAB (true);
    }

    void setAB (bool processed)
    {
        main.playback.setShowProcessed (processed);
        aBtn.setToggleState (! processed, juce::dontSendNotification);
        bBtn.setToggleState (processed, juce::dontSendNotification);
        wave.repaint();
    }
    void refresh()
    {
        play.setButtonText (main.playback.isPlaying() ? "PAUSE" : "PLAY");
        play.setToggleState (main.playback.isPlaying(), juce::dontSendNotification);
        wave.repaint();
        repaint (statusArea);
        repaint (infoArea);
    }
    void audioChanged() { wave.invalidate(); removeBtn.setVisible (main.original != nullptr); repaint(); }

    void exportSettingsChanged (bool save = true)
    {
        const bool wav = fmtBox.getSelectedId() == 1;
        bitsBox.setItemEnabled (32, wav);                     // 32-bit float is WAV only
        if (! wav && bitsBox.getSelectedId() == 32) bitsBox.setSelectedId (24, juce::dontSendNotification);
        main.exportFormat = fmtBox.getSelectedId();
        main.exportBits = bitsBox.getSelectedId();
        main.exportChannels = chanBox.getSelectedId();
        if (save) main.saveSettings();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (C::panel);
        g.setColour (C::amber);
        g.setFont (monoFont (13.0f, true));
        g.drawText ("AUDIO WORKSPACE", 12, 8, 200, 20, juce::Justification::centredLeft);

        g.setFont (monoFont (12.0f));
        g.setColour (C::text);
        juce::String info = "no audio loaded";
        if (main.original)
            info = main.sourceFile.getFileName() + "\n"
                 + juce::String (main.original->getNumSamples() / main.sampleRate, 2) + " s   "
                 + juce::String (main.sampleRate / 1000.0, 1) + " kHz   "
                 + (main.original->getNumChannels() > 1 ? "stereo" : "mono");
        g.drawFittedText (info, infoArea.withTrimmedRight (30), juce::Justification::topLeft, 2);

        g.setColour (C::dim);
        g.setFont (uiFont (12.0f));
        g.drawText ("EXPORT AS", exportLabelArea, juce::Justification::centredLeft);

        // position
        g.setColour (C::amber);
        g.setFont (monoFont (20.0f, true));
        double t = main.playback.getPosition();
        g.drawText ((t < 0 ? "-" : " ") + juce::String (std::abs (t), 2) + " s", timeArea, juce::Justification::centredRight);

        // status
        auto sr = statusArea.toFloat();
        g.setColour (C::bg);
        g.fillRoundedRectangle (sr, 4.0f);
        const bool busy = ! main.renderUpToDate;
        g.setColour (busy ? C::amber : C::teal);
        g.fillEllipse (sr.getX() + 8, sr.getY() + 10, 8, 8);
        g.setFont (uiFont (13.0f));
        g.setColour (C::text);
        if (busy)
        {
            const float p = juce::jlimit (0.0f, 1.0f, main.renderProgress.load());
            const double elapsed = (juce::Time::getMillisecondCounterHiRes() - main.renderStartMs.load()) / 1000.0;
            juce::String line = "Processing...  " + juce::String ((int) (p * 100)) + "%";
            if (main.renderStartMs.load() > 0 && elapsed > 0.8 && p > 0.03f && p < 0.999f)
            {
                const double left = elapsed / p * (1.0 - p);
                line << "   about " << formatDuration (left) << " left";
            }
            else if (main.renderStartMs.load() > 0 && elapsed > 0.8)
                line << "   " << formatDuration (elapsed) << " so far";
            g.drawText (line, statusArea.withTrimmedLeft (24).reduced (4, 4).removeFromTop (18), juce::Justification::centredLeft);
            auto bar = statusArea.toFloat().reduced (10, 0).withTrimmedTop (32).withHeight (8);
            g.setColour (C::line);
            g.fillRoundedRectangle (bar, 4.0f);
            g.setColour (C::amber);
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (8.0f, bar.getWidth() * p)), 4.0f);
            g.setColour (C::dim);
            g.setFont (uiFont (12.0f));
            g.drawFittedText ("You can keep adjusting; it restarts with your latest settings.",
                              statusArea.reduced (10, 0).withTrimmedTop (48).withHeight (32), juce::Justification::topLeft, 2);
        }
        else
            g.drawFittedText (main.statusText, statusArea.withTrimmedLeft (24).reduced (4, 4), juce::Justification::topLeft, 5);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12).withTrimmedTop (26);
        infoArea = r.removeFromTop (34);
        removeBtn.setBounds (infoArea.getRight() - 26, infoArea.getY() + 2, 26, 24);
        wave.setBounds (r.removeFromTop (juce::jmax (140, r.getHeight() / 3)));
        r.removeFromTop (8);
        auto t = r.removeFromTop (34);
        timeArea = t.removeFromRight (110);
        play.setBounds (t.removeFromLeft (90)); t.removeFromLeft (6);
        stop.setBounds (t.removeFromLeft (70));
        r.removeFromTop (8);
        auto ab = r.removeFromTop (34);
        aBtn.setBounds (ab.removeFromLeft (ab.getWidth() / 2 - 3)); ab.removeFromLeft (6);
        bBtn.setBounds (ab);
        r.removeFromTop (12);
        exportLabelArea = r.removeFromTop (16);
        auto ex = r.removeFromTop (28);
        const int cw = (ex.getWidth() - 12) / 3;
        fmtBox.setBounds (ex.removeFromLeft (cw - 20)); ex.removeFromLeft (6);
        bitsBox.setBounds (ex.removeFromLeft (cw)); ex.removeFromLeft (6);
        chanBox.setBounds (ex);
        r.removeFromTop (6);
        exportBtn.setBounds (r.removeFromTop (40));
        r.removeFromTop (8);
        auto c = r.removeFromTop (32);
        saveChar.setBounds (c.removeFromLeft (c.getWidth() / 2 - 3)); c.removeFromLeft (6);
        loadChar.setBounds (c);
        r.removeFromTop (8);
        auto o = r.removeFromTop (30);
        open.setBounds (o.removeFromLeft (o.getWidth() / 2 - 3)); o.removeFromLeft (6);
        settings.setBounds (o);
        r.removeFromTop (12);
        statusArea = r.removeFromTop (juce::jmin (130, r.getHeight()));
    }

    Waveform wave;

private:
    static juce::String formatDuration (double sec)
    {
        if (sec < 60) return juce::String ((int) std::ceil (sec)) + " s";
        const int m = (int) (sec / 60), s2 = (int) sec % 60;
        return juce::String (m) + " min " + juce::String (s2).paddedLeft ('0', 2) + " s";
    }

    juce::TextButton open, play, stop, aBtn, bBtn, exportBtn, saveChar, loadChar, settings, removeBtn;
    juce::ComboBox fmtBox, bitsBox, chanBox;
    juce::Rectangle<int> infoArea, statusArea, timeArea, exportLabelArea;
};

//==============================================================================
// Playback
//==============================================================================
void PlaybackSource::setBuffers (std::shared_ptr<const juce::AudioBuffer<float>> o,
                                 std::shared_ptr<const juce::AudioBuffer<float>> p, int pre, double rate)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    orig = std::move (o); proc = std::move (p); preRoll = pre; fileRate = rate;
}

double PlaybackSource::startTime() const { return -preRoll / juce::jmax (1.0, fileRate); }
double PlaybackSource::endTime() const
{
    double e = orig ? orig->getNumSamples() / fileRate : 0.0;
    if (proc) e = juce::jmax (e, (proc->getNumSamples() - preRoll) / fileRate);
    return e;
}
void PlaybackSource::stop() { playing = false; pos = startTime() * fileRate; }

void PlaybackSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();
    if (! playing) return;
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked()) return;

    const bool useProc = showProcessed && proc != nullptr;
    const juce::AudioBuffer<float>* b = useProc ? proc.get() : orig.get();
    if (b == nullptr) return;
    const int offset = useProc ? preRoll : 0;
    const double ratio = fileRate / deviceRate;
    double p = pos.load();
    const int n = b->getNumSamples();
    const int outCh = info.buffer->getNumChannels();

    for (int i = 0; i < info.numSamples; ++i)
    {
        double idx = p + offset;
        int i0 = (int) std::floor (idx);
        float f = (float) (idx - i0);
        for (int ch = 0; ch < outCh; ++ch)
        {
            int sc = juce::jmin (ch, b->getNumChannels() - 1);
            float a = (i0 >= 0 && i0 < n) ? b->getSample (sc, i0) : 0.0f;
            float c = (i0 + 1 >= 0 && i0 + 1 < n) ? b->getSample (sc, i0 + 1) : 0.0f;
            info.buffer->setSample (ch, info.startSample + i, a + (c - a) * f);
        }
        p += ratio;
    }
    pos = p;
    if (p / fileRate >= endTime()) { playing = false; pos = startTime() * fileRate; }
}

//==============================================================================
// MainComponent
//==============================================================================
MainComponent::MainComponent() : juce::Thread ("Monster Lab renderer")
{
    lnf = std::make_unique<MonsterLookAndFeel>();
    juce::LookAndFeel::setDefaultLookAndFeel (lnf.get());
    formats.registerBasicFormats();
    loadSettings();
    soundLibraryFolder();   // create the Sound Library folders on first run

    presets    = std::make_unique<PresetPanel> (*this);
    stackPanel = std::make_unique<StackPanel> (*this);
    grid       = std::make_unique<EffectGrid> (*this);
    audioPanel = std::make_unique<AudioPanel> (*this);
    addAndMakeVisible (*presets);
    addAndMakeVisible (*stackPanel);
    addAndMakeVisible (*grid);
    addAndMakeVisible (*audioPanel);

    deviceManager.initialiseWithDefaultDevices (0, 2);
    player.setSource (&playback);
    deviceManager.addAudioCallback (&player);

    setWantsKeyboardFocus (true);
    setSize (1480, 900);
    startThread (juce::Thread::Priority::normal);
    startTimerHz (30);

    // Automated end-to-end check used by the build (MONSTERLAB_SELFTEST=<wav> MONSTERLAB_SELFTEST_OUT=<dir>)
    auto testWav = juce::SystemStats::getEnvironmentVariable ("MONSTERLAB_SELFTEST", {});
    if (testWav.isNotEmpty())
    {
        selfTestOut = juce::File (juce::SystemStats::getEnvironmentVariable ("MONSTERLAB_SELFTEST_OUT", "/tmp"));
        selfTestOut.createDirectory();
        selfTestStage = 0;
        juce::MessageManager::callAsync ([this, testWav] { if (! loadAudio (juce::File (testWav))) juce::JUCEApplication::quit(); });
    }
}

void MainComponent::selfTestStep()
{
    auto log = [this] (const juce::String& s) { selfTestOut.getChildFile ("selftest.log").appendText (s + "\n"); };
    auto snap = [this] (const juce::String& name)
    {
        auto img = getTopLevelComponent()->createComponentSnapshot (getTopLevelComponent()->getLocalBounds());
        juce::FileOutputStream os (selfTestOut.getChildFile (name));
        os.setPosition (0); os.truncate();
        juce::PNGImageFormat().writeImageToStream (img, os);
    };
    switch (selfTestStage)
    {
        case 0: addLayer (makeBuiltInPreset ("Demon")); addLayer (makeBuiltInPreset ("VHS - Worn Tape")); log ("added Demon + VHS"); ++selfTestStage; break;
        case 1: if (renderUpToDate && processed) { snap ("1_demon_vhs.png"); log ("render 1 ok: " + statusText); ++selfTestStage; } break;
        case 2:
            selectLayer (0);
            stack[0].fx[E_Pitch].p[0] = -9.0f;           // user adjusts Demon pitch
            stack[1].mix = 70.0f;                         // and VHS blend
            grid->rebuild(); stackPanel->rebuild();
            parametersChanged(); log ("adjusted pitch -9, VHS mix 70"); ++selfTestStage; break;
        case 3: if (renderUpToDate && ! renderScheduled) { snap ("2_adjusted.png"); ++selfTestStage; } break;
        case 4:
        {
            auto tryExport = [&] (int fmt, int bits, int chans, const juce::String& name)
            {
                exportFormat = fmt; exportBits = bits; exportChannels = chans;
                auto f = selfTestOut.getChildFile (name);
                bool ok = writeProcessedWav (f);
                std::unique_ptr<juce::AudioFormatReader> rd (formats.createReaderFor (f));
                log ((ok && rd ? "export ok " : "export FAILED ") + name + " (" + exportDescription() + ")"
                     + (rd ? "  -> " + juce::String ((int) rd->numChannels) + " ch, " + juce::String ((int) rd->bitsPerSample) + " bit, "
                              + juce::String (rd->lengthInSamples / rd->sampleRate, 2) + " s" : juce::String()));
            };
            tryExport (1, 24, 1, "selftest_export.wav");
            tryExport (1, 32, 3, "selftest_export_32f.wav");
            tryExport (2, 24, 3, "selftest_export.aif");
            tryExport (3, 16, 2, "selftest_export.flac");
            exportFormat = 1; exportBits = 24; exportChannels = 1;
            playback.play(); ++selfTestStage; break;
        }
        case 5: case 6: case 7: case 8: case 9: ++selfTestStage; break;     // let playback run a moment
        case 10: log ("playback position after ~0.2 s: " + juce::String (playback.getPosition(), 3)); playback.stop();
                 addLayer (makeBuiltInPreset ("Ghost")); ++selfTestStage; break;
        case 11:
            if (! renderUpToDate && renderProgress.load() > 0.25f && ! progressShot) { snap ("3a_progress.png"); progressShot = true; log ("progress shown at " + juce::String ((int) (renderProgress.load() * 100)) + "%"); }
            if (renderUpToDate && ! renderScheduled) { snap ("3_three_layers.png"); log ("3 layers ok: " + statusText); ++selfTestStage; }
            break;
        case 12:
            selectLayer (0);
            stack[0].fx[E_Sample1].on = true;
            stack[0].fx[E_Sample1].file = "Creature/Growls/growl.wav";
            stack[0].fx[E_Sample1].p[0] = 1;
            grid->rebuild(); grid->scrollToSection (Samples); parametersChanged(); log ("Demon + growl (Through)"); ++selfTestStage; break;
        case 13: if (renderUpToDate && ! renderScheduled) { snap ("4_samples.png"); log ("samples ok: " + statusText); ++selfTestStage; } break;
        case 14: clearAudio(); log (original == nullptr && processed == nullptr ? "remove audio ok" : "remove audio FAILED"); snap ("5_removed.png"); ++selfTestStage; break;
        default: log ("done"); juce::JUCEApplication::quit(); selfTestStage = -1; break;
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    abortRender = true;
    signalThreadShouldExit();
    renderWake.signal();
    stopThread (10000);
    deviceManager.removeAudioCallback (&player);
    player.setSource (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (C::bg);
    auto top = getLocalBounds().removeFromTop (44);
    g.setColour (C::amber);
    g.setFont (monoFont (22.0f, true));
    g.drawText ("DEGRAVE BROTHERS' MONSTER LAB", top.withTrimmedLeft (16), juce::Justification::centredLeft);
    g.setColour (C::dim);
    g.setFont (uiFont (13.0f));
    g.drawText ("creature & vintage voice processor   v0.4", top.withTrimmedLeft (420), juce::Justification::centredLeft);
    g.setColour (C::amber.withAlpha (0.5f));
    g.drawHorizontalLine (43, 0, (float) getWidth());
}

void MainComponent::resized()
{
    auto r = getLocalBounds().withTrimmedTop (44).reduced (6);
    presets->setBounds (r.removeFromLeft (230));
    r.removeFromLeft (6);
    audioPanel->setBounds (r.removeFromRight (400));
    r.removeFromRight (6);
    stackPanel->setBounds (r.removeFromTop (juce::jlimit (150, 300, r.getHeight() / 3)));
    r.removeFromTop (6);
    grid->setBounds (r);
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey) { togglePlay(); return true; }
    return false;
}

//------------------------------------------------------------------ model ops
void MainComponent::addLayer (Layer l)
{
    stack.push_back (std::move (l));
    selected = (int) stack.size() - 1;
    structureChanged();
}

void MainComponent::structureChanged()
{
    if (selected >= (int) stack.size()) selected = (int) stack.size() - 1;
    stackPanel->rebuild();
    grid->rebuild();
    parametersChanged();
}

void MainComponent::selectLayer (int i)
{
    if (i == selected) return;
    selected = i;
    stackPanel->repaintRows();
    grid->rebuild();
}

void MainComponent::parametersChanged()
{
    stackPanel->repaintRows();
    lastChangeMs = juce::Time::currentTimeMillis();
    renderScheduled = true;
    if (original) renderUpToDate = false;
    audioPanel->refresh();
}

void MainComponent::saveLayerPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) stack.size())) return;
    auto* w = new juce::AlertWindow ("Save layer preset", "Name for this layer preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", stack[(size_t) index].name);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, index] (int r)
    {
        if (r != 1 || ! juce::isPositiveAndBelow (index, (int) stack.size())) return;
        auto name = juce::File::createLegalFileName (w->getTextEditorContents ("name").trim());
        if (name.isEmpty()) return;
        auto l = stack[(size_t) index];
        l.name = name;
        auto f = userLayerPresetFolder().getChildFile (name + ".mlayer");
        if (f.replaceWithText (juce::JSON::toString (l.toVar())))
        {
            statusText = "Saved layer preset \"" + name + "\". It's now under MY LAYER PRESETS.";
            presets->refresh();
        }
        else statusText = "Could not save the layer preset.";
        audioPanel->refresh();
    }), true);
}

//------------------------------------------------------------------ audio files
bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;m4a;ogg;monster")) return true;
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    juce::File f (files[0]);
    if (f.hasFileExtension ("monster"))
    {
        auto v = juce::JSON::parse (f);
        if (v.isObject()) { stack = stackFromVar (v); selected = stack.empty() ? -1 : 0; structureChanged(); statusText = "Loaded character " + f.getFileName(); }
        return;
    }
    loadAudio (f);
}

void MainComponent::openAudioDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a voice recording", sourceFile.existsAsFile() ? sourceFile.getParentDirectory() : juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) loadAudio (f); });
}

bool MainComponent::loadAudio (const juce::File& f, bool longFileConfirmed)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr)
    {
        statusText = "Couldn't read " + f.getFileName() + ". Try a WAV file.";
        audioPanel->refresh();
        return false;
    }
    if (reader->lengthInSamples > (juce::int64) std::numeric_limits<int>::max() / 2)
    {
        statusText = "That file is too long for Monster Lab to hold in memory. Split it into parts first.";
        audioPanel->refresh();
        return false;
    }
    const double minutes = reader->lengthInSamples / reader->sampleRate / 60.0;
    if (minutes > 20.0 && ! longFileConfirmed)
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Long recording",
            f.getFileName() + " is " + juce::String (minutes, 0) + " minutes long.\n\n"
            "Every change re-processes the whole file, so with heavy layers each adjustment can take several minutes "
            "(the progress bar shows an estimate). It may also use a lot of memory.\n\n"
            "Tip: trim a short section to design the character, save it, then load the long file and apply it.",
            "Load anyway", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([sp = juce::Component::SafePointer<MainComponent> (this), f] (int r)
            {
                if (r == 1) if (auto* m = sp.getComponent()) m->loadAudio (f, true);
            }));
        return false;
    }
    auto buf = std::make_shared<juce::AudioBuffer<float>> ((int) juce::jmin (2u, reader->numChannels), (int) reader->lengthInSamples);
    reader->read (buf.get(), 0, (int) reader->lengthInSamples, 0, true, buf->getNumChannels() > 1);

    playback.stop();
    sourceFile = f;
    sampleRate = reader->sampleRate;
    original = buf;
    processed = nullptr;
    processedPreRoll = 0;
    ++sourceGeneration;
    playback.setBuffers (original, nullptr, 0, sampleRate);
    statusText = "Loaded. Click presets on the left to stack effects.";
    audioPanel->audioChanged();
    parametersChanged();
    return true;
}

void MainComponent::clearAudio()
{
    playback.stop();
    abortRender = true;
    ++sourceGeneration;
    original = nullptr;
    processed = nullptr;
    processedPreRoll = 0;
    sourceFile = juce::File();
    playback.setBuffers (nullptr, nullptr, 0, sampleRate);
    renderUpToDate = true;
    renderScheduled = false;
    statusText = "Audio removed. Drop in another recording to carry on; your layers are still here.";
    audioPanel->audioChanged();
    audioPanel->refresh();
}

static const char* exportExtension (int fmt) { return fmt == 2 ? "aif" : (fmt == 3 ? "flac" : "wav"); }

juce::String MainComponent::exportDescription() const
{
    juce::String d = juce::String (exportFormat == 2 ? "AIFF" : (exportFormat == 3 ? "FLAC" : "WAV")) + ", ";
    d << (exportBits == 32 ? "32-bit float" : juce::String (exportBits) + "-bit") << ", ";
    d << (exportMono() ? "mono" : "stereo");
    return d;
}

bool MainComponent::exportMono() const
{
    if (exportChannels == 2) return true;
    if (exportChannels == 3) return false;
    return original != nullptr && original->getNumChannels() == 1;
}

void MainComponent::loadSettings()
{
    auto v = juce::JSON::parse (userDataFolder().getChildFile ("settings.json"));
    exportFormat   = juce::jlimit (1, 3, (int) v.getProperty ("exportFormat", 1));
    exportBits     = (int) v.getProperty ("exportBits", 24);
    if (exportBits != 16 && exportBits != 24 && exportBits != 32) exportBits = 24;
    if (exportFormat != 1 && exportBits == 32) exportBits = 24;
    exportChannels = juce::jlimit (1, 3, (int) v.getProperty ("exportChannels", 1));
}

void MainComponent::saveSettings()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("exportFormat", exportFormat);
    o->setProperty ("exportBits", exportBits);
    o->setProperty ("exportChannels", exportChannels);
    userDataFolder().getChildFile ("settings.json").replaceWithText (juce::JSON::toString (juce::var (o)));
}

void MainComponent::exportDialog()
{
    if (! original) { statusText = "Load a recording first."; audioPanel->refresh(); return; }
    if (! renderUpToDate || ! processed)
    {
        statusText = "Still rendering. Export as soon as the light turns teal.";
        audioPanel->refresh();
        return;
    }
    const juce::String ext = exportExtension (exportFormat);
    auto def = sourceFile.getSiblingFile (sourceFile.getFileNameWithoutExtension() + " - Monster Lab." + ext);
    chooser = std::make_unique<juce::FileChooser> ("Export processed audio (" + exportDescription() + ")", def, "*." + ext);
    auto rate = sampleRate;
    auto pre = processedPreRoll;
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, rate, pre] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension (exportExtension (exportFormat));
        if (f == sourceFile) { statusText = "Refusing to overwrite the original recording. Pick another name."; audioPanel->refresh(); return; }
        bool ok = writeProcessedWav (f);
        if (! ok) statusText = "Export failed. Is the folder writable?";
        else
        {
            statusText = "Exported " + f.getFileName() + " (" + exportDescription() + ").";
            if (pre > 0)
                statusText << "\nNote: it starts " << juce::String (pre / rate, 2)
                           << " s BEFORE the original (reverse-reverb swell). In Audition, place it that much earlier to line up.";
        }
        audioPanel->refresh();
    });
}

bool MainComponent::writeProcessedWav (const juce::File& f)
{
    if (! processed || f == sourceFile) return false;

    const bool mono = exportMono();
    const int bits = exportBits;
    juce::AudioBuffer<float> data (mono ? 1 : 2, processed->getNumSamples());
    if (mono)
    {
        data.clear();
        data.addFrom (0, 0, *processed, 0, 0, processed->getNumSamples(), 0.5f);
        data.addFrom (0, 0, *processed, 1, 0, processed->getNumSamples(), 0.5f);
    }
    else
        data.makeCopyOf (*processed);

    if (bits == 16)   // TPDF dither so quiet tails don't get grainy
    {
        juce::Random r (12345);
        const float lsb = 1.0f / 32768.0f;
        for (int ch = 0; ch < data.getNumChannels(); ++ch)
        {
            auto* d = data.getWritePointer (ch);
            for (int i = 0; i < data.getNumSamples(); ++i) d[i] += (r.nextFloat() - r.nextFloat()) * lsb;
        }
    }

    std::unique_ptr<juce::AudioFormat> format;
    if (exportFormat == 2)      format = std::make_unique<juce::AiffAudioFormat>();
    else if (exportFormat == 3) format = std::make_unique<juce::FlacAudioFormat>();
    else                        format = std::make_unique<juce::WavAudioFormat>();

    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (! os) return false;
    auto opts = juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (data.getNumChannels())
                    .withBitsPerSample (bits)
                    .withSampleFormat (bits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                  : juce::AudioFormatWriterOptions::SampleFormat::integral);
    auto w = format->createWriterFor (os, opts);
    return w != nullptr && w->writeFromAudioSampleBuffer (data, 0, data.getNumSamples());
}

void MainComponent::saveCharacterDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Save character", userCharacterFolder().getChildFile ("My Character.monster"), "*.monster");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("monster");
        statusText = f.replaceWithText (juce::JSON::toString (stackToVar (stack))) ? "Saved character " + f.getFileName() : juce::String ("Could not save.");
        audioPanel->refresh();
    });
}

void MainComponent::loadCharacterDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Load character", userCharacterFolder(), "*.monster");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        auto v = juce::JSON::parse (f);
        if (! v.isObject()) { statusText = "That file isn't a Monster Lab character."; audioPanel->refresh(); return; }
        stack = stackFromVar (v);
        selected = stack.empty() ? -1 : 0;
        statusText = "Loaded character " + f.getFileName();
        structureChanged();
    });
}

void MainComponent::showAudioSettings()
{
    auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 2, 2, false, false, true, false);
    sel->setSize (500, 300);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (sel);
    o.dialogTitle = "Audio Output";
    o.dialogBackgroundColour = C::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::togglePlay()
{
    if (! original) return;
    if (playback.isPlaying()) playback.pause(); else playback.play();
    audioPanel->refresh();
}
void MainComponent::stopPlay() { playback.stop(); audioPanel->refresh(); }

//------------------------------------------------------------------ rendering
void MainComponent::timerCallback()
{
    if (renderScheduled && juce::Time::currentTimeMillis() - lastChangeMs > 180)
    {
        renderScheduled = false;
        requestRender();
    }
    audioPanel->refresh();
    if (selfTestStage >= 0) selfTestStep();
}

void MainComponent::requestRender()
{
    if (! original) { renderUpToDate = true; return; }
    {
        const juce::ScopedLock sl (renderLock);
        pendingStack = stack;
        pendingSource = original;
        pendingRate = sampleRate;
        pendingSourceGen = sourceGeneration;
        pendingGeneration = ++requestedGeneration;
    }
    renderUpToDate = false;
    renderProgress = 0;
    abortRender = true;
    renderWake.signal();
}

void MainComponent::run()
{
    StackRenderer renderer;
    int lastSourceGen = -1;
    while (! threadShouldExit())
    {
        renderWake.wait (-1);
        while (! threadShouldExit())
        {
            Stack st;
            std::shared_ptr<const juce::AudioBuffer<float>> src;
            double rate; int gen, srcGen;
            {
                const juce::ScopedLock sl (renderLock);
                st = pendingStack; src = pendingSource; rate = pendingRate; gen = pendingGeneration; srcGen = pendingSourceGen;
                abortRender = false;
            }
            if (src == nullptr) break;
            if (srcGen != lastSourceGen) { renderer.clear(); lastSourceGen = srcGen; }

            auto out = std::make_shared<RenderOutput>();
            renderProgress = 0;
            renderStartMs = juce::Time::getMillisecondCounterHiRes();
            bool ok = renderer.render (*src, rate, st, *out, &abortRender, [this] (float p) { renderProgress = p; });
            if (ok && ! threadShouldExit())
            {
                juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<MainComponent> (this), out, gen, srcGen]
                {
                    auto* m = sp.getComponent();
                    if (m == nullptr || srcGen != m->sourceGeneration) return;
                    m->processed = std::make_shared<juce::AudioBuffer<float>> (std::move (out->audio));
                    m->processedPreRoll = out->preRoll;
                    m->playback.setBuffers (m->original, m->processed, out->preRoll, m->sampleRate);
                    if (gen == m->requestedGeneration.load())
                    {
                        m->renderUpToDate = true;
                        m->renderLimited = out->limited;
                        int active = 0;
                        for (auto& l : m->stack) if (l.enabled) ++active;
                        m->statusText = active == 0 ? juce::String ("Ready. No active layers: processed = original.")
                                                    : "Ready. " + juce::String (active) + " active layer" + (active == 1 ? "" : "s") + ". Press space to play.";
                        if (out->preRoll > 0)
                            m->statusText << "\nReverse-reverb lead-in: " << juce::String (out->preRoll / m->sampleRate, 2) << " s before the original.";
                        if (out->limited)
                            m->statusText << "\nOutput was turned down slightly to avoid clipping.";
                    }
                    m->audioPanel->audioChanged();
                });
            }
            if (requestedGeneration.load() == gen) break;   // nothing newer queued
        }
    }
}
} // namespace ml
