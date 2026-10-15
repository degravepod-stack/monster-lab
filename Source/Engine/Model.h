// Monster Lab — data model: layers, the stack, presets, save/load.
#pragma once
#include <JuceHeader.h>
#include "Effects.h"

namespace ml
{
struct EffectState
{
    bool on = false;
    float p[kMaxParams] {};
};

struct Layer
{
    juce::String name { "Layer" };
    juce::String presetName;           // which preset it started from (informational)
    bool enabled = true;
    float mix = 100.0f;                // wet/dry, 0..100 %
    bool sectionOn[NumSections] { true, true, true, true, true };
    std::array<EffectState, NumEffects> fx;
    juce::uint32 seed = 1;             // keeps random textures stable between renders

    Layer();                           // all effects off, params at defaults
    bool isActive (int e) const { return enabled && fx[(size_t) e].on && sectionOn[effectTable()[(size_t) e].section]; }
    float v (int e, int i) const       { return fx[(size_t) e].p[i]; }

    juce::var toVar() const;
    static Layer fromVar (const juce::var&);
};

using Stack = std::vector<Layer>;

juce::var stackToVar (const Stack&);
Stack stackFromVar (const juce::var&);

// ---------------- built-in presets
struct PresetInfo { juce::String category, name; };
const std::vector<PresetInfo>& builtInPresets();
Layer makeBuiltInPreset (const juce::String& name);

// ---------------- user files
juce::File userDataFolder();         // ~/Documents/Monster Lab
juce::File userLayerPresetFolder();  // ~/Documents/Monster Lab/Layer Presets
juce::File userCharacterFolder();    // ~/Documents/Monster Lab/Characters
} // namespace ml
