#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class GlitchNoiseAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit GlitchNoiseAudioProcessorEditor (GlitchNoiseAudioProcessor&);
    ~GlitchNoiseAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GlitchNoiseAudioProcessor& processor;

    class Os9WebUI : public juce::WebBrowserComponent
    {
    public:
        explicit Os9WebUI (GlitchNoiseAudioProcessor& p);
        bool pageAboutToLoad (const juce::String& newURL) override;
        void loadUi();

    private:
        GlitchNoiseAudioProcessor& proc;

        static juce::String makeHtml (GlitchNoiseAudioProcessor& proc);
        static juce::URL makeDataUrl (const juce::String& html);

        static juce::String getAction (const juce::String& juceUrl);
        static juce::String getQuery (const juce::String& juceUrl);
        static juce::String getParam (const juce::String& query, const juce::String& key);
    };

    Os9WebUI web;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchNoiseAudioProcessorEditor)
};
