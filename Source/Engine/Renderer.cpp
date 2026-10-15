#include "Renderer.h"
#include <rubberband/RubberBandStretcher.h>
#include <complex>
#include <cmath>

namespace ml
{
using Buf = juce::AudioBuffer<float>;
static constexpr double twoPi = juce::MathConstants<double>::twoPi;

static inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
static inline bool aborted (const std::atomic<bool>* a) { return a != nullptr && a->load(); }

//==============================================================================
// Small DSP building blocks
//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double s1[2] {}, s2[2] {};

    void setRaw (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }

    static double clampF (double sr, double f) { return juce::jlimit (5.0, sr * 0.47, f); }

    static Biquad lp (double sr, double f, double q = 0.7071)
    { Biquad b; double w = twoPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
      b.setRaw ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); return b; }
    static Biquad hp (double sr, double f, double q = 0.7071)
    { Biquad b; double w = twoPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
      b.setRaw ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); return b; }
    static Biquad bp (double sr, double f, double q)
    { Biquad b; double w = twoPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
      b.setRaw (al, 0, -al, 1 + al, -2 * c, 1 - al); return b; }
    static Biquad peak (double sr, double f, double q, double db)
    { Biquad b; double A = std::pow (10.0, db / 40), w = twoPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
      b.setRaw (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); return b; }
    static Biquad lowShelf (double sr, double f, double db)
    { Biquad b; double A = std::pow (10.0, db / 40), w = twoPi * clampF (sr, f) / sr, c = std::cos (w), s = std::sin (w);
      double al = s / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
      b.setRaw (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq); return b; }

    inline float tick (float x, int ch)
    {
        double y = b0 * x + s1[ch];
        s1[ch] = b1 * x - a1 * y + s2[ch];
        s2[ch] = b2 * x - a2 * y;
        return (float) y;
    }
    void run (Buf& b)
    {
        for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
        {
            auto* d = b.getWritePointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i) d[i] = tick (d[i], ch);
        }
    }
};

// Smoothly interpolated random value in [-1, 1] changing `rate` times per second.
struct SmoothNoise
{
    juce::Random r;
    double phase = 0, inc = 0;
    float a = 0, b = 0;
    SmoothNoise (juce::int64 seed, double rateHz, double sr) : r (seed), inc (rateHz / sr)
    { a = r.nextFloat() * 2 - 1; b = r.nextFloat() * 2 - 1; }
    float next()
    {
        phase += inc;
        if (phase >= 1.0) { phase -= 1.0; a = b; b = r.nextFloat() * 2 - 1; }
        float t = (float) (0.5 - 0.5 * std::cos (phase * juce::MathConstants<double>::pi));
        return a + (b - a) * t;
    }
};

static inline float readCubic (const float* x, int n, double pos)
{
    int i = (int) std::floor (pos);
    float f = (float) (pos - i);
    auto s = [x, n] (int k) { return (k >= 0 && k < n) ? x[k] : 0.0f; };
    float y0 = s (i - 1), y1 = s (i), y2 = s (i + 1), y3 = s (i + 2);
    float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2 * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

// y[n] = x[n - delay(n, ch)]  (delay in samples, >= 0). Offline, so reads straight from a copy.
template <typename DelayFn>
static void modulatedRead (Buf& b, DelayFn delayFn)
{
    Buf src (b);
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* x = src.getReadPointer (ch);
        float* y = b.getWritePointer (ch);
        const int n = b.getNumSamples();
        for (int i = 0; i < n; ++i)
            y[i] = readCubic (x, n, i - (double) delayFn (i, ch));
    }
}

static float rms (const Buf& b)
{
    double s = 0; long long cnt = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        auto* d = b.getReadPointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i) s += (double) d[i] * d[i];
        cnt += b.getNumSamples();
    }
    return cnt > 0 ? (float) std::sqrt (s / (double) cnt) : 0.0f;
}

static void mixInto (Buf& dst, const Buf& wet, float dryGain, float wetGain)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = dst.getWritePointer (ch);
        auto* w = wet.getReadPointer (ch);
        const int n = juce::jmin (dst.getNumSamples(), wet.getNumSamples());
        for (int i = 0; i < n; ++i) d[i] = d[i] * dryGain + w[i] * wetGain;
    }
}

static void reverse (Buf& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        auto* d = b.getWritePointer (ch);
        std::reverse (d, d + b.getNumSamples());
    }
}

// Index of the last sample louder than `thresh` (or -1).
static int lastAbove (const Buf& b, float thresh)
{
    for (int i = b.getNumSamples() - 1; i >= 0; --i)
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            if (std::abs (b.getSample (ch, i)) > thresh) return i;
    return -1;
}

//==============================================================================
// Rubber Band (R3 "finer" engine): high quality pitch shifting with independent
// formant control, duration preserved.
//==============================================================================
static Buf rubberBand (const Buf& in, double sr, double pitchScale, double formantScale,
                       bool preserveFormants, bool fastEngine, const std::atomic<bool>* abort)
{
    using RB = RubberBand::RubberBandStretcher;
    const int n = in.getNumSamples();
    RB::Options opts = RB::OptionProcessOffline | RB::OptionChannelsTogether | RB::OptionPitchHighQuality
                     | (fastEngine ? RB::OptionEngineFaster : RB::OptionEngineFiner)
                     | (preserveFormants ? RB::OptionFormantPreserved : RB::OptionFormantShifted);
    RB s ((size_t) sr, 2, opts, 1.0, pitchScale);
    if (! fastEngine && formantScale > 0.0)
        s.setFormantScale (formantScale);

    const int block = 8192;
    s.setExpectedInputDuration ((size_t) n);
    s.setMaxProcessSize ((size_t) block);

    for (int pos = 0; pos < n; pos += block)
    {
        int len = juce::jmin (block, n - pos);
        const float* ptrs[2] = { in.getReadPointer (0, pos), in.getReadPointer (1, pos) };
        s.study (ptrs, (size_t) len, pos + len >= n);
    }

    Buf out (2, n + 1024);
    out.clear();
    int written = 0;
    auto drain = [&]
    {
        int avail;
        while ((avail = s.available()) > 0)
        {
            if (written + avail > out.getNumSamples())
                out.setSize (2, written + avail + 8192, true, true);
            float* optr[2] = { out.getWritePointer (0, written), out.getWritePointer (1, written) };
            written += (int) s.retrieve (optr, (size_t) avail);
        }
    };

    for (int pos = 0; pos < n; pos += block)
    {
        if (aborted (abort)) return in;
        int len = juce::jmin (block, n - pos);
        const float* ptrs[2] = { in.getReadPointer (0, pos), in.getReadPointer (1, pos) };
        s.process (ptrs, (size_t) len, pos + len >= n);
        drain();
    }
    drain();

    out.setSize (2, n, true, true);   // offline mode is latency-compensated: same length as input
    if (written < n)
        for (int ch = 0; ch < 2; ++ch) out.clear (ch, written, n - written);
    return out;
}

