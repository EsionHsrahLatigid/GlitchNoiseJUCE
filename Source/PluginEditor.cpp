#include "PluginEditor.h"

GlitchNoiseAudioProcessorEditor::Os9WebUI::Os9WebUI (GlitchNoiseAudioProcessor& p)
: juce::WebBrowserComponent(), proc (p)
{
    loadUi();
}

static juce::String buildPresetOptions (GlitchNoiseAudioProcessor& proc)
{
    juce::String opts;
    const int n = proc.getPresetCount();
    for (int i = 0; i < n; ++i)
        opts << "<option value='" << i << "'>" << proc.getPresetName(i) << "</option>";
    return opts;
}

juce::String GlitchNoiseAudioProcessorEditor::Os9WebUI::makeHtml (GlitchNoiseAudioProcessor& proc)
{
    const auto presetOptions = buildPresetOptions (proc);

    juce::String html;
    html << R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>GlitchNoiseJUCE</title>
<style>
  :root{
    --bg:#C0C0C0;
    --panel:#D0D0D0;
    --dark:#808080;
    --light:#FFFFFF;
    --text:#000000;
  }
  body{
    margin:0; padding:0;
    background:var(--bg);
    font-family: Chicago, Charcoal, Geneva, "MS Sans Serif", system-ui, sans-serif;
    color:var(--text);
    user-select:none;
  }
  .wrap{ padding:10px; }
  .titlebar{
    background:var(--panel);
    border-top:2px solid var(--light);
    border-left:2px solid var(--light);
    border-right:2px solid var(--dark);
    border-bottom:2px solid var(--dark);
    padding:6px 8px;
    font-weight:700;
    display:flex; justify-content:space-between; align-items:center;
  }
  .panel{
    margin-top:10px;
    background:var(--panel);
    border-top:2px solid var(--light);
    border-left:2px solid var(--light);
    border-right:2px solid var(--dark);
    border-bottom:2px solid var(--dark);
    padding:10px;
  }
  .grid{
    display:grid;
    grid-template-columns: 1fr 1fr;
    gap:10px;
  }
  .row{
    display:flex;
    align-items:center;
    gap:8px;
  }
  label{ width:82px; font-size:12px; }
  input[type="range"]{ width:100%; }
  .lock{ width:16px; height:16px; }
  .btn{
    background:var(--panel);
    border-top:2px solid var(--light);
    border-left:2px solid var(--light);
    border-right:2px solid var(--dark);
    border-bottom:2px solid var(--dark);
    padding:4px 10px;
    font-size:12px;
    cursor:pointer;
  }
  .btn:active{
    border-top:2px solid var(--dark);
    border-left:2px solid var(--dark);
    border-right:2px solid var(--light);
    border-bottom:2px solid var(--light);
  }
  .small{ font-size:11px; opacity:0.9; }
  .tog{ display:flex; align-items:center; gap:6px; }
  select{
    background:var(--panel);
    border-top:2px solid var(--light);
    border-left:2px solid var(--light);
    border-right:2px solid var(--dark);
    border-bottom:2px solid var(--dark);
    padding:2px 6px;
    font-size:12px;
  }
</style>
</head>
<body>
<div class="wrap">
  <div class="titlebar">
    <div>GlitchNoiseJUCE</div>
    <div class="small">OS9-ish</div>
  </div>

  <div class="panel">
    <div style="display:flex; justify-content:space-between; align-items:center; gap:10px;">
      <div style="display:flex; align-items:center; gap:8px;">
        <label style="width:auto;">Preset</label>
        <select id="preset">
)HTML";

    html << presetOptions;

    html << R"HTML(
        </select>
        <button class="btn" id="loadPreset">Load</button>
      </div>
      <div class="tog">
        <input id="limiterOn" type="checkbox">
        <label for="limiterOn" style="width:auto;">Limiter</label>
      </div>
    </div>

    <div class="grid" style="margin-top:10px;">
      <div class="row"><label>Clock</label>    <input id="clock"    type="range" min="0" max="1" step="0.0001"><input class="lock" id="lock_clock" type="checkbox" title="Lock"></div>
      <div class="row"><label>WordSize</label> <input id="wordSize" type="range" min="0" max="1" step="0.0001"><input class="lock" id="lock_wordSize" type="checkbox" title="Lock"></div>
      <div class="row"><label>OpMorph</label>  <input id="opMorph"  type="range" min="0" max="1" step="0.0001"><input class="lock" id="lock_opMorph" type="checkbox" title="Lock"></div>
      <div class="row"><label>Mask</label>     <input id="mask"     type="range" min="0" max="1" step="0.0001"><input class="lock" id="lock_mask" type="checkbox" title="Lock"></div>
      <div class="row"><label>Jitter</label>   <input id="jitter"   type="range" min="0" max="1" step="0.0001"><input class="lock" id="lock_jitter" type="checkbox" title="Lock"></div>
      <div class="row"><label>Stutter</label>  <input id="stutter"  type="range" min="0" max="1" step="0.0001"><input class="lock" id="lock_stutter" type="checkbox" title="Lock"></div>
      <div class="row"><label>Feedback</label> <input id="feedback" type="range" min="0" max="1" step="0.0001"><input class="lock" id="lock_feedback" type="checkbox" title="Lock"></div>
      <div class="row"><label>Density</label>  <input id="density"  type="range" min="0" max="1" step="0.0001"><input class="lock" id="lock_density" type="checkbox" title="Lock"></div>
    </div>

    <div style="margin-top:10px; display:flex; justify-content:space-between; align-items:center;">
      <div class="small">
        Randomize respects locks. Limiter is for A/B; keep ON by default.
      </div>
      <div style="display:flex; gap:8px;">
        <button class="btn" id="randomize">Randomize</button>
        <button class="btn" id="panic">Panic</button>
      </div>
    </div>
  </div>
