#pragma once
#include <JuceHeader.h>
#include "GlitchEngine.h"
#include "PresetBank.h"

class GlitchNoiseAudioProcessor  : public juce::AudioProcessor
{
public:
    GlitchNoiseAudioProcessor();
    ~GlitchNoiseAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // --- JUCE 8 (headless AudioProcessor) required overrides ---
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }

    // UI commands
    void triggerPanic();
    void randomizeParams();
    void setLock (const juce::String& pid, bool locked);
    void loadPreset (int index);

    // UI status
    struct UiStatus { int opState = 0; int clockIndex = 0; int microMs = 0; };
    UiStatus getUiStatus() const;

    int getPresetCount() const { return (int) presetBank.size(); }
    juce::String getPresetName (int i) const;

private:
    GlitchEngine engine;

    // output smoothing
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smOut;

    // lock mask for randomize (macros only)
    enum LockBit
    {
        L_CLOCK    = 1 << 0,
        L_WORDSIZE = 1 << 1,
        L_OPMORPH  = 1 << 2,
        L_MASK     = 1 << 3,
        L_JITTER   = 1 << 4,
        L_STUTTER  = 1 << 5,
        L_FEEDBACK = 1 << 6,
        L_DENSITY  = 1 << 7,
    };
    std::atomic<uint32_t> lockMask { 0 };

    std::vector<Preset> presetBank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchNoiseAudioProcessor)
};