//==============================================================================
// STFT helper (Hann analysis + synthesis, overlap-add, exact normalisation)
//==============================================================================
template <typename BinFn>
static void stft (const float* x, float* y, int n, int order, int hop, BinFn fn)
{
    const int N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) N), buf ((size_t) N * 2);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos ((float) twoPi * i / N);

    std::vector<float> acc ((size_t) (n + N), 0.0f), norm ((size_t) (n + N), 0.0f);
    for (int pos = -N + hop; pos < n; pos += hop)
    {
        for (int i = 0; i < N; ++i)
        {
            int k = pos + i;
            buf[(size_t) i] = (k >= 0 && k < n) ? x[k] * win[(size_t) i] : 0.0f;
        }
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        fft.performRealOnlyForwardTransform (buf.data(), true);
        fn (reinterpret_cast<std::complex<float>*> (buf.data()), N / 2 + 1);
        fft.performRealOnlyInverseTransform (buf.data());
        for (int i = 0; i < N; ++i)
        {
            int k = pos + i;
            if (k >= 0 && k < n)
            {
                acc[(size_t) k] += buf[(size_t) i] * win[(size_t) i];
                norm[(size_t) k] += win[(size_t) i] * win[(size_t) i];
            }
        }
    }
    for (int i = 0; i < n; ++i)
        y[i] = norm[(size_t) i] > 1.0e-3f ? acc[(size_t) i] / norm[(size_t) i] : 0.0f;
}

static int fftOrderFor (double sr, int base44k)   // keeps time resolution similar at 44.1/48/96k
{
    return sr > 60000 ? base44k + 1 : base44k;
}

//==============================================================================
// Feedback-delay-network reverb (8 lines, Householder matrix, input diffusion).
// Writes a 100% wet stereo signal.
//==============================================================================
static Buf fdnReverb (const Buf& in, double sr, float decaySec, float tone01, float predelayMs = 15.0f)
{
    const int n = in.getNumSamples();
    Buf wet (2, n);
    wet.clear();

    static const float lineMs[8]  = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.1f, 73.3f };
    static const float diffMs[4]  = { 4.7f, 3.6f, 12.7f, 9.3f };
    const float scale = juce::jlimit (0.6f, 1.6f, 0.75f + decaySec * 0.08f);   // bigger rooms for longer decays

    std::vector<std::vector<float>> lines (8), diff (4);
    int len[8], pos[8] {}, dlen[4], dpos[4] {};
    float g[8], lpState[8] {};
    for (int i = 0; i < 8; ++i)
    {
        len[i] = juce::jmax (8, (int) (lineMs[i] * scale * 0.001 * sr));
        lines[(size_t) i].assign ((size_t) len[i], 0.0f);
        g[i] = std::pow (10.0f, -3.0f * (float) len[i] / (float) (sr * juce::jmax (0.1f, decaySec)));
    }
    for (int i = 0; i < 4; ++i)
    {
        dlen[i] = juce::jmax (4, (int) (diffMs[i] * 0.001 * sr));
        diff[(size_t) i].assign ((size_t) dlen[i], 0.0f);
    }
    // tone: 0 = dark, 100% = bright. one-pole lowpass inside the loop.
    const float cutoff = 1500.0f + tone01 * 12000.0f;
    const float lpA = std::exp (-(float) twoPi * cutoff / (float) sr);
    const int pre = (int) (predelayMs * 0.001 * sr);

    for (int i = 0; i < n; ++i)
    {
        int src = i - pre;
        float x = src >= 0 ? 0.5f * (in.getSample (0, src) + in.getSample (1, src)) : 0.0f;

        for (int d = 0; d < 4; ++d)   // series allpass diffusion
        {
            auto& buf = diff[(size_t) d];
            float delayed = buf[(size_t) dpos[d]];
            float v = x + 0.6f * delayed;
            buf[(size_t) dpos[d]] = v;
            x = delayed - 0.6f * v;
            if (++dpos[d] >= dlen[d]) dpos[d] = 0;
        }

        float o[8], sum = 0;
        for (int l = 0; l < 8; ++l)
        {
            float v = lines[(size_t) l][(size_t) pos[l]];
            lpState[l] = v * (1.0f - lpA) + lpState[l] * lpA;
            o[l] = lpState[l] * g[l];
            sum += o[l];
        }
        const float h = sum * 0.25f;   // Householder: o - 2/N * sum
        float outL = 0, outR = 0;
        for (int l = 0; l < 8; ++l)
        {
            float fb = o[l] - h;
            lines[(size_t) l][(size_t) pos[l]] = fb + x * ((l & 1) ? -0.35f : 0.35f);
            if (++pos[l] >= len[l]) pos[l] = 0;
            if (l & 1) outR += o[l]; else outL += o[l];
        }
        wet.setSample (0, i, outL * 0.5f);
        wet.setSample (1, i, outR * 0.5f);
    }
    return wet;
}

