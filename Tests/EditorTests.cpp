#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <ehl/juce_design/EhlDesign.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>

namespace
{
bool check(bool condition, const char* message)
{
    if (! condition)
        std::cerr << "[FAIL] " << message << '\n';
    return condition;
}

bool checkNeutralChrome(juce::AudioProcessorEditor& editor)
{
    juce::Image image(juce::Image::RGB, editor.getWidth(), editor.getHeight(), true);
    {
        juce::Graphics g(image);
        editor.paint(g);
    }

    bool passed = true;
    passed &= check(image.getPixelAt(0, 0) == ehl::juce_design::Palette::paper(), "top strip uses paper");
    passed &= check(image.getPixelAt(0, 4) == ehl::juce_design::Palette::ink(), "background uses ink");

    bool neutral = true;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto pixel = image.getPixelAt(x, y);
            const auto minChannel = std::min({ pixel.getRed(), pixel.getGreen(), pixel.getBlue() });
            const auto maxChannel = std::max({ pixel.getRed(), pixel.getGreen(), pixel.getBlue() });
            neutral = neutral && static_cast<int>(maxChannel) - static_cast<int>(minChannel) <= 4;
        }

    passed &= check(neutral, "chrome paint remains neutral monochrome");
    return passed;
}

bool checkSlider(juce::AudioProcessorEditor& editor, const juce::String& id, std::size_t index)
{
    auto* slider = dynamic_cast<juce::Slider*>(editor.findChildWithID("glitchnoisejuce-control-" + id));
    auto* label = dynamic_cast<juce::Label*>(editor.findChildWithID("glitchnoisejuce-label-" + id));
    bool passed = true;
    passed &= check(slider != nullptr, "slider exists");
    passed &= check(label != nullptr, "label exists");
    if (slider == nullptr || label == nullptr)
        return false;

    const auto expected = ehl::juce_design::labelledControlBounds(ehl::juce_design::controlCell(editor.getLocalBounds(), index));
    passed &= check(slider->getBounds() == expected.control, "slider uses shared control grid");
    passed &= check(label->getBounds() == expected.label, "label uses shared label grid");
    passed &= check(slider->findColour(juce::Slider::thumbColourId) == ehl::juce_design::Palette::paper(), "slider thumb uses paper");
    passed &= check(slider->findColour(juce::Slider::trackColourId) == ehl::juce_design::Palette::mid(), "slider track uses mid");
    return passed;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    GlitchNoiseAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());

    bool passed = true;
    passed &= check(editor->getWidth() == ehl::juce_design::Metrics::defaultWidth, "default width");
    passed &= check(editor->getHeight() == ehl::juce_design::Metrics::defaultHeight, "default height");
    passed &= check(editor->getComponentID() == "glitchnoisejuce-editor", "editor component id");
    passed &= check(editor->findChildWithID("glitchnoisejuce-parameter-display") != nullptr, "parameter display exists");
    passed &= check(editor->findChildWithID("glitchnoisejuce-preset") != nullptr, "preset control exists");
    passed &= check(editor->findChildWithID("glitchnoisejuce-randomize") != nullptr, "randomize button exists");
    passed &= check(editor->findChildWithID("glitchnoisejuce-panic") != nullptr, "panic button exists");
    passed &= check(editor->findChildWithID("glitchnoisejuce-limiterOn") != nullptr, "limiter toggle exists");

    const char* ids[] { "clock", "wordSize", "opMorph", "mask", "jitter", "stutter", "feedback", "density", "outputDb" };
    for (std::size_t i = 0; i < std::size(ids); ++i)
        passed &= checkSlider(*editor, ids[i], i);

    for (const char* id : { "clock", "wordSize", "opMorph", "mask", "jitter", "stutter", "feedback", "density" })
        passed &= check(editor->findChildWithID(juce::String("glitchnoisejuce-lock-") + id) != nullptr, "lock control exists");

    passed &= checkNeutralChrome(*editor);
    editor->setBounds(0, 0, ehl::juce_design::Metrics::minimumWidth, ehl::juce_design::Metrics::minimumHeight);
    passed &= checkNeutralChrome(*editor);

    return passed ? 0 : 1;
}
