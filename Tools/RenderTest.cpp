// Headless test: renders every built-in preset (and some stacks) over a WAV file,
// checks the output for NaNs/silence/clipping and writes the results as 24-bit WAVs.
//   MonsterLabRenderTest input.wav outputFolder
#include <JuceHeader.h>
#include "Engine/Renderer.h"

using namespace ml;

static bool loadWav (const juce::File& f, juce::AudioBuffer<float>& buf, double& sr)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (! r) return false;
    buf.setSize ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&buf, 0, (int) r->lengthInSamples, 0, true, true);
    sr = r->sampleRate;
    return true;
}

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& buf, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (! os) return false;
    auto opts = juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (buf.getNumChannels()).withBitsPerSample (24);
    auto w = wav.createWriterFor (os, opts);
    if (! w) return false;
    return w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

static int failures = 0;

static void check (const juce::String& label, const Stack& stack, const juce::AudioBuffer<float>& src, double sr, const juce::File& outDir)
{
    RenderOutput out;
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    bool ok = renderStack (src, sr, stack, out);
    auto ms = juce::Time::getMillisecondCounterHiRes() - t0;

    bool finite = true;
    double sumSq = 0;
    for (int ch = 0; ch < out.audio.getNumChannels(); ++ch)
        for (int i = 0; i < out.audio.getNumSamples(); ++i)
        {
            float x = out.audio.getSample (ch, i);
            if (! std::isfinite (x)) finite = false;
            sumSq += (double) x * x;
        }
    double rmsDb = juce::Decibels::gainToDecibels (std::sqrt (sumSq / juce::jmax (1, out.audio.getNumSamples() * 2)), -120.0);
    float peak = out.audio.getMagnitude (0, out.audio.getNumSamples());
    bool pass = ok && finite && rmsDb > -50 && peak <= 0.98f;
    if (! pass) ++failures;

    std::printf ("%-5s %-34s len %6.2fs  pre %4.2fs  rms %6.1f dB  peak %5.3f  %6.0f ms%s\n",
                 pass ? "OK" : "FAIL", label.toRawUTF8(), out.audio.getNumSamples() / sr, out.preRoll / sr,
                 rmsDb, peak, ms, out.limited ? "  (limited)" : "");
    writeWav (outDir.getChildFile (juce::File::createLegalFileName (label) + ".wav"), out.audio, sr);
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: %s input.wav outdir\n", argv[0]); return 2; }
    juce::AudioBuffer<float> src; double sr = 0;
    if (! loadWav (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]), src, sr)) { std::printf ("cannot read input\n"); return 2; }
    auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
    outDir.createDirectory();
    std::printf ("input: %d ch, %.2fs @ %.0f Hz\n", src.getNumChannels(), src.getNumSamples() / sr, sr);

    // every built-in preset on its own
    for (auto& p : builtInPresets())
        check (p.name, { makeBuiltInPreset (p.name) }, src, sr, outDir);

    // every effect alone at defaults
    for (int e = 0; e < NumEffects; ++e)
    {
        Layer l; l.fx[(size_t) e].on = true;
        check (juce::String ("fx_") + effectTable()[(size_t) e].id, { l }, src, sr, outDir);
    }

    // the target workflow: Demon + VHS stacked; Demon + Ghost + VHS
    check ("STACK Demon + VHS", { makeBuiltInPreset ("Demon"), makeBuiltInPreset ("VHS - Worn Tape") }, src, sr, outDir);
    check ("STACK Demon + Ghost + VHS", { makeBuiltInPreset ("Demon"), makeBuiltInPreset ("Ghost"), makeBuiltInPreset ("VHS - Worn Tape") }, src, sr, outDir);

    // bypass must reproduce the original (apart from mono->stereo)
    {
        auto l = makeBuiltInPreset ("Demon"); l.enabled = false;
        RenderOutput out; renderStack (src, sr, { l }, out);
        float diff = 0;
        for (int i = 0; i < src.getNumSamples(); ++i) diff = juce::jmax (diff, std::abs (out.audio.getSample (0, i) - src.getSample (0, i)));
        std::printf ("%-5s bypassed layer == original (max diff %g)\n", diff < 1e-6f ? "OK" : "FAIL", diff);
        if (diff >= 1e-6f) ++failures;
    }
    // save/load round trip
    {
        Stack s { makeBuiltInPreset ("Demon"), makeBuiltInPreset ("Old Telephone (1990s)") };
        s[0].fx[E_Pitch].p[0] = -9.5f; s[1].mix = 42; s[1].sectionOn[Media] = false;
        auto json = juce::JSON::toString (stackToVar (s));
        auto back = stackFromVar (juce::JSON::parse (json));
        bool same = back.size() == 2 && back[0].fx[E_Pitch].p[0] == -9.5f && back[1].mix == 42 && ! back[1].sectionOn[Media]
                    && back[1].name == s[1].name && back[0].fx[E_Reverb].on == s[0].fx[E_Reverb].on;
        std::printf ("%-5s character save/load round trip\n", same ? "OK" : "FAIL");
        if (! same) ++failures;
    }
    std::printf ("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