//==============================================================================
// Niemitalo IIR Hilbert pair (90 degree phase difference) for frequency shifting.
//==============================================================================
struct HilbertPair
{
    struct AP { double c, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                double tick (double x) { double y = c * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
    AP a[4] { { 0.6923878 * 0.6923878 }, { 0.9360654322959 * 0.9360654322959 }, { 0.9882295226860 * 0.9882295226860 }, { 0.9987488452737 * 0.9987488452737 } };
    AP b[4] { { 0.4021921162426 * 0.4021921162426 }, { 0.8561710882420 * 0.8561710882420 }, { 0.9722909545651 * 0.9722909545651 }, { 0.9952884791278 * 0.9952884791278 } };
    double bDelay = 0;
    void tick (double x, double& re, double& im)
    {
        double ya = x, yb = x;
        for (auto& s : a) ya = s.tick (ya);
        for (auto& s : b) yb = s.tick (yb);
        re = ya; im = bDelay; bDelay = yb;
    }
};

//==============================================================================
// The effects
//==============================================================================
namespace fx
{
static void bass (Buf& b, double sr, float db, float freq)
{
    auto f = Biquad::lowShelf (sr, freq, db);
    f.run (b);
}

static void sizeResonance (Buf& b, double sr, float size01)   // -1..1
{
    if (std::abs (size01) < 0.01f) return;
    auto f = size01 > 0 ? Biquad::peak (sr, 95, 0.9, size01 * 5.0) : Biquad::peak (sr, 3500, 0.8, -size01 * 4.0);
    f.run (b);
}

static void voices (Buf& b, double sr, int copies, float detune, float blend01, juce::uint32 seed, const std::atomic<bool>* abort)
{
    juce::Random r ((juce::int64) seed * 31 + 7);
    Buf sum (2, b.getNumSamples());
    sum.clear();
    for (int i = 1; i <= copies; ++i)
    {
        if (aborted (abort)) return;
        float st = -detune * (float) i / (float) copies + (r.nextFloat() - 0.5f) * 0.25f;
        Buf c = rubberBand (b, sr, std::pow (2.0, st / 12.0), 0.0, false, true, abort);
        const int delay = (int) ((6 + 7 * i + r.nextFloat() * 5) * 0.001 * sr);
        const float pan = ((i & 1) ? -1.0f : 1.0f) * juce::jmin (0.8f, 0.25f + 0.12f * (float) i);
        const float gl = std::cos ((pan + 1) * juce::MathConstants<float>::pi / 4) * 1.414f;
        const float gr = std::sin ((pan + 1) * juce::MathConstants<float>::pi / 4) * 1.414f;
        for (int s = 0; s + delay < b.getNumSamples(); ++s)
        {
            sum.addSample (0, s + delay, c.getSample (0, s) * gl);
            sum.addSample (1, s + delay, c.getSample (1, s) * gr);
        }
    }
    mixInto (b, sum, 1.0f - 0.5f * blend01, blend01 / std::sqrt ((float) copies));
}

static void envelope (const Buf& b, std::vector<float>& env, double sr, float attMs, float relMs)
{
    env.assign ((size_t) b.getNumSamples(), 0.0f);
    float e = 0, a = std::exp (-1.0f / (float) (attMs * 0.001 * sr)), rl = std::exp (-1.0f / (float) (relMs * 0.001 * sr));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        float x = 0.5f * (std::abs (b.getSample (0, i)) + std::abs (b.getSample (1, i)));
        e = x > e ? a * e + (1 - a) * x : rl * e + (1 - rl) * x;
        env[(size_t) i] = e;
    }
}

static void grit (Buf& b, double sr, float amt, juce::uint32 seed)
{
    // Vocal rasp without added noise:
    //  1. level-tracking saturation of the mid band (harmonics that follow the voice,
    //     so quiet tails and breaths don't crackle)
    //  2. slow, smooth amplitude roughness (20-45 Hz), like an irregular, strained throat
    //  3. gentle low-pass on the new harmonics so it never gets fizzy
    const float a = amt / 100.0f;
    std::vector<float> env;
    envelope (b, env, sr, 5, 80);
    float peakEnv = 1.0e-6f;
    for (auto e : env) peakEnv = juce::jmax (peakEnv, e);

    SmoothNoise rough ((juce::int64) seed, 32.0, sr);
    auto bandHp = Biquad::hp (sr, 350), bandLp = Biquad::lp (sr, 3200), smooth = Biquad::lp (sr, 4500);
    const float drive = 2.0f + 10.0f * a;

    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float e = env[(size_t) i];
        const float gate = juce::jlimit (0.0f, 1.0f, (e / peakEnv - 0.04f) * 8.0f);   // only where there's voice
        const float norm = 1.0f / juce::jmax (0.02f * peakEnv, e);
        const float am = 1.0f + 0.35f * a * gate * rough.next();
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = b.getSample (ch, i);
            float mid = bandLp.tick (bandHp.tick (x, ch), ch);
            float shaped = std::tanh (mid * norm * drive) / (norm * drive) * 2.0f;   // level-matched, harmonic-rich
            float rasp = smooth.tick (shaped - mid, ch);                             // just the added harmonics
            b.setSample (ch, i, (x + a * gate * 1.5f * rasp) * am);
        }
    }
}

static void growl (Buf& b, double sr, float amt, float rate, juce::uint32 seed)
{
    const float d = amt / 100.0f;
    SmoothNoise jitter ((juce::int64) seed, 6.0, sr);
    double ph = 0;
    auto lowLp = Biquad::lp (sr, 700);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        ph += rate * (1.0 + 0.18 * jitter.next()) / sr;
        if (ph >= 1) ph -= 1;
        float s = 0.5f + 0.5f * (float) std::sin (twoPi * ph);
        float shape = s * s * s;                      // narrow pulses: fry / rattle
        float gain = 1.0f - d * 0.85f + d * 0.85f * shape * 1.6f;
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = b.getSample (ch, i);
            float low = lowLp.tick (x, ch);
            b.setSample (ch, i, x * gain + d * 0.6f * std::tanh (4.0f * low * shape));
        }
    }
}

static void breath (Buf& b, double sr, float amt, juce::uint32 seed)
{
    // True whisperisation: keep each STFT frame's magnitudes, randomise the phases.
    const float a = amt / 100.0f;
    const int order = fftOrderFor (sr, 10);
    const int n = b.getNumSamples();
    Buf w (2, n);
    for (int ch = 0; ch < 2; ++ch)
    {
        juce::Random r ((juce::int64) seed + ch * 101);
        stft (b.getReadPointer (ch), w.getWritePointer (ch), n, order, (1 << order) / 4,
              [&r] (std::complex<float>* bins, int nb)
              {
                  for (int k = 1; k < nb - 1; ++k)
                      bins[k] = std::polar (std::abs (bins[k]), r.nextFloat() * (float) twoPi);
                  bins[0] = 0;
              });
    }
    auto hp = Biquad::hp (sr, 350);
    hp.run (w);
    mixInto (b, w, 1.0f - 0.85f * a, a * 1.3f);
}

static void distortion (Buf& b, double sr, float driveDb, float tone)
{
    const float g = dbToGain (driveDb);
    const float comp = 1.0f / std::sqrt (g);
    auto lp = Biquad::lp (sr, tone);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float x = b.getSample (ch, i) * g;
            float y = std::tanh (x + 0.15f * x * x) - std::tanh (0.0f);   // slight asymmetry = even harmonics
            b.setSample (ch, i, lp.tick (y * comp * 1.6f, ch));
        }
}

