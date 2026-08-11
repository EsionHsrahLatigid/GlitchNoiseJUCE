#include "PluginProcessor.h"

#include <cmath>
#include <iostream>

namespace
{
bool check(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "[FAIL] " << message << '\n';
    return condition;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    GlitchNoiseAudioProcessor processor;
    bool passed = true;

    passed &= check(processor.getName() == "GlitchNoiseJUCE", "product name should match EHL identity");
    passed &= check(!processor.acceptsMidi(), "processor should not accept MIDI");
    passed &= check(!processor.isMidiEffect(), "processor should be an audio effect");
    passed &= check(processor.getPresetCount() > 0, "preset bank should be available");
    passed &= check(GlitchEngine::rotateLeftForTest(0x12345678u, 0) == 0x12345678u,
                    "zero-bit rotate should return input without UB");
    passed &= check(GlitchEngine::rotateLeftForTest(0x12345678u, 32) == 0x12345678u,
                    "32-bit rotate should normalize to zero-bit rotate");
    passed &= check(GlitchEngine::rotateLeftForTest(0x80000001u, 1) == 0x00000003u,
                    "one-bit rotate should preserve wrapped bit behavior");

    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add(juce::AudioChannelSet::stereo());
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    passed &= check(processor.isBusesLayoutSupported(stereo), "stereo layout should be supported");

    auto* outputDb = processor.apvts.getParameter("outputDb");
    passed &= check(outputDb != nullptr, "output parameter should exist");
    if (outputDb != nullptr)
    {
        outputDb->setValueNotifyingHost(outputDb->convertTo0to1(-12.0f));
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        outputDb->setValueNotifyingHost(outputDb->convertTo0to1(-2.0f));
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        passed &= check(std::abs(processor.apvts.getRawParameterValue("outputDb")->load() + 12.0f) < 0.001f,
                        "APVTS state should round-trip");
    }

    constexpr double sampleRate = 44100.0;
    processor.prepareToPlay(sampleRate, 512);
    int generatedSamples = 0;
    const int blockSizes[] { 32, 128, 512, 1024, 256, 64 };

    for (const auto blockSize : blockSizes)
    {
        juce::AudioBuffer<float> audio(2, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = static_cast<float>(0.2 * std::sin(2.0 * juce::MathConstants<double>::pi
                                                                 * 110.0 * generatedSamples / sampleRate));
            audio.setSample(0, sample, value);
            audio.setSample(1, sample, value);
            ++generatedSamples;
        }

        juce::MidiBuffer midi;
        processor.processBlock(audio, midi);
        passed &= check(midi.isEmpty(), "processor should clear MIDI");

        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                passed &= check(std::isfinite(audio.getSample(channel, sample)), "processed audio should remain finite");
    }

    processor.triggerPanic();
    juce::AudioBuffer<float> panicAudio(2, 64);
    panicAudio.setSample(0, 0, 1.0f);
    panicAudio.setSample(1, 0, 1.0f);
    juce::MidiBuffer midi;
    processor.processBlock(panicAudio, midi);
    passed &= check(panicAudio.getRMSLevel(0, 0, panicAudio.getNumSamples()) == 0.0f,
                    "panic should clear the next block");

    if (passed)
        std::cout << "GlitchNoiseJUCE integration checks passed\n";
    return passed ? 0 : 1;
}
