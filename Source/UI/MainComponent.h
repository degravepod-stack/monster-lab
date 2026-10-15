#pragma once
#include <JuceHeader.h>
#include "Engine/Renderer.h"

namespace ml
{
class PresetPanel;
class StackPanel;
class EffectGrid;
class AudioPanel;
class MonsterLookAndFeel;

//==============================================================================
// Plays either the original or the processed buffer on one shared timeline.
class PlaybackSource : public juce::AudioSource
{
public:
    void prepareToPlay (int, double sampleRate) override { deviceRate = sampleRate; }
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

    void setBuffers (std::shared_ptr<const juce::AudioBuffer<float>> original,
                     std::shared_ptr<const juce::AudioBuffer<float>> processed,
                     int processedPreRoll, double fileRate);
    void setShowProcessed (bool p) { showProcessed = p; }
    bool isShowingProcessed() const { return showProcessed; }
    void play()  { playing = true; }
    void pause() { playing = false; }
    void stop();
    bool isPlaying() const { return playing; }
    // timeline position in seconds; 0 = start of original. Negative = reverse-reverb lead-in.
    double getPosition() const { return pos.load() / juce::jmax (1.0, fileRate); }
    void setPosition (double seconds) { pos = seconds * fileRate; }
    double startTime() const;
    double endTime() const;

private:
    juce::SpinLock lock;
    std::shared_ptr<const juce::AudioBuffer<float>> orig, proc;
    int preRoll = 0;
    double fileRate = 44100, deviceRate = 44100;
    std::atomic<double> pos { 0 };
    std::atomic<bool> playing { false }, showProcessed { true };
};

//==============================================================================
class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer,
                      private juce::Thread
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    // ---- model operations (all on the message thread)
    Stack stack;
    int selected = -1;
    Layer* selectedLayer() { return juce::isPositiveAndBelow (selected, (int) stack.size()) ? &stack[(size_t) selected] : nullptr; }

    void addLayer (Layer l);
    void structureChanged();      // layers added / removed / moved
    void selectLayer (int index);
    void parametersChanged();     // any value or switch changed
    void saveLayerPreset (int index);

    void openAudioDialog();
    bool loadAudio (const juce::File&);
    void exportDialog();
    void saveCharacterDialog();
    void loadCharacterDialog();
    void showAudioSettings();
    bool writeProcessedWav (const juce::File&);
    void togglePlay();
    void stopPlay();

    PlaybackSource playback;
    std::shared_ptr<const juce::AudioBuffer<float>> original, processed;
    int processedPreRoll = 0;
    double sampleRate = 44100;
    juce::File sourceFile;
    juce::String statusText { "Drop a voice recording (WAV) on the right, then click presets on the left." };
    bool renderUpToDate = true, renderLimited = false;
    std::atomic<float> renderProgress { 0 };

private:
    void timerCallback() override;
    void run() override;               // render thread
    void requestRender();
    void selfTestStep();
    int selfTestStage = -1;
    juce::File selfTestOut;

    std::unique_ptr<MonsterLookAndFeel> lnf;
    std::unique_ptr<PresetPanel> presets;
    std::unique_ptr<StackPanel> stackPanel;
    std::unique_ptr<EffectGrid> grid;
    std::unique_ptr<AudioPanel> audioPanel;

    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer player;
    juce::AudioFormatManager formats;
    std::unique_ptr<juce::FileChooser> chooser;

    // render thread state
    juce::CriticalSection renderLock;
    juce::WaitableEvent renderWake;
    std::atomic<bool> abortRender { false };
    std::atomic<int> requestedGeneration { 0 };
    int sourceGeneration = 0;
    Stack pendingStack;
    std::shared_ptr<const juce::AudioBuffer<float>> pendingSource;
    double pendingRate = 44100;
    int pendingGeneration = 0, pendingSourceGen = 0;
    juce::int64 lastChangeMs = 0;
    bool renderScheduled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace ml