static void instability (Buf& b, double sr, float depth, float speed, juce::uint32 seed)
{
    const float d = depth / 100.0f;
    SmoothNoise pn ((juce::int64) seed, speed, sr), an ((juce::int64) seed + 9, speed * 1.7, sr);
    const int n = b.getNumSamples();
    std::vector<float> del ((size_t) n), amp ((size_t) n);
    // pitch wobble scaled so faster speeds don't explode the pitch deviation
    const float maxMs = 5.0f * d / juce::jmax (0.5f, std::sqrt (speed));
    for (int i = 0; i < n; ++i)
    {
        del[(size_t) i] = (float) ((maxMs + maxMs * pn.next()) * 0.001 * sr) + 2.0f;
        amp[(size_t) i] = 1.0f - 0.35f * d * (0.5f + 0.5f * an.next());
    }
    modulatedRead (b, [&] (int i, int) { return del[(size_t) i]; });
    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::multiply (b.getWritePointer (ch), amp.data(), n);
}

static void subharmonics (Buf& b, double sr, float level, int octaves, const std::atomic<bool>* abort)
{
    Buf low = rubberBand (b, sr, octaves >= 2 ? 0.25 : 0.5, 0.0, false, true, abort);
    auto lp1 = Biquad::lp (sr, octaves >= 2 ? 350 : 600), lp2 = lp1;
    lp1.run (low); lp2.run (low);
    mixInto (b, low, 1.0f, level / 100.0f * 1.4f);
}

static void robot (Buf& b, double sr, float amt, float pitchHz)
{
    // Robotisation: zero every bin's phase each hop; hop = one period of the target pitch.
    const int order = fftOrderFor (sr, 11);
    const int hop = juce::jlimit (32, (1 << order) - 64, (int) std::round (sr / pitchHz));
    const int n = b.getNumSamples();
    Buf w (2, n);
    for (int ch = 0; ch < 2; ++ch)
        stft (b.getReadPointer (ch), w.getWritePointer (ch), n, order, hop,
              [] (std::complex<float>* bins, int nb) { for (int k = 0; k < nb; ++k) bins[k] = std::abs (bins[k]); });
    // normalise loudness of the robot path to the input
    float ri = rms (b), rw = rms (w);
    float gw = rw > 1e-6f ? juce::jlimit (0.1f, 10.0f, ri / rw) : 1.0f;
    mixInto (b, w, 1.0f - amt / 100.0f, amt / 100.0f * gw);
}

static void ringMod (Buf& b, double sr, float freq, float mix)
{
    const float m = mix / 100.0f;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        float c = (float) std::sin (twoPi * freq * i / sr);
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = b.getSample (ch, i);
            b.setSample (ch, i, x * (1 - m) + x * c * m * 1.4f);
        }
    }
}

static void freqShift (Buf& b, double sr, float shiftHz, float mix)
{
    const float m = mix / 100.0f;
    HilbertPair h[2];
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        double w = twoPi * shiftHz * i / sr, c = std::cos (w), s = std::sin (w);
        for (int ch = 0; ch < 2; ++ch)
        {
            double re, im, x = b.getSample (ch, i);
            h[ch].tick (x, re, im);
            float y = (float) (re * c - im * s);
            b.setSample (ch, i, (float) x * (1 - m) + y * m);
        }
    }
}

static void bitcrush (Buf& b, double sr, float bits, float rate, float mix)
{
    const float m = mix / 100.0f, q = std::pow (2.0f, bits - 1);
    const double inc = juce::jmin (1.0, rate / sr);
    double ph = 1.0;
    float held[2] {};
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        ph += inc;
        bool grab = ph >= 1.0;
        if (grab) ph -= 1.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = b.getSample (ch, i);
            if (grab) held[ch] = std::round (x * q) / q;
            b.setSample (ch, i, x * (1 - m) + held[ch] * m);
        }
    }
}

struct Ring
{
    std::vector<float> d; int w = 0;
    explicit Ring (int size) : d ((size_t) size, 0.0f) {}
    void push (float x) { d[(size_t) w] = x; if (++w >= (int) d.size()) w = 0; }
    float read (double delay) const   // delay in samples, >= 1
    {
        double p = w - delay;
        while (p < 0) p += (double) d.size();
        int i0 = (int) p; float f = (float) (p - i0);
        int i1 = i0 + 1 >= (int) d.size() ? 0 : i0 + 1;
        return d[(size_t) i0] * (1 - f) + d[(size_t) i1] * f;
    }
};

static void flanger (Buf& b, double sr, float rate, float depth, float feedback)
{
    Ring r[2] { Ring ((int) (0.02 * sr)), Ring ((int) (0.02 * sr)) };
    const float fb = feedback / 100.0f, dep = depth / 100.0f;
    float last[2] {};
    for (int i = 0; i < b.getNumSamples(); ++i)
        for (int ch = 0; ch < 2; ++ch)
        {
            double lfo = 0.5 + 0.5 * std::sin (twoPi * rate * i / sr + ch * 0.5);
            double delay = (0.0007 + dep * 0.006 * lfo) * sr;
            float x = b.getSample (ch, i);
            r[ch].push (x + last[ch] * fb);
            float dl = r[ch].read (delay);
            last[ch] = dl;
            b.setSample (ch, i, 0.6f * (x + dl));
        }
}

static void chorus (Buf& b, double sr, float rate, float depth, float mix)
{
    const float m = mix / 100.0f, dep = depth / 100.0f;
    Ring r[2] { Ring ((int) (0.05 * sr)), Ring ((int) (0.05 * sr)) };
    for (int i = 0; i < b.getNumSamples(); ++i)
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = b.getSample (ch, i);
            r[ch].push (x);
            float wet = 0;
            for (int v = 0; v < 3; ++v)
            {
                double lfo = std::sin (twoPi * rate * (1.0 + 0.13 * v) * i / sr + v * 2.1 + ch * 1.3);
                wet += r[ch].read ((0.012 + 0.006 * v + dep * 0.006 * (0.5 + 0.5 * lfo)) * sr);
            }
            b.setSample (ch, i, x * (1 - 0.5f * m) + wet / 3.0f * m * 1.2f);
        }
}

