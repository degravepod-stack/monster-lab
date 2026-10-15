#include "Model.h"
#include <cmath>

namespace ml
{
Layer::Layer()
{
    const auto& t = effectTable();
    for (int e = 0; e < NumEffects; ++e)
        for (int i = 0; i < kMaxParams; ++i)
            fx[(size_t) e].p[i] = t[(size_t) e].p[i].label ? t[(size_t) e].p[i].def : 0.0f;
    captureStart();
}

juce::var Layer::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", name);
    o->setProperty ("preset", presetName);
    o->setProperty ("enabled", enabled);
    o->setProperty ("mix", mix);
    o->setProperty ("seed", (int) seed);

    auto* sec = new juce::DynamicObject();
    for (int s = 0; s < NumSections; ++s)
        sec->setProperty (sectionName (s), sectionOn[s]);
    o->setProperty ("sections", juce::var (sec));

    auto* effects = new juce::DynamicObject();
    const auto& t = effectTable();
    for (int e = 0; e < NumEffects; ++e)
    {
        auto* eo = new juce::DynamicObject();
        eo->setProperty ("on", fx[(size_t) e].on);
        for (int i = 0; i < kMaxParams; ++i)
            if (auto* l = t[(size_t) e].p[i].label)
                eo->setProperty (l, fx[(size_t) e].p[i]);
        effects->setProperty (t[(size_t) e].id, juce::var (eo));
    }
    o->setProperty ("effects", juce::var (effects));
    return juce::var (o);
}

Layer Layer::fromVar (const juce::var& v)
{
    Layer l;
    l.name       = v.getProperty ("name", "Layer").toString();
    l.presetName = v.getProperty ("preset", "").toString();
    l.enabled    = (bool) v.getProperty ("enabled", true);
    l.mix        = (float) (double) v.getProperty ("mix", 100.0);
    l.seed       = (juce::uint32) (int) v.getProperty ("seed", 1);

    auto sec = v.getProperty ("sections", {});
    for (int s = 0; s < NumSections; ++s)
        l.sectionOn[s] = (bool) sec.getProperty (sectionName (s), true);

    auto effects = v.getProperty ("effects", {});
    const auto& t = effectTable();
    for (int e = 0; e < NumEffects; ++e)
    {
        auto eo = effects.getProperty (t[(size_t) e].id, {});
        if (! eo.isObject()) continue;
        l.fx[(size_t) e].on = (bool) eo.getProperty ("on", false);
        for (int i = 0; i < kMaxParams; ++i)
            if (auto* lab = t[(size_t) e].p[i].label)
            {
                auto& ps = t[(size_t) e].p[i];
                float x = (float) (double) eo.getProperty (lab, ps.def);
                l.fx[(size_t) e].p[i] = juce::jlimit (ps.min, ps.max, x);
            }
    }
    l.captureStart();
    return l;
}

juce::var stackToVar (const Stack& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "Monster Lab");
    o->setProperty ("format", 1);
    juce::Array<juce::var> layers;
    for (auto& l : s) layers.add (l.toVar());
    o->setProperty ("layers", layers);
    return juce::var (o);
}

Stack stackFromVar (const juce::var& v)
{
    Stack s;
    if (auto* arr = v.getProperty ("layers", {}).getArray())
        for (auto& lv : *arr)
            s.push_back (Layer::fromVar (lv));
    return s;
}

//==============================================================================
// Built-in presets. Each entry switches effects on; listed numbers override the
// effect's parameters in order (NAN = keep that parameter's default).
namespace
{
    constexpr float D = NAN;
    struct Fx { const char* id; std::vector<float> vals; };
    struct PresetDef { const char* category; const char* name; std::vector<Fx> fx; };

