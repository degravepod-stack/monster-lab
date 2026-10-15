// Monster Lab — offline render engine.
// The whole stack is rendered from the untouched original into a new buffer.
// Offline (whole-file) rendering is deliberate: it allows true reverse reverb,
// high-quality Rubber Band pitch/formant processing and identical preview/export.
#pragma once
#include "Model.h"
#include <atomic>
#include <functional>

namespace ml
{
struct RenderOutput
{
    juce::AudioBuffer<float> audio;  // always stereo
    int preRoll = 0;                 // samples of lead-in added before the original's sample 0 (reverse reverb swell)
    double sampleRate = 44100.0;
    bool limited = false;            // true if the output was scaled down to avoid clipping
};

// Returns false if aborted. `src` may be mono or stereo and is never modified.
bool renderStack (const juce::AudioBuffer<float>& src, double sampleRate, const Stack& stack,
                  RenderOutput& out, const std::atomic<bool>* abort = nullptr,
                  std::function<void (float)> progress = {});

// Renderer that remembers the output of each layer, so editing the top layer
// doesn't re-render everything beneath it. Call clear() when the source changes.
class StackRenderer
{
public:
    bool render (const juce::AudioBuffer<float>& src, double sampleRate, const Stack& stack,
                 RenderOutput& out, const std::atomic<bool>* abort = nullptr,
                 std::function<void (float)> progress = {});
    void clear() { cache.clear(); }
private:
    struct Entry { juce::String key; juce::AudioBuffer<float> audio; int preRoll = 0; };
    std::vector<Entry> cache;
};

// Process one layer in place (stereo). Exposed for tests.
void processLayer (const Layer& layer, juce::AudioBuffer<float>& io, double sr, int& preRoll,
                   const std::atomic<bool>* abort);
} // namespace ml