static void spectralFreeze (Buf& b, double sr, float hold, float mix, juce::uint32 seed)
{
    const int order = fftOrderFor (sr, 11);
    const int N = 1 << order, hop = N / 4, n = b.getNumSamples();
    const double holdSec = hold >= 99.5f ? 1.0e9 : 0.1 * std::pow (300.0, hold / 100.0);
    const float decay = (float) std::exp (-6.9 * hop / (sr * holdSec));
    Buf w (2, n);
    for (int ch = 0; ch < 2; ++ch)
    {
        juce::Random r ((juce::int64) seed + ch);
        std::vector<float> held ((size_t) (N / 2 + 1), 0.0f);
        stft (b.getReadPointer (ch), w.getWritePointer (ch), n, order, hop,
              [&] (std::complex<float>* bins, int nb)
              {
                  for (int k = 0; k < nb; ++k)
                  {
                      held[(size_t) k] = juce::jmax (std::abs (bins[k]), held[(size_t) k] * decay);
                      bins[k] = std::polar (held[(size_t) k], r.nextFloat() * (float) twoPi);
                  }
              });
    }
    float ri = rms (b), rw = rms (w);
    float gw = rw > 1e-6f ? juce::jlimit (0.05f, 4.0f, ri / rw) : 1.0f;
    mixInto (b, w, 1.0f - 0.5f * mix / 100.0f, mix / 100.0f * gw);
}

static void reverb (Buf& b, double sr, float decay, float mix, float tone)
{
    Buf wet = fdnReverb (b, sr, decay, tone / 100.0f);
    const float m = mix / 100.0f;
    mixInto (b, wet, 1.0f - 0.6f * m, m * 1.2f);
}

static void reverseReverb (Buf& b, double sr, float length, float mix)
{
    // reverse -> reverb (wet only) -> reverse: the tail now swells INTO each word.
    Buf r (b);
    reverse (r);
    Buf wet = fdnReverb (r, sr, length, 0.6f, 0.0f);
    reverse (wet);
    // trim the swell so it ends right at the attack: shift it later by a few ms
    const float m = mix / 100.0f;
    mixInto (b, wet, 1.0f - 0.3f * m, m * 1.6f);
}

static void shimmer (Buf& b, double sr, float interval, float decay, float mix, const std::atomic<bool>* abort)
{
    Buf r1 = fdnReverb (b, sr, decay * 0.6f, 0.7f);
    Buf up = rubberBand (r1, sr, std::pow (2.0, interval / 12.0), 0.0, false, true, abort);
    Buf r2 = fdnReverb (up, sr, decay, 0.8f, 30.0f);
    const float m = mix / 100.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float wet = 0.35f * r1.getSample (ch, i) + 1.0f * r2.getSample (ch, i);
            b.setSample (ch, i, b.getSample (ch, i) * (1 - 0.5f * m) + wet * m * 1.3f);
        }
}

static void echo (Buf& b, double sr, float timeMs, float feedback, float mix)
{
    // tape-style: darker and slightly saturated on every repeat
    const int d = juce::jmax (1, (int) (timeMs * 0.001 * sr));
    Ring r[2] { Ring (d + 4), Ring (d + 4) };
    auto lp = Biquad::lp (sr, 3200), hp = Biquad::hp (sr, 180);
    const float fb = feedback / 100.0f, m = mix / 100.0f;
    for (int i = 0; i < b.getNumSamples(); ++i)
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = b.getSample (ch, i);
            float del = r[ch].read (d);
            float fbSig = std::tanh (hp.tick (lp.tick (del, ch), ch) * 1.2f) * fb;
            r[ch].push (x + fbSig);
            b.setSample (ch, i, x * (1 - 0.3f * m) + del * m);
        }
}

static void pingPong (Buf& b, double sr, float timeMs, float feedback, float mix)
{
    const int d = juce::jmax (1, (int) (timeMs * 0.001 * sr));
    Ring rl (d + 4), rr (d + 4);
    const float fb = feedback / 100.0f, m = mix / 100.0f;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        float xl = b.getSample (0, i), xr = b.getSample (1, i);
        float dl = rl.read (d), dr = rr.read (d);
        rl.push (0.5f * (xl + xr) + dr * fb);   // left feeds from right's output
        rr.push (dl * fb);
        b.setSample (0, i, xl * (1 - 0.3f * m) + dl * m);
        b.setSample (1, i, xr * (1 - 0.3f * m) + dr * m);
    }
}

static void stereoMove (Buf& b, double sr, float rate, float width)
{
    const float w = width / 100.0f;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        float pan = w * (float) std::sin (twoPi * rate * i / sr);
        float ang = (pan + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
        float mono = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
        float side = 0.5f * (b.getSample (0, i) - b.getSample (1, i));
        b.setSample (0, i, mono * std::cos (ang) * 1.414f + side);
        b.setSample (1, i, mono * std::sin (ang) * 1.414f - side);
    }
}

static void speed (Buf& b, double sr, int& preRoll, float speedPct)
{
    const double ratio = speedPct / 100.0;
    if (std::abs (ratio - 1.0) < 1e-4) return;
    const int n = b.getNumSamples();
    const int m = (int) std::ceil (n / ratio);
    if (ratio > 1.0)   // band-limit before speeding up to avoid aliasing
    {
        auto a = Biquad::lp (sr, sr * 0.45 / ratio), c = a;
        a.run (b); c.run (b);
    }
    Buf out (2, m);
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* x = b.getReadPointer (ch);
        for (int i = 0; i < m; ++i) out.setSample (ch, i, readCubic (x, n, i * ratio));
    }
    b = std::move (out);
    preRoll = (int) std::round (preRoll / ratio);
}

static void wowFlutter (Buf& b, double sr, float depth, float rate, bool isFlutter, juce::uint32 seed)
{
    const float d = depth / 100.0f;
    const double maxMs = isFlutter ? 0.35 * d : 6.0 * d;
    SmoothNoise drift ((juce::int64) seed, isFlutter ? rate * 0.7 : rate * 0.5, sr);
    const int n = b.getNumSamples();
    std::vector<float> del ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        double lfo = 0.75 * std::sin (twoPi * rate * i / sr) + 0.25 * drift.next();
        del[(size_t) i] = (float) ((maxMs * (1.0 + lfo) + 1.0) * 0.001 * sr);
    }
    modulatedRead (b, [&] (int i, int) { return del[(size_t) i]; });
}

