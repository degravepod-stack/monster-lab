// Monster Lab — universal effect catalogue.
// Every layer carries one EffectState for every entry in this table.
// Presets only decide which effects start switched on and with what values.
#pragma once
#include <array>
#include <cstring>

namespace ml
{
enum Section { Voice = 0, Texture, SciFi, Space, Media, Samples, NumSections };

inline const char* sectionName (int s)
{
    static const char* n[] = { "VOICE", "TEXTURE", "SCI-FI", "SPACE", "MEDIA", "SAMPLES" };
    return n[s];
}

constexpr int kMaxParams = 3;

struct ParamSpec
{
    const char* label = nullptr;   // nullptr = unused slot
    float min = 0, max = 1, def = 0, step = 0.01f;
    const char* unit = "";
    float centre = 0;              // value at slider middle (skew); 0 = linear
};

struct EffectSpec
{
    const char* id;
    const char* label;
    Section section;
    ParamSpec p[kMaxParams];
};

// Order inside each section = processing order inside the layer.
// Section processing order: Voice -> Texture -> Sci-Fi -> Space -> Media
// (so tape/radio/phone colour sits on top of reverbs, like a real recording).
enum EffectId
{
    // Voice
    E_Pitch, E_Formant, E_Size, E_Bass, E_Voices,
    // Texture
    E_Grit, E_Growl, E_Breath, E_Distortion, E_Instability, E_Subharm,
    // Sci-fi
    E_Robot, E_RingMod, E_FreqShift, E_Bitcrush, E_Flanger, E_Chorus,
    // Space
    E_Freeze, E_Shimmer, E_RevReverb, E_Reverb, E_Echo, E_Delay, E_Stereo,
    // Media
    E_Speed, E_Wow, E_Flutter, E_Warp, E_TapeSat, E_Resonance, E_Bandwidth, E_Compress,
    E_Dropouts, E_Hiss, E_Crackle,
    // Samples (user sound files)
    E_Sample1, E_Sample2,
    NumEffects
};

// Sample modes (Samples > Sample "Mode" param)
inline const char* sampleModeName (int m)
{
    static const char* n[] = { "Bed", "Through", "Granular" };
    return n[m < 0 ? 0 : (m > 2 ? 2 : m)];
}
inline bool isSampleEffect (int e) { return e == E_Sample1 || e == E_Sample2; }

// Resonance types (Media > Resonance "Type" param)
inline const char* resonanceTypeName (int t)
{
    static const char* n[] = { "Telephone", "Megaphone", "AM Radio", "Walkie", "Gramophone" };
    return n[t < 0 ? 0 : (t > 4 ? 4 : t)];
}

inline const std::array<EffectSpec, NumEffects>& effectTable()
{
    static const std::array<EffectSpec, NumEffects> t {{
        // ---------------- VOICE
        { "pitch",   "Pitch",          Voice,   {{ "Semitones", -24, 24, -5, 0.1f, "st" }} },
        { "formant", "Formant",        Voice,   {{ "Shift", -12, 12, -4, 0.1f, "st" }} },
        { "size",    "Creature Size",  Voice,   {{ "Size", -100, 100, 50, 1, "%" }} },
        { "bass",    "Bass",           Voice,   {{ "Boost", 0, 18, 6, 0.1f, "dB" }, { "Freq", 40, 300, 110, 1, "Hz" }} },
        { "voices",  "Multiple Voices",Voice,   {{ "Copies", 1, 6, 2, 1, "" }, { "Detune", 0, 24, 7, 0.1f, "st" }, { "Blend", 0, 100, 55, 1, "%" }} },
        // ---------------- TEXTURE
        { "grit",    "Grit",           Texture, {{ "Amount", 0, 100, 35, 1, "%" }} },
        { "growl",   "Growl",          Texture, {{ "Amount", 0, 100, 45, 1, "%" }, { "Rate", 15, 90, 40, 1, "Hz" }} },
        { "breath",  "Breath / Whisper",Texture,{{ "Amount", 0, 100, 35, 1, "%" }} },
        { "dist",    "Distortion",     Texture, {{ "Drive", 0, 36, 10, 0.1f, "dB" }, { "Tone", 1000, 16000, 6000, 10, "Hz", 5000 }} },
        { "instab",  "Instability",    Texture, {{ "Depth", 0, 100, 30, 1, "%" }, { "Speed", 0.1f, 8, 1.5f, 0.01f, "Hz", 1.5f }} },
        { "subharm", "Subharmonics",   Texture, {{ "Level", 0, 100, 45, 1, "%" }, { "Octaves", 1, 2, 1, 1, "" }} },
        // ---------------- SCI-FI
        { "robot",   "Robot",          SciFi,   {{ "Amount", 0, 100, 100, 1, "%" }, { "Pitch", 50, 300, 110, 1, "Hz" }} },
        { "ringmod", "Ring Mod",       SciFi,   {{ "Freq", 10, 2000, 70, 1, "Hz", 200 }, { "Mix", 0, 100, 50, 1, "%" }} },
        { "fshift",  "Freq Shift",     SciFi,   {{ "Shift", -500, 500, -40, 1, "Hz" }, { "Mix", 0, 100, 100, 1, "%" }} },
        { "crush",   "Bitcrush",       SciFi,   {{ "Bits", 2, 16, 8, 1, "bit" }, { "Rate", 1000, 44100, 11025, 1, "Hz", 8000 }, { "Mix", 0, 100, 100, 1, "%" }} },
        { "flanger", "Flanger",        SciFi,   {{ "Rate", 0.05f, 5, 0.25f, 0.01f, "Hz", 0.5f }, { "Depth", 0, 100, 70, 1, "%" }, { "Feedback", -95, 95, 50, 1, "%" }} },
        { "chorus",  "Chorus",         SciFi,   {{ "Rate", 0.05f, 5, 0.8f, 0.01f, "Hz", 0.8f }, { "Depth", 0, 100, 40, 1, "%" }, { "Mix", 0, 100, 40, 1, "%" }} },
        // ---------------- SPACE
        { "freeze",  "Spectral Freeze",Space,   {{ "Hold", 0, 100, 60, 1, "%" }, { "Mix", 0, 100, 40, 1, "%" }} },
        { "shimmer", "Shimmer Reverb", Space,   {{ "Interval", -24, 24, 12, 1, "st" }, { "Decay", 0.5f, 15, 5, 0.1f, "s", 4 }, { "Mix", 0, 100, 35, 1, "%" }} },
        { "revverb", "Reverse Reverb", Space,   {{ "Length", 0.3f, 4, 1.5f, 0.05f, "s" }, { "Mix", 0, 100, 60, 1, "%" }} },
        { "reverb",  "Reverb",         Space,   {{ "Decay", 0.2f, 15, 2.2f, 0.1f, "s", 2.5f }, { "Mix", 0, 100, 25, 1, "%" }, { "Tone", 0, 100, 55, 1, "%" }} },
        { "echo",    "Echo",           Space,   {{ "Time", 50, 1500, 380, 1, "ms" }, { "Feedback", 0, 90, 40, 1, "%" }, { "Mix", 0, 100, 25, 1, "%" }} },
        { "delay",   "Delay",          Space,   {{ "Time", 20, 1500, 250, 1, "ms" }, { "Feedback", 0, 90, 30, 1, "%" }, { "Mix", 0, 100, 25, 1, "%" }} },
        { "stereo",  "Stereo Movement",Space,   {{ "Rate", 0.02f, 5, 0.3f, 0.01f, "Hz", 0.5f }, { "Width", 0, 100, 60, 1, "%" }} },
        // ---------------- MEDIA
        { "speed",   "Playback Speed", Media,   {{ "Speed", 50, 200, 90, 1, "%", 100 }} },
        { "wow",     "Wow",            Media,   {{ "Depth", 0, 100, 30, 1, "%" }, { "Rate", 0.1f, 3, 0.6f, 0.01f, "Hz" }} },
        { "flutter", "Flutter",        Media,   {{ "Depth", 0, 100, 25, 1, "%" }, { "Rate", 4, 25, 9, 0.1f, "Hz" }} },
        { "warp",    "Tape Warp",      Media,   {{ "Frequency", 0, 100, 30, 1, "%" }, { "Depth", 0, 100, 40, 1, "%" }, { "Length", 0.1f, 2, 0.5f, 0.01f, "s" }} },
        { "tapesat", "Tape Saturation",Media,   {{ "Drive", 0, 100, 40, 1, "%" }} },
        { "reson",   "Resonance",      Media,   {{ "Type", 0, 4, 0, 1, "" }, { "Amount", 0, 100, 80, 1, "%" }} },
        { "band",    "Bandwidth",      Media,   {{ "Low cut", 20, 2000, 300, 1, "Hz", 200 }, { "High cut", 1000, 20000, 3400, 10, "Hz", 5000 }} },
        { "comp",    "Compression",    Media,   {{ "Amount", 0, 100, 50, 1, "%" }} },
        { "dropout", "Dropouts",       Media,   {{ "Frequency", 0, 100, 25, 1, "%" }, { "Depth", 0, 100, 80, 1, "%" }} },
        { "hiss",    "Hiss",           Media,   {{ "Level", -100, -40, -76.5f, 0.5f, "dB" }} },
        { "crackle", "Crackle",        Media,   {{ "Density", 0, 100, 30, 1, "%" }, { "Level", -80, -20, -45, 0.5f, "dB" }} },
        // ---------------- SAMPLES (sound file chosen per effect, stored separately)
        { "sample1", "Sample A",       Samples, {{ "Mode", 0, 2, 1, 1, "" }, { "Amount", 0, 100, 50, 1, "%" }, { "Pitch", -24, 24, 0, 0.1f, "st" }} },
        { "sample2", "Sample B",       Samples, {{ "Mode", 0, 2, 0, 1, "" }, { "Amount", 0, 100, 40, 1, "%" }, { "Pitch", -24, 24, 0, 0.1f, "st" }} },
    }};
    return t;
}

inline int effectIndexFromId (const char* id)
{
    const auto& t = effectTable();
    for (int i = 0; i < NumEffects; ++i)
        if (std::strcmp (t[(size_t) i].id, id) == 0)
            return i;
    return -1;
}

inline int numParams (int effect)
{
    int n = 0;
    for (auto& p : effectTable()[(size_t) effect].p)
        if (p.label != nullptr) ++n;
    return n;
}
} // namespace ml