    const std::vector<PresetDef>& defs()
    {
        static const std::vector<PresetDef> d {
            // ------------------------------------------------ HORROR & SUPERNATURAL
            { "HORROR & SUPERNATURAL", "Demon", {
                { "pitch", { -7 } }, { "formant", { -5 } }, { "bass", { 6, 100 } },
                { "subharm", { 40, 1 } }, { "grit", { 30 } }, { "growl", { 35, 38 } },
                { "dist", { 7, 5000 } }, { "reverb", { 2.0f, 16, 45 } } } },
            { "HORROR & SUPERNATURAL", "Zombie", {
                { "pitch", { -3 } }, { "formant", { -2 } }, { "growl", { 60, 28 } }, { "grit", { 55 } },
                { "breath", { 20 } }, { "instab", { 40, 2.0f } }, { "band", { 120, 7000 } },
                { "reverb", { 1.2f, 12, 40 } } } },
            { "HORROR & SUPERNATURAL", "Ghost", {
                { "formant", { 1.5f } }, { "breath", { 55 } }, { "instab", { 20, 0.5f } },
                { "freeze", { 60, 25 } }, { "shimmer", { 12, 6, 30 } }, { "revverb", { 2.0f, 70 } },
                { "echo", { 420, 35, 20 } }, { "stereo", { 0.15f, 70 } }, { "band", { 250, 9000 } } } },
            { "HORROR & SUPERNATURAL", "Vampire", {
                { "pitch", { -2 } }, { "formant", { -1 } }, { "bass", { 3, 120 } }, { "breath", { 35 } },
                { "chorus", { 0.4f, 25, 20 } }, { "reverb", { 3.5f, 25, 60 } } } },
            { "HORROR & SUPERNATURAL", "Dragon", {
                { "size", { 85 } }, { "bass", { 8, 80 } }, { "subharm", { 55, 1 } }, { "growl", { 45, 32 } },
                { "dist", { 10, 4000 } }, { "reverb", { 4.0f, 28, 40 } } } },
            { "HORROR & SUPERNATURAL", "Undead", {
                { "pitch", { -4 } }, { "formant", { -3 } }, { "grit", { 40 } }, { "breath", { 30 } },
                { "instab", { 50, 3 } }, { "revverb", { 0.8f, 30 } }, { "reverb", { 2.0f, 20, 50 } } } },
            { "HORROR & SUPERNATURAL", "Possessed", {
                { "voices", { 2, 12, 80 } }, { "growl", { 30, 45 } }, { "dist", { 6, 6000 } },
                { "instab", { 35, 2.5f } }, { "ringmod", { 35, 15 } }, { "revverb", { 1.2f, 35 } } } },
            { "HORROR & SUPERNATURAL", "Ancient Necromancer", {
                { "pitch", { -5 } }, { "formant", { -6 } }, { "grit", { 20 } }, { "breath", { 25 } },
                { "shimmer", { -12, 6, 25 } }, { "reverb", { 5.0f, 30, 45 } }, { "echo", { 600, 30, 15 } } } },
            { "HORROR & SUPERNATURAL", "Eldritch Horror", {
                { "voices", { 5, 12, 70 } }, { "subharm", { 50, 2 } }, { "instab", { 45, 1.2f } },
                { "fshift", { -30, 60 } }, { "ringmod", { 23, 25 } }, { "freeze", { 70, 30 } },
                { "reverb", { 6.0f, 30, 40 } } } },
            { "HORROR & SUPERNATURAL", "Demonic Entity", {
                { "pitch", { -12 } }, { "formant", { -7 } }, { "bass", { 6, 90 } }, { "voices", { 3, 7, 60 } },
                { "subharm", { 60, 1 } }, { "growl", { 50, 50 } }, { "dist", { 14, 4500 } },
                { "revverb", { 1.5f, 45 } }, { "reverb", { 3.0f, 25, 40 } } } },
            // ------------------------------------------------ SCIENCE FICTION
            { "SCIENCE FICTION", "Robot", {
                { "robot", { 100, 110 } }, { "ringmod", { 50, 20 } }, { "crush", { 10, 22050, 40 } },
                { "band", { 150, 8000 } }, { "comp", { 50 } }, { "reverb", { 0.6f, 10, 50 } } } },
            { "SCIENCE FICTION", "Alien", {
                { "pitch", { 4 } }, { "formant", { 6 } }, { "fshift", { 60, 50 } },
                { "flanger", { 0.3f, 60, 60 } }, { "ringmod", { 260, 15 } }, { "echo", { 300, 30, 18 } } } },
            { "SCIENCE FICTION", "Cyborg", {
                { "robot", { 45, 95 } }, { "dist", { 6, 7000 } }, { "flanger", { 0.15f, 50, 40 } },
                { "crush", { 12, 22050, 30 } }, { "comp", { 60 } } } },
            { "SCIENCE FICTION", "Artificial Intelligence", {
                { "formant", { 1 } }, { "robot", { 25, 140 } }, { "chorus", { 0.5f, 30, 30 } },
                { "crush", { 14, 32000, 20 } }, { "delay", { 120, 20, 15 } }, { "reverb", { 1.5f, 15, 60 } },
                { "comp", { 50 } } } },
            { "SCIENCE FICTION", "Space Communications", {
                { "crush", { 8, 8000, 30 } }, { "reson", { 2, 60 } }, { "band", { 400, 3200 } },
                { "comp", { 75 } }, { "dropout", { 20, 70 } }, { "hiss", { -40 } } } },
            { "SCIENCE FICTION", "Damaged Android", {
                { "instab", { 50, 6 } }, { "robot", { 70, 120 } }, { "fshift", { -20, 30 } },
                { "crush", { 6, 6000, 40 } }, { "dropout", { 45, 90 } } } },
            // ------------------------------------------------ MEDIA & VINTAGE
            { "MEDIA & VINTAGE AUDIO", "VHS - Worn Tape", {
                { "wow", { 30, 0.5f } }, { "flutter", { 18, 9 } }, { "tapesat", { 45 } }, { "band", { 80, 10000 } },
                { "comp", { 30 } }, { "dropout", { 10, 50 } }, { "hiss", { -40 } } } },
            { "MEDIA & VINTAGE AUDIO", "VHS - Tracking Failure", {
                { "instab", { 25, 4 } }, { "wow", { 60, 0.7f } }, { "flutter", { 35, 11 } }, { "tapesat", { 60 } },
                { "band", { 120, 7000 } }, { "dropout", { 55, 90 } }, { "hiss", { -34 } }, { "crackle", { 20, -30 } } } },
            { "MEDIA & VINTAGE AUDIO", "Cassette - Old Recording", {
                { "wow", { 20, 0.7f } }, { "flutter", { 15, 11 } }, { "tapesat", { 40 } }, { "band", { 60, 9000 } },
                { "comp", { 40 } }, { "hiss", { -36 } } } },
            { "MEDIA & VINTAGE AUDIO", "Vinyl - Haunted Record", {
                { "reverb", { 3.0f, 20, 50 } }, { "wow", { 25, 0.55f } }, { "tapesat", { 20 } }, { "band", { 80, 9000 } },
                { "hiss", { -50 } }, { "crackle", { 45, -22 } } } },
            { "MEDIA & VINTAGE AUDIO", "78 RPM Antique Record", {
                { "dist", { 4, 6000 } }, { "wow", { 15, 1.3f } }, { "reson", { 4, 80 } }, { "band", { 300, 4500 } },
                { "hiss", { -32 } }, { "crackle", { 70, -18 } } } },
            { "MEDIA & VINTAGE AUDIO", "AM Radio", {
                { "tapesat", { 20 } }, { "reson", { 2, 80 } }, { "band", { 200, 4500 } }, { "comp", { 70 } }, { "hiss", { -45 } } } },
            { "MEDIA & VINTAGE AUDIO", "Shortwave Radio", {
                { "fshift", { 35, 100 } }, { "band", { 300, 3000 } }, { "comp", { 60 } }, { "dropout", { 30, 70 } },
                { "hiss", { -30 } }, { "crackle", { 30, -30 } } } },
            { "MEDIA & VINTAGE AUDIO", "Walkie-Talkie", {
                { "dist", { 14, 3500 } }, { "crush", { 10, 16000, 30 } }, { "reson", { 3, 90 } }, { "band", { 400, 3000 } },
                { "comp", { 80 } }, { "hiss", { -42 } } } },
            { "MEDIA & VINTAGE AUDIO", "Old Telephone (1990s)", {
                { "dist", { 4, 3400 } }, { "crush", { 13, 8000, 50 } }, { "reson", { 0, 90 } }, { "band", { 300, 3400 } },
                { "comp", { 60 } }, { "hiss", { -55 } } } },
            { "MEDIA & VINTAGE AUDIO", "Vintage Megaphone", {
                { "dist", { 18, 4000 } }, { "reverb", { 0.8f, 10, 40 } }, { "reson", { 1, 100 } }, { "band", { 500, 4000 } },
                { "comp", { 85 } } } },
            { "MEDIA & VINTAGE AUDIO", "Broken Digital Audio", {
                { "ringmod", { 900, 10 } }, { "crush", { 5, 7000, 70 } }, { "freeze", { 30, 20 } }, { "dropout", { 60, 100 } } } },
            { "MEDIA & VINTAGE AUDIO", "Damaged Film Soundtrack", {
                { "wow", { 12, 0.4f } }, { "flutter", { 30, 18 } }, { "tapesat", { 25 } }, { "band", { 120, 6000 } },
                { "comp", { 40 } }, { "dropout", { 20, 60 } }, { "hiss", { -34 } }, { "crackle", { 50, -24 } } } },
        };
        return d;
    }
}