static void tapeSat (Buf& b, double sr, float drive)
{
    const float k = 1.0f + drive * 0.05f;
    auto bump = Biquad::peak (sr, 85, 1.0, drive * 0.04);         // head bump
    auto hf = Biquad::lp (sr, 16000.0 - drive * 90.0, 0.6);      // HF loss rises with drive
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float x = b.getSample (ch, i) * k;
            float y = std::tanh (x + 0.08f) - std::tanh (0.08f);    // bias asymmetry
            b.setSample (ch, i, hf.tick (bump.tick (y / std::sqrt (k), ch), ch));
        }
}

static void resonance (Buf& b, double sr, int type, float amount)
{
    Buf wet (b);
    std::vector<Biquad> chain;
    float sat = 1.0f;
    switch (type)
    {
        case 0: // 1990s telephone handset (carbon/electret mic + tiny earpiece)
            chain = { Biquad::hp (sr, 300), Biquad::hp (sr, 300), Biquad::lp (sr, 3400), Biquad::lp (sr, 3400),
                      Biquad::peak (sr, 1500, 1.2, 6) };
            sat = 2.0f; break;
        case 1: // megaphone horn
            chain = { Biquad::hp (sr, 500, 1.0), Biquad::hp (sr, 500), Biquad::lp (sr, 4500, 1.2), Biquad::lp (sr, 4500),
                      Biquad::peak (sr, 1800, 2.0, 10), Biquad::peak (sr, 3000, 3.0, 5) };
            sat = 4.0f; break;
        case 2: // AM radio
            chain = { Biquad::hp (sr, 150), Biquad::lp (sr, 5000), Biquad::lp (sr, 5000),
                      Biquad::peak (sr, 1000, 0.7, 3), Biquad::peak (sr, 2800, 1.0, 3) };
            sat = 1.5f; break;
        case 3: // walkie-talkie
            chain = { Biquad::hp (sr, 450), Biquad::hp (sr, 450), Biquad::lp (sr, 2800), Biquad::lp (sr, 2800),
                      Biquad::peak (sr, 1200, 1.5, 8) };
            sat = 3.0f; break;
        default: // gramophone horn
            chain = { Biquad::hp (sr, 250), Biquad::lp (sr, 4500), Biquad::lp (sr, 4500),
                      Biquad::peak (sr, 900, 3, 6), Biquad::peak (sr, 1900, 4, 5), Biquad::peak (sr, 3100, 4, 4) };
            sat = 1.5f; break;
    }
    for (auto& f : chain) f.run (wet);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < wet.getNumSamples(); ++i)
            wet.setSample (ch, i, std::tanh (wet.getSample (ch, i) * sat) / std::sqrt (sat));
    float ri = rms (b), rw = rms (wet);
    float g = rw > 1e-6f ? juce::jlimit (0.25f, 8.0f, ri / rw) : 1.0f;
    mixInto (b, wet, 1.0f - amount / 100.0f, amount / 100.0f * g);
}

static void bandwidth (Buf& b, double sr, float lo, float hi)
{
    if (lo > 25)  { auto a = Biquad::hp (sr, lo), c = Biquad::hp (sr, lo); a.run (b); c.run (b); }
    if (hi < sr * 0.45) { auto a = Biquad::lp (sr, hi), c = Biquad::lp (sr, hi); a.run (b); c.run (b); }
}

static void compress (Buf& b, double sr, float amount)
{
    const float a = amount / 100.0f;
    const float thresh = -10.0f - 22.0f * a, ratio = 1.5f + 8.0f * a;
    const float att = std::exp (-1.0f / (float) (0.004 * sr)), rel = std::exp (-1.0f / (float) (0.12 * sr));
    float env = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        float x = juce::jmax (std::abs (b.getSample (0, i)), std::abs (b.getSample (1, i)));
        env = x > env ? att * env + (1 - att) * x : rel * env + (1 - rel) * x;
        float db = juce::Decibels::gainToDecibels (env, -120.0f);
        float over = db - thresh;
        float gr = over > 0 ? -over * (1.0f - 1.0f / ratio) : 0.0f;
        float g = dbToGain (gr);
        b.setSample (0, i, b.getSample (0, i) * g);
        b.setSample (1, i, b.getSample (1, i) * g);
    }
}

static void dropouts (Buf& b, double sr, float freq, float depth, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed);
    const int n = b.getNumSamples();
    std::vector<float> g ((size_t) n, 1.0f);
    const double eventsPerSec = freq / 100.0 * 1.6;
    const int ramp = (int) (0.006 * sr);
    double t = 0;
    while (true)
    {
        t += -std::log (juce::jmax (1.0e-6, (double) r.nextFloat())) / juce::jmax (1.0e-3, eventsPerSec);
        int start = (int) (t * sr);
        if (start >= n) break;
        int len = (int) ((0.03 + 0.25 * r.nextFloat() * r.nextFloat()) * sr);
        float floorG = 1.0f - depth / 100.0f * (0.6f + 0.4f * r.nextFloat());
        for (int i = 0; i < len + 2 * ramp && start + i < n; ++i)
        {
            float env = i < ramp ? (float) i / ramp : (i > len + ramp ? (float) (len + 2 * ramp - i) / ramp : 1.0f);
            float v = 1.0f - (1.0f - floorG) * env;
            g[(size_t) (start + i)] = juce::jmin (g[(size_t) (start + i)], v);
        }
    }
    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::multiply (b.getWritePointer (ch), g.data(), n);
}

static void hiss (Buf& b, double sr, float levelDb, int contentEnd, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed);
    auto shape = Biquad::hp (sr, 1500, 0.5);
    auto top = Biquad::lp (sr, 14000);
    const float g = dbToGain (levelDb) * 2.5f;
    const int end = juce::jmin (b.getNumSamples(), contentEnd);
    const int fade = (int) (0.3 * sr);
    for (int i = 0; i < end; ++i)
    {
        float fadeG = i > end - fade ? (float) (end - i) / fade : 1.0f;
        for (int ch = 0; ch < 2; ++ch)
            b.addSample (ch, i, top.tick (shape.tick (r.nextFloat() * 2 - 1, ch), ch) * g * fadeG);
    }
}

