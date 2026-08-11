#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String pid (const char* s) { return juce::String (s); }

GlitchNoiseAudioProcessor::GlitchNoiseAudioProcessor()
: AudioProcessor (BusesProperties()
                  .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    smOut.reset (44100.0, 0.02);
    smOut.setCurrentAndTargetValue (1.0f);
    presetBank = makePresetBank();
}

juce::AudioProcessorValueTreeState::ParameterLayout GlitchNoiseAudioProcessor::createParameterLayout()
{
    using AP = juce::AudioParameterFloat;
    using BP = juce::AudioParameterBool;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // Macros (normalized 0..1)
    p.push_back (std::make_unique<AP> (pid("clock"),    "Clock",    juce::NormalisableRange<float>(0.f, 1.f, 0.0001f), 0.35f));
    p.push_back (std::make_unique<AP> (pid("wordSize"), "WordSize", juce::NormalisableRange<float>(0.f, 1.f, 0.0001f), 0.55f));
    p.push_back (std::make_unique<AP> (pid("opMorph"),  "OpMorph",  juce::NormalisableRange<float>(0.f, 1.f, 0.0001f), 0.10f));
    p.push_back (std::make_unique<AP> (pid("mask"),     "Mask",     juce::NormalisableRange<float>(0.f, 1.f, 0.0001f), 0.50f));
    p.push_back (std::make_unique<AP> (pid("jitter"),   "Jitter",   juce::NormalisableRange<float>(0.f, 1.f, 0.0001f), 0.15f));
    p.push_back (std::make_unique<AP> (pid("stutter"),  "Stutter",  juce::NormalisableRange<float>(0.f, 1.f, 0.0001f), 0.20f));
    p.push_back (std::make_unique<AP> (pid("feedback"), "Feedback", juce::NormalisableRange<float>(0.f, 1.f, 0.0001f), 0.30f));
    p.push_back (std::make_unique<AP> (pid("density"),  "Density",  juce::NormalisableRange<float>(0.f, 1.f, 0.0001f), 0.25f));

    p.push_back (std::make_unique<BP> (pid("limiterOn"), "Limiter", true));

    // Output trim in dB
    p.push_back (std::make_unique<AP> (pid("outputDb"), "Output", juce::NormalisableRange<float>(-24.f, 12.f, 0.01f), -6.0f));

    // Seed as float param (0..2^31-1)
    p.push_back (std::make_unique<AP> (pid("seed"), "Seed", juce::NormalisableRange<float>(0.f, 2147483647.f, 1.f), 1234567.f));

    return { p.begin(), p.end() };
}

void GlitchNoiseAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    smOut.reset (sampleRate, 0.02);
    smOut.setCurrentAndTargetValue (1.0f);
}

void GlitchNoiseAudioProcessor::releaseResources() {}

bool GlitchNoiseAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet().isDisabled())
        return true;

    return layouts.getMainInputChannelSet() == out;
}

void GlitchNoiseAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto getF = [&](const char* id) -> float { return apvts.getRawParameterValue (id)->load(); };
    auto getB = [&](const char* id) -> bool  { return apvts.getRawParameterValue (id)->load() > 0.5f; };

    const float clock    = getF ("clock");
    const float wordSize = getF ("wordSize");
    const float opMorph  = getF ("opMorph");
    const float mask     = getF ("mask");
    const float jitter   = getF ("jitter");
    const float stutter  = getF ("stutter");
    const float feedback = getF ("feedback");
    const float density  = getF ("density");

    const bool limiterOn = getB ("limiterOn");

    const float outDb = getF ("outputDb");
    const float outLinTarget = juce::Decibels::decibelsToGain (outDb);
    smOut.setTargetValue (outLinTarget);

    const float seedF = getF ("seed");
    const uint32_t seed = (uint32_t) juce::jlimit (0.0f, 2147483647.0f, seedF);
    engine.setSeed (seed);

    const float outLin = smOut.getNextValue();
    engine.setParams (clock, wordSize, opMorph, mask, jitter, stutter, feedback, density, limiterOn, outLin);
    engine.process (buffer);
}

void GlitchNoiseAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GlitchNoiseAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* GlitchNoiseAudioProcessor::createEditor()
{
    return new GlitchNoiseAudioProcessorEditor (*this);
}

// ---- UI commands ----
void GlitchNoiseAudioProcessor::triggerPanic()
{
    engine.triggerPanic();
}

static void setParamNorm (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float norm)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        p->endChangeGesture();
    }
}

static void setParamBool (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool v)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (v ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

static void setParamValue (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    // for non-normalized params, set via converting to normalized
    if (auto* p = apvts.getParameter (id))
    {
        auto range = p->getNormalisableRange();
        const float norm = range.convertTo0to1 (value);
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        p->endChangeGesture();
    }
}

void GlitchNoiseAudioProcessor::randomizeParams()
{
    juce::Random r;

    const uint32_t m = lockMask.load (std::memory_order_acquire);

    auto maybe = [&](uint32_t bit, const char* id, float norm)
    {
        if ((m & bit) == 0)
            setParamNorm (apvts, id, norm);
    };

    maybe (L_CLOCK,    "clock",    r.nextFloat());
    maybe (L_WORDSIZE, "wordSize", 0.15f + 0.85f * r.nextFloat());
    maybe (L_OPMORPH,  "opMorph",  r.nextFloat());
    maybe (L_MASK,     "mask",     r.nextFloat());
    maybe (L_JITTER,   "jitter",   r.nextFloat());
    maybe (L_STUTTER,  "stutter",  r.nextFloat());
    maybe (L_FEEDBACK, "feedback", 0.20f + 0.80f * r.nextFloat());
    maybe (L_DENSITY,  "density",  0.10f + 0.90f * r.nextFloat());

    // always keep limiter ON by default; user can toggle for A/B
    setParamBool (apvts, "limiterOn", true);

    // output and seed also randomize slightly (no locks for simplicity)
    setParamValue (apvts, "outputDb", -12.0f + 10.0f * r.nextFloat()); // -12..-2
    setParamNorm  (apvts, "seed", r.nextFloat());
}

void GlitchNoiseAudioProcessor::setLock (const juce::String& pid, bool locked)
{
    uint32_t bit = 0;
    if      (pid == "clock")    bit = L_CLOCK;
    else if (pid == "wordSize") bit = L_WORDSIZE;
    else if (pid == "opMorph")  bit = L_OPMORPH;
    else if (pid == "mask")     bit = L_MASK;
    else if (pid == "jitter")   bit = L_JITTER;
    else if (pid == "stutter")  bit = L_STUTTER;
    else if (pid == "feedback") bit = L_FEEDBACK;
    else if (pid == "density")  bit = L_DENSITY;
    else return;

    auto cur = lockMask.load (std::memory_order_acquire);
    if (locked) cur |= bit;
    else        cur &= ~bit;
    lockMask.store (cur, std::memory_order_release);
}

void GlitchNoiseAudioProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presetBank.size())
        return;

    const auto& pr = presetBank[(size_t) index];

    setParamNorm (apvts, "clock",    pr.clock);
    setParamNorm (apvts, "wordSize", pr.wordSize);
    setParamNorm (apvts, "opMorph",  pr.opMorph);
    setParamNorm (apvts, "mask",     pr.mask);
    setParamNorm (apvts, "jitter",   pr.jitter);
    setParamNorm (apvts, "stutter",  pr.stutter);
    setParamNorm (apvts, "feedback", pr.feedback);
    setParamNorm (apvts, "density",  pr.density);
    setParamBool (apvts, "limiterOn", pr.limiterOn);

    setParamValue (apvts, "outputDb", pr.outputDb);
    setParamNorm  (apvts, "seed", pr.seedNorm);
}

GlitchNoiseAudioProcessor::UiStatus GlitchNoiseAudioProcessor::getUiStatus() const
{
    UiStatus s;
    s.opState = engine.getUiOpState();
    s.clockIndex = engine.getUiClockIndex();
    s.microMs = engine.getUiMicroLenMs();
    return s;
}

juce::String GlitchNoiseAudioProcessor::getPresetName (int i) const
{
    if (i < 0 || i >= (int) presetBank.size()) return {};
    return presetBank[(size_t) i].name;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlitchNoiseAudioProcessor();
}