const std::vector<PresetInfo>& builtInPresets()
{
    static std::vector<PresetInfo> list = []
    {
        std::vector<PresetInfo> r;
        for (auto& d : defs()) r.push_back ({ d.category, d.name });
        return r;
    }();
    return list;
}

Layer makeBuiltInPreset (const juce::String& name)
{
    Layer l;
    l.name = name;
    l.presetName = name;
    l.seed = (juce::uint32) name.hashCode();
    for (auto& d : defs())
    {
        if (name != d.name) continue;
        for (auto& f : d.fx)
        {
            int e = effectIndexFromId (f.id);
            jassert (e >= 0);
            if (e < 0) continue;
            l.fx[(size_t) e].on = true;
            for (size_t i = 0; i < f.vals.size() && i < (size_t) kMaxParams; ++i)
                if (! std::isnan (f.vals[i]))
                {
                    auto& ps = effectTable()[(size_t) e].p[i];
                    l.fx[(size_t) e].p[i] = juce::jlimit (ps.min, ps.max, f.vals[i]);
                }
        }
    }
    l.captureStart();
    return l;
}

juce::File userDataFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Monster Lab");
    f.createDirectory();
    return f;
}
juce::File userLayerPresetFolder() { auto f = userDataFolder().getChildFile ("Layer Presets"); f.createDirectory(); return f; }
juce::File userCharacterFolder()   { auto f = userDataFolder().getChildFile ("Characters");    f.createDirectory(); return f; }
} // namespace ml