static void crackle (Buf& b, double sr, float density, float levelDb, int contentEnd, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed);
    const float g = dbToGain (levelDb);
    const double perSample = (density / 100.0f) * 70.0 / sr;
    auto clickHp = Biquad::hp (sr, 900);
    auto rumbleLp = Biquad::lp (sr, 250);
    const int end = juce::jmin (b.getNumSamples(), contentEnd);
    float click = 0;
    for (int i = 0; i < end; ++i)
    {
        if (r.nextDouble() < perSample)
        {
            float u = r.nextFloat();
            click = (r.nextBool() ? 1.0f : -1.0f) * (0.2f + 0.8f * u * u * u);
        }
        float c = clickHp.tick (click, 0);
        click *= 0.55f;   // very fast decay
        float surface = rumbleLp.tick (r.nextFloat() * 2 - 1, 0) * 0.08f;
        b.addSample (0, i, (c + surface) * g);
        b.addSample (1, i, (c * 0.8f + surface) * g);
    }
}
} // namespace fx

//==============================================================================
void processLayer (const Layer& L, Buf& b, double sr, int& preRoll, const std::atomic<bool>* abort)
{
    auto on = [&L] (int e) { return L.isActive (e); };
    auto v  = [&L] (int e, int i) { return L.v (e, i); };
    auto seed = [&L] (int e) { return L.seed * 2654435761u + (juce::uint32) e * 97u; };
#define ML_CHECK if (aborted (abort)) return;

    // ---------------- VOICE: one high-quality Rubber Band pass for pitch + formant + size
    {
        double pitchSt = 0, formantSt = 0;
        float size = on (E_Size) ? v (E_Size, 0) / 100.0f : 0.0f;
        if (on (E_Pitch))   pitchSt += v (E_Pitch, 0);
        if (on (E_Formant)) formantSt += v (E_Formant, 0);
        pitchSt   += -size * 4.0;
        formantSt += -size * 7.0;
        if (std::abs (pitchSt) > 0.01 || std::abs (formantSt) > 0.01)
        {
            double P = std::pow (2.0, pitchSt / 12.0), F = std::pow (2.0, formantSt / 12.0);
            b = rubberBand (b, sr, P, F / P, true, false, abort);
        }
        ML_CHECK
        if (on (E_Size))   fx::sizeResonance (b, sr, size);
        if (on (E_Bass))   fx::bass (b, sr, v (E_Bass, 0), v (E_Bass, 1));
        if (on (E_Voices)) fx::voices (b, sr, (int) v (E_Voices, 0), v (E_Voices, 1), v (E_Voices, 2) / 100.0f, seed (E_Voices), abort);
        ML_CHECK
    }
    // ---------------- TEXTURE
    if (on (E_Grit))        fx::grit (b, sr, v (E_Grit, 0), seed (E_Grit));
    if (on (E_Growl))       fx::growl (b, sr, v (E_Growl, 0), v (E_Growl, 1), seed (E_Growl));
    ML_CHECK
    if (on (E_Breath))      fx::breath (b, sr, v (E_Breath, 0), seed (E_Breath));
    ML_CHECK
    if (on (E_Distortion))  fx::distortion (b, sr, v (E_Distortion, 0), v (E_Distortion, 1));
    if (on (E_Instability)) fx::instability (b, sr, v (E_Instability, 0), v (E_Instability, 1), seed (E_Instability));
    ML_CHECK
    if (on (E_Subharm))     fx::subharmonics (b, sr, v (E_Subharm, 0), (int) v (E_Subharm, 1), abort);
    ML_CHECK
    // ---------------- SCI-FI
    if (on (E_Robot))     fx::robot (b, sr, v (E_Robot, 0), v (E_Robot, 1));
    ML_CHECK
    if (on (E_RingMod))   fx::ringMod (b, sr, v (E_RingMod, 0), v (E_RingMod, 1));
    if (on (E_FreqShift)) fx::freqShift (b, sr, v (E_FreqShift, 0), v (E_FreqShift, 1));
    if (on (E_Bitcrush))  fx::bitcrush (b, sr, v (E_Bitcrush, 0), v (E_Bitcrush, 1), v (E_Bitcrush, 2));
    if (on (E_Flanger))   fx::flanger (b, sr, v (E_Flanger, 0), v (E_Flanger, 1), v (E_Flanger, 2));
    if (on (E_Chorus))    fx::chorus (b, sr, v (E_Chorus, 0), v (E_Chorus, 1), v (E_Chorus, 2));
    ML_CHECK
    // ---------------- SPACE
    if (on (E_Freeze))    fx::spectralFreeze (b, sr, v (E_Freeze, 0), v (E_Freeze, 1), seed (E_Freeze));
    ML_CHECK
    if (on (E_Shimmer))   fx::shimmer (b, sr, v (E_Shimmer, 0), v (E_Shimmer, 1), v (E_Shimmer, 2), abort);
    ML_CHECK
    if (on (E_RevReverb)) fx::reverseReverb (b, sr, v (E_RevReverb, 0), v (E_RevReverb, 1));
    if (on (E_Reverb))    fx::reverb (b, sr, v (E_Reverb, 0), v (E_Reverb, 1), v (E_Reverb, 2));
    if (on (E_Echo))      fx::echo (b, sr, v (E_Echo, 0), v (E_Echo, 1), v (E_Echo, 2));
    if (on (E_Delay))     fx::pingPong (b, sr, v (E_Delay, 0), v (E_Delay, 1), v (E_Delay, 2));
    if (on (E_Stereo))    fx::stereoMove (b, sr, v (E_Stereo, 0), v (E_Stereo, 1));
    ML_CHECK
    // ---------------- MEDIA
    if (on (E_Speed))     fx::speed (b, sr, preRoll, v (E_Speed, 0));
    if (on (E_Wow))       fx::wowFlutter (b, sr, v (E_Wow, 0), v (E_Wow, 1), false, seed (E_Wow));
    if (on (E_Flutter))   fx::wowFlutter (b, sr, v (E_Flutter, 0), v (E_Flutter, 1), true, seed (E_Flutter));
    if (on (E_TapeSat))   fx::tapeSat (b, sr, v (E_TapeSat, 0));
    if (on (E_Resonance)) fx::resonance (b, sr, (int) std::round (v (E_Resonance, 0)), v (E_Resonance, 1));
    if (on (E_Bandwidth)) fx::bandwidth (b, sr, v (E_Bandwidth, 0), v (E_Bandwidth, 1));
    if (on (E_Compress))  fx::compress (b, sr, v (E_Compress, 0));
    if (on (E_Dropouts))  fx::dropouts (b, sr, v (E_Dropouts, 0), v (E_Dropouts, 1), seed (E_Dropouts));
    if (on (E_Hiss) || on (E_Crackle))
    {
        const int contentEnd = lastAbove (b, 0.0003f) + (int) (0.4 * sr);
        if (on (E_Hiss))    fx::hiss (b, sr, v (E_Hiss, 0), contentEnd, seed (E_Hiss));
        if (on (E_Crackle)) fx::crackle (b, sr, v (E_Crackle, 0), v (E_Crackle, 1), contentEnd, seed (E_Crackle));
    }
#undef ML_CHECK
}