</div>

<script>
  function sendSet(pid, value){
    window.location.href = `juce://set?pid=${encodeURIComponent(pid)}&value=${encodeURIComponent(value)}`;
  }
  function sendCmd(name){
    window.location.href = `juce://cmd?name=${encodeURIComponent(name)}`;
  }
  function sendLock(pid, value){
    window.location.href = `juce://lock?pid=${encodeURIComponent(pid)}&value=${encodeURIComponent(value)}`;
  }
  function sendPreset(index){
    window.location.href = `juce://preset?index=${encodeURIComponent(index)}`;
  }

  const ids = ["clock","wordSize","opMorph","mask","jitter","stutter","feedback","density"];
  ids.forEach(id=>{
    const el = document.getElementById(id);
    el.addEventListener("input", ()=> sendSet(id, el.value));

    const lk = document.getElementById("lock_" + id);
    lk.addEventListener("change", ()=> sendLock(id, lk.checked ? "1" : "0"));
  });

  const lim = document.getElementById("limiterOn");
  lim.addEventListener("change", ()=> sendSet("limiterOn", lim.checked ? "1" : "0"));

  document.getElementById("randomize").addEventListener("click", ()=> sendCmd("randomize"));
  document.getElementById("panic").addEventListener("click", ()=> sendCmd("panic"));

  const presetSel = document.getElementById("preset");
  document.getElementById("loadPreset").addEventListener("click", ()=> sendPreset(presetSel.value));
</script>
</body>
</html>
)HTML";
    return html;
}

static juce::String makeDataUrlString (const juce::String& html)
{
    return "data:text/html;charset=utf-8," + juce::URL::addEscapeChars (html, true);
}

juce::URL GlitchNoiseAudioProcessorEditor::Os9WebUI::makeDataUrl (const juce::String& html)
{
    auto escaped = juce::URL::addEscapeChars (html, true);
    return juce::URL ("data:text/html;charset=utf-8," + escaped);
}

void GlitchNoiseAudioProcessorEditor::Os9WebUI::loadUi()
{
    goToURL (makeDataUrlString (makeHtml (proc)));
}

juce::String GlitchNoiseAudioProcessorEditor::Os9WebUI::getAction (const juce::String& juceUrl)
{
    auto s = juceUrl.fromFirstOccurrenceOf ("juce://", false, false);
    if (s.containsChar ('?'))
        return s.upToFirstOccurrenceOf ("?", false, false);
    return s;
}

juce::String GlitchNoiseAudioProcessorEditor::Os9WebUI::getQuery (const juce::String& juceUrl)
{
    return juceUrl.fromFirstOccurrenceOf ("?", false, false);
}

juce::String GlitchNoiseAudioProcessorEditor::Os9WebUI::getParam (const juce::String& query, const juce::String& key)
{
    juce::StringArray parts;
    parts.addTokens (query, "&", "");

    for (auto& part : parts)
    {
        const auto k = part.upToFirstOccurrenceOf ("=", false, false);
        if (k == key)
        {
            auto v = part.fromFirstOccurrenceOf ("=", false, false);
            return juce::URL::removeEscapeChars (v);
        }
    }
    return {};
}

bool GlitchNoiseAudioProcessorEditor::Os9WebUI::pageAboutToLoad (const juce::String& newURL)
{
    if (newURL.startsWith ("juce://"))
    {
        const auto action = getAction (newURL);
        const auto query  = getQuery (newURL);

        if (action == "set")
        {
            const auto pid = getParam (query, "pid");
            const auto val = getParam (query, "value");

            if (pid.isNotEmpty())
            {
                if (pid == "limiterOn")
                {
                    if (auto* p = proc.apvts.getParameter ("limiterOn"))
                    {
                        p->beginChangeGesture();
                        p->setValueNotifyingHost (val == "1" ? 1.0f : 0.0f);
                        p->endChangeGesture();
                    }
                }
                else
                {
                    if (auto* p = proc.apvts.getParameter (pid))
                    {
                        const float f = (float) val.getDoubleValue(); // sliders are 0..1
                        p->beginChangeGesture();
                        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, f));
                        p->endChangeGesture();
                    }
                }
            }
        }
        else if (action == "cmd")
        {
            const auto name = getParam (query, "name");
            if (name == "panic")      proc.triggerPanic();
            if (name == "randomize")  proc.randomizeParams();
        }
        else if (action == "lock")
        {
            const auto pid = getParam (query, "pid");
            const auto val = getParam (query, "value");
            proc.setLock (pid, val == "1");
        }
        else if (action == "preset")
        {
            const auto idx = getParam (query, "index").getIntValue();
            proc.loadPreset (idx);
        }

        return false;
    }

    return true;
}

GlitchNoiseAudioProcessorEditor::GlitchNoiseAudioProcessorEditor (GlitchNoiseAudioProcessor& p)
: AudioProcessorEditor (&p), processor (p), web (p)
{
    setSize (460, 300);
    addAndMakeVisible (web);
}

void GlitchNoiseAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void GlitchNoiseAudioProcessorEditor::resized()
{
    web.setBounds (getLocalBounds());
}