//==============================================================================
// Lead-in / tail a layer needs so nothing it creates is cut off.
static void layerNeeds (const Layer& L, double inputSeconds, double& pre, double& tail)
{
    pre = 0; tail = 0.2;
    if (! L.enabled) return;
    if (L.isActive (E_RevReverb)) pre = L.v (E_RevReverb, 0) * 1.1 + 0.15;
    if (L.isActive (E_Reverb))    tail += L.v (E_Reverb, 0) * 1.1;
    if (L.isActive (E_Shimmer))   tail += L.v (E_Shimmer, 1) * 1.5;
    if (L.isActive (E_Freeze))    tail += 1.0 + L.v (E_Freeze, 0) * 0.05;
    if (L.isActive (E_Echo))      tail += L.v (E_Echo, 0) * 0.001 * (2 + 20 * L.v (E_Echo, 1) / 100.0);
    if (L.isActive (E_Delay))     tail += L.v (E_Delay, 0) * 0.001 * (2 + 20 * L.v (E_Delay, 1) / 100.0);
    if (L.isActive (E_Speed))     tail += inputSeconds * juce::jmax (0.0, 100.0 / L.v (E_Speed, 0) - 1.0);
    tail = juce::jmin (tail, 30.0);
}

// Run one layer (padding, processing, loudness match, wet/dry) on b in place.
static bool applyLayer (const Layer& L, Buf& b, double sr, int& preRoll, const std::atomic<bool>* abort)
{
    if (! L.enabled) return true;

    // trim silence left by previous layers, then add what this layer needs
    int last = lastAbove (b, 0.00005f);
    int keep = juce::jlimit (juce::jmin (b.getNumSamples(), preRoll + 1), b.getNumSamples(), last + (int) (0.1 * sr));
    b.setSize (2, keep, true, true);

    double pre, tail;
    layerNeeds (L, b.getNumSamples() / sr, pre, tail);
    int addPre = juce::jmax (0, (int) (pre * sr) - preRoll);   // swell room already available counts
    int addTail = (int) (tail * sr);
    if (addPre > 0 || addTail > 0)
    {
        Buf padded (2, addPre + b.getNumSamples() + addTail);
        padded.clear();
        for (int ch = 0; ch < 2; ++ch) padded.copyFrom (ch, addPre, b, ch, 0, b.getNumSamples());
        b = std::move (padded);
        preRoll += addPre;
    }

    Buf dry (b);
    const float dryRms = rms (dry);
    processLayer (L, b, sr, preRoll, abort);
    if (aborted (abort)) return false;

    // keep each layer at the loudness of the signal entering it, so stacking stays controllable
    const float wetRms = rms (b);
    if (wetRms > 1e-7f && dryRms > 1e-7f)
        b.applyGain (juce::jlimit (0.25f, 4.0f, dryRms / wetRms));

    // per-layer wet/dry against the signal entering this layer
    const float m = juce::jlimit (0.0f, 1.0f, L.mix / 100.0f);
    if (m < 0.999f)
    {
        b.applyGain (m);
        for (int ch = 0; ch < 2; ++ch)
            b.addFrom (ch, 0, dry, ch, 0, juce::jmin (b.getNumSamples(), dry.getNumSamples()), 1.0f - m);
    }
    return true;
}

static void finalise (Buf b, int preRoll, double sr, int srcLen, RenderOutput& out)
{
    int last = lastAbove (b, 0.00005f);
    int keep = juce::jlimit (juce::jmin (b.getNumSamples(), preRoll + srcLen), b.getNumSamples(), last + (int) (0.25 * sr));
    b.setSize (2, keep, true, true);

    out.limited = false;
    float peak = juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples()));
    if (peak > 0.97f) { b.applyGain (0.97f / peak); out.limited = true; }

    out.audio = std::move (b);
    out.preRoll = preRoll;
    out.sampleRate = sr;
}

static Buf toStereo (const Buf& src)
{
    Buf b (2, src.getNumSamples());
    for (int ch = 0; ch < 2; ++ch)
        b.copyFrom (ch, 0, src, juce::jmin (ch, src.getNumChannels() - 1), 0, src.getNumSamples());
    return b;
}

bool renderStack (const Buf& src, double sr, const Stack& stack, RenderOutput& out,
                  const std::atomic<bool>* abort, std::function<void (float)> progress)
{
    StackRenderer r;
    return r.render (src, sr, stack, out, abort, progress);
}

bool StackRenderer::render (const Buf& src, double sr, const Stack& stack, RenderOutput& out,
                            const std::atomic<bool>* abort, std::function<void (float)> progress)
{
    // Reuse the cached result of every unchanged layer at the bottom of the stack.
    juce::String key;
    std::vector<juce::String> keys;
    for (auto& L : stack)
    {
        key += juce::JSON::toString (L.toVar(), true) + "|";
        keys.push_back (juce::String (key.hashCode64()) + ":" + juce::String (key.length()));
    }

    size_t reuse = 0;
    while (reuse < stack.size() && reuse < cache.size() && cache[reuse].key == keys[reuse])
        ++reuse;
    cache.resize (reuse);

    Buf b; int preRoll = 0;
    if (reuse > 0) { b = cache[reuse - 1].audio; preRoll = cache[reuse - 1].preRoll; }
    else            b = toStereo (src);

    for (size_t i = reuse; i < stack.size(); ++i)
    {
        if (aborted (abort)) return false;
        if (progress) progress ((float) i / (float) juce::jmax ((size_t) 1, stack.size()));
        if (! applyLayer (stack[i], b, sr, preRoll, abort)) return false;
        cache.push_back ({ keys[i], b, preRoll });
    }

    finalise (std::move (b), preRoll, sr, src.getNumSamples(), out);
    if (progress) progress (1.0f);
    return true;
}
} // namespace ml
