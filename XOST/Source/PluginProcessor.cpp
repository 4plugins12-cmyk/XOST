#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "SampleTools.h"

using namespace xo;

XOSTProcessor::XOSTProcessor (bool offlineScratch)
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, offlineScratch ? nullptr : &undo, "XOST", createLayout()),
      scratchMode (offlineScratch)
{
    formats.registerBasicFormats();
    WavetableBank::get();   // build factory tables up front
    currentPresetName = Presets::name (0);
    for (int l = 0; l < numMxLanes; ++l) lanes[l] = defaultLane (l);
    rebuildShapes();
    if (! scratchMode) startTimerHz (2);
}

XOSTProcessor::~XOSTProcessor() { stopTimer(); gen.reset(); }

bool XOSTProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void XOSTProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    blockSize = samplesPerBlock;
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatency());
}

void XOSTProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();

    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            if (auto q = pos->getPpqPosition()) { ppq = *q; playing = pos->getIsPlaying(); }
        }

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    Resources r;
    for (int o = 0; o < numOsc; ++o) r.userTables[o] = userLive[o].load();
    r.sample = sampleLive.load();
    engine.setResources (r);

    // render in chunks no larger than the prepared block size
    const int total = buffer.getNumSamples();
    if (total <= blockSize)
        engine.process (buffer, midi, bpm, ppq, playing);
    else
    {
        for (int start = 0; start < total; start += blockSize)
        {
            const int n = std::min (blockSize, total - start);
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, n);
            juce::MidiBuffer sub;
            for (const auto m : midi)
                if (m.samplePosition >= start && m.samplePosition < start + n) sub.addEvent (m.getMessage(), m.samplePosition - start);
            engine.process (view, sub, bpm, ppq + (double) start * bpm / 60.0 / sr, playing);
        }
    }
    if (getLatencySamples() != engine.getLatency()) setLatencySamples (engine.getLatency());

    pushScope (buffer.getReadPointer (0), total);
    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const float load = (float) (secs / ((double) total / sr));
    cpuLoad = cpuLoad.load() * 0.9f + load * 0.1f;
}

void XOSTProcessor::pushScope (const float* data, int n)
{
    int s1, n1, s2, n2;
    scopeFifo.prepareToWrite (n, s1, n1, s2, n2);
    if (n1 > 0) std::copy (data, data + n1, scopeData.begin() + s1);
    if (n2 > 0) std::copy (data + n1, data + n1 + n2, scopeData.begin() + s2);
    scopeFifo.finishedWrite (n1 + n2);
}

void XOSTProcessor::timerCallback()
{
    // free retired wavetables / samples once the audio thread can no longer be using them
    const auto now = juce::Time::getMillisecondCounter();
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(), [now] (const Retired& r) { return now - r.time > 3000; }), graveyard.end());
}

// ================================================================== wavetables & samples
void XOSTProcessor::setUserWavetable (int osc, std::unique_ptr<Wavetable> wt)
{
    if (! juce::isPositiveAndBelow (osc, numOsc)) return;
    userLive[osc] = wt.get();
    graveyard.push_back ({ std::move (userOwned[osc]), nullptr, juce::Time::getMillisecondCounter(), nullptr });
    userOwned[osc] = std::move (wt);
}

bool XOSTProcessor::loadWavetableFile (int osc, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return false;
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, Wavetable::N * Wavetable::maxFrames * 64);
    juce::AudioBuffer<float> buf (1, len);
    reader->read (&buf, 0, len, 0, true, false);
    auto wt = wavetableFromAudio (buf.getReadPointer (0), len, false);
    if (! wt) return false;
    setUserWavetable (osc, std::move (wt));
    if (auto* p = apvts.getParameter (oscId (osc, "table"))) p->setValueNotifyingHost (p->convertTo0to1 (16.0f));
    return true;
}

bool XOSTProcessor::loadSampleFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return false;
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
    if (len < 8) return false;
    juce::AudioBuffer<float> buf ((int) std::min (2u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);
    auto d = std::make_unique<SampleData>();
    d->L.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    if (buf.getNumChannels() > 1) d->R.assign (buf.getReadPointer (1), buf.getReadPointer (1) + len);
    d->sampleRate = reader->sampleRate;
    d->name = file.getFileNameWithoutExtension();
    d->path = file.getFullPathName();
    sampleLive = d.get();
    graveyard.push_back ({ nullptr, std::move (sampleOwned), juce::Time::getMillisecondCounter(), nullptr });
    sampleOwned = std::move (d);
    if (auto* p = apvts.getParameter ("smp_on")) p->setValueNotifyingHost (1.0f);
    return true;
}

void XOSTProcessor::clearSample()
{
    sampleLive = nullptr;
    graveyard.push_back ({ nullptr, std::move (sampleOwned), juce::Time::getMillisecondCounter(), nullptr });
}

// ================================================================== bounce / resample
void XOSTProcessor::renderOffline (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, double rate, double bpm)
{
    Engine off (apvts);
    off.prepare (rate, 512);
    off.reseed (0x2545f491u);
    Resources r;
    for (int o = 0; o < numOsc; ++o) r.userTables[o] = userLive[o].load();
    r.sample = sampleLive.load();
    off.setResources (r);
    off.fx.setOrderFrom (engine.fx);
    off.shapes = engine.shapes.load();
    const int total = out.getNumSamples();
    out.clear();
    double beat = 0.0;
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = std::min (512, total - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
        juce::MidiBuffer m;
        for (const auto meta : midi)
            if (meta.samplePosition >= pos && meta.samplePosition < pos + n)
                m.addEvent (meta.getMessage(), meta.samplePosition - pos);
        off.process (view, m, bpm, beat, false);
        beat += n / rate * bpm / 60.0;
    }
}

juce::File XOSTProcessor::bounceToFile (int note, double seconds)
{
    const double rate = sr > 0 ? sr : 44100.0;
    const int total = (int) (rate * (seconds + 2.5));
    juce::AudioBuffer<float> out (2, total);
    const int noteOffAt = (int) (rate * seconds);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);
    m.addEvent (juce::MidiMessage::noteOff (1, note), noteOffAt);
    renderOffline (out, m, rate, 120.0);
    // trim trailing silence
    int last = total - 1;
    while (last > noteOffAt && out.getMagnitude (last - 64 > 0 ? last - 64 : 0, 64) < 1.0e-4f) last -= 64;
    auto dir = getUserFolder ("Bounces");
    auto file = dir.getNonexistentChildFile (juce::File::createLegalFileName (currentPresetName) + " " + juce::MidiMessage::getMidiNoteName (note, true, true, 3), ".wav");
    juce::WavAudioFormat wav;
    if (auto os = file.createOutputStream())
    {
        std::unique_ptr<juce::OutputStream> stream (os.release());
        if (auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (2).withBitsPerSample (24)))
            writer->writeFromAudioSampleBuffer (out, 0, last + 1);
    }
    return file;
}

// ================================================================== one-shot export
juce::File XOSTProcessor::exportOneShot (const OneShotOptions& o, juce::String* rootOut)
{
    const double rate = sr > 0 ? sr : 48000.0;
    const auto cat = currentCategoryName;
    int note = o.note;
    if (note < 0) note = cat == "Bass" ? 36 : 60;
    // how long the key is held: short for plucks and hits, longer for leads, longest for pads
    static const double holds[] = { 0.35, 1.0, 2.5 };
    double hold;
    if (o.length >= 0 && o.length < 3) hold = holds[o.length];
    else if (cat == "Pad" || cat == "Orchestral" || cat == "Motion") hold = 2.5;
    else if (cat == "Lead" || cat == "Bass" || cat == "Vox") hold = 1.0;
    else hold = 0.5;
    if (apvts.getRawParameterValue ("arp_on")->load() > 0.5f) hold = std::max (hold, 2.0);

    const int maxLen = (int) (rate * (hold + 8.0));
    juce::AudioBuffer<float> raw (2, maxLen);
    juce::MidiBuffer m;
    const int offAt = (int) (rate * hold);
    m.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);
    m.addEvent (juce::MidiMessage::noteOff (1, note), offAt);
    renderOffline (raw, m, rate, 150.0);

    // cut the render where the tail has died away, then trim it like Uno does (silence at the front, fades, normalise)
    int last = maxLen - 1;
    while (last > offAt && raw.getMagnitude (std::max (0, last - 256), 256) < 2.0e-5f) last -= 256;
    juce::AudioBuffer<float> used (2, std::max (64, last + 1));
    for (int ch = 0; ch < 2; ++ch) used.copyFrom (ch, 0, raw, ch, 0, used.getNumSamples());
    auto trim = uno::findTrim (used, rate, -48.0f);
    auto trimmed = uno::makeTrimmed (used, rate, trim, o.normalise);
    if (trimmed.getNumSamples() < 32) return {};

    // root note: what pitch actually sounds (oscillator octave / semitone settings move it); falls back to the played key
    int expected = note + 12 * (int) apvts.getRawParameterValue ("oa_oct")->load() + juce::roundToInt (apvts.getRawParameterValue ("oa_semi")->load());
    int root = expected;
    const auto pitch = uno::detectPitch (trimmed, rate);
    if (pitch.hz > 0.0f && pitch.confidence > 0.5f && apvts.getRawParameterValue ("chord_type")->load() < 0.5f)
    {
        const int det = juce::roundToInt (pitch.midi);
        const int diff = det - expected;
        root = (diff % 12 == 0) ? expected : det;   // octave errors fall back to the expected octave
    }
    const auto rootName = uno::noteName (root, o.middleCOctave);
    if (rootOut != nullptr) *rootOut = rootName;

    auto dir = getUserFolder ("One Shots");
    auto base = juce::File::createLegalFileName (currentPresetName.upToFirstOccurrenceOf (" *", false, false).trim());
    if (base.isEmpty()) base = "XOST";
    auto file = dir.getChildFile (base + " (" + rootName + ").wav");
    for (int n = 2; file.exists() && n < 1000; ++n) file = dir.getChildFile (base + " " + juce::String (n) + " (" + rootName + ").wav");
    juce::WavAudioFormat wav;
    if (auto os = file.createOutputStream())
    {
        std::unique_ptr<juce::OutputStream> stream (os.release());
        if (auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (2).withBitsPerSample (24)))
            writer->writeFromAudioSampleBuffer (trimmed, 0, trimmed.getNumSamples());
    }
    if (! file.existsAsFile() || file.getSize() < 100) return {};
    lastOneShot = file;
    return file;
}

// ================================================================== text-to-sound
xost::Generator& XOSTProcessor::generator()
{
    if (gen == nullptr) gen = std::make_unique<xost::Generator>();
    return *gen;
}

const xost::ParamTable& XOSTProcessor::paramTable()
{
    if (table == nullptr) table = std::make_unique<xost::ParamTable> (xost::makeParamTable (*this));
    return *table;
}

juce::String XOSTProcessor::compactValues() const
{
    xost::PatchValues pv;
    auto& t = const_cast<XOSTProcessor*> (this)->paramTable();
    pv.load (captureValues (Scope::all), t);
    return pv.toString (t, true);
}

void XOSTProcessor::loadVariation (const xost::Variation& v, const juce::String& prompt)
{
    undo.beginNewTransaction ("Generate: " + prompt);
    applyValues (v.values, Scope::all);
    for (int o = 0; o < numOsc; ++o) if (userOwned[o]) setUserWavetable (o, nullptr);
    setFxOrder (defaultFxOrder());
    macroNames = defaultMacroNames();
    currentPresetName = v.name;
    if (v.category.isNotEmpty()) currentCategoryName = v.category;
    currentProgram = -1;
    ++presetChangeCounter;
}

void XOSTProcessor::addHistory (const juce::String& prompt, const xost::Variation& v)
{
    const juce::ScopedLock sl (createLock);
    auto& h = create.history;
    h.erase (std::remove_if (h.begin(), h.end(), [&] (const HistoryItem& i) { return i.values == v.values; }), h.end());
    h.insert (h.begin(), { prompt, v.name, v.values, v.category });
    if (h.size() > 40) h.resize (40);
    ++create.counter;
}

bool XOSTProcessor::resampleToWavetable (int osc, int note)
{
    // render at a rate where one cycle of 'note' is exactly 2048 samples, so every frame is a clean single cycle
    const double hz = juce::MidiMessage::getMidiNoteInHertz (note);
    const double rate = hz * Wavetable::N;
    Engine off (apvts);
    off.prepare (rate, 1024);
    off.bypassFx = true;
    Resources r;
    for (int o = 0; o < numOsc; ++o) r.userTables[o] = userLive[o].load();
    r.sample = sampleLive.load();
    off.setResources (r);
    const int total = (int) (rate * 1.2);
    juce::AudioBuffer<float> out (2, total);
    for (int pos = 0; pos < total; pos += 1024)
    {
        const int n = std::min (1024, total - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
        juce::MidiBuffer m;
        if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);
        off.process (view, m, 120.0, 0.0, false);
    }
    // pick 16 cycles spread over the first second (skipping the very start)
    std::vector<float> cycles;
    const int numCycles = total / Wavetable::N;
    for (int k = 0; k < Wavetable::maxFrames; ++k)
    {
        const int c = 2 + (int) ((double) k * (numCycles - 4) / (Wavetable::maxFrames - 1));
        for (int i = 0; i < Wavetable::N; ++i)
            cycles.push_back (0.5f * (out.getSample (0, c * Wavetable::N + i) + out.getSample (1, c * Wavetable::N + i)));
    }
    auto wt = wavetableFromAudio (cycles.data(), (int) cycles.size(), true);
    if (! wt) return false;
    setUserWavetable (osc, std::move (wt));
    if (auto* p = apvts.getParameter (oscId (osc, "table"))) p->setValueNotifyingHost (p->convertTo0to1 (16.0f));
    return true;
}

// ================================================================== FX order
void XOSTProcessor::setFxOrder (const std::array<int, numFx>& o)
{
    for (int i = 0; i < numFx; ++i) engine.fx.order[i] = o[(size_t) i];
}

std::array<int, numFx> XOSTProcessor::getFxOrder() const
{
    std::array<int, numFx> o {};
    for (int i = 0; i < numFx; ++i) o[(size_t) i] = engine.fx.order[i].load();
    return o;
}

// ================================================================== presets, init, randomize
int XOSTProcessor::getNumPrograms() { return Presets::count(); }
const juce::String XOSTProcessor::getProgramName (int i) { return Presets::name (i); }

void XOSTProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, Presets::count())) return;
    currentProgram = index;
    currentPresetName = Presets::name (index);
    currentCategoryName = Presets::category (index);
    undo.beginNewTransaction ("Load " + currentPresetName);
    applyValues (Presets::factory()[(size_t) index].values, Scope::all);
    for (int o = 0; o < numOsc; ++o) if (userOwned[o]) setUserWavetable (o, nullptr);
    setFxOrder (defaultFxOrder());
    macroNames = defaultMacroNames();
    ++presetChangeCounter;
}

juce::File XOSTProcessor::getUserFolder (const juce::String& sub)
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("XOST").getChildFile (sub);
    d.createDirectory();
    return d;
}

void XOSTProcessor::storeExtras (juce::ValueTree& state) const
{
    juce::StringArray order;
    for (int v : getFxOrder()) order.add (juce::String (v));
    state.setProperty ("fxOrder", order.joinIntoString (","), nullptr);
    for (int o = 0; o < numOsc; ++o)
    {
        const auto key = "userTable" + juce::String (o);
        if (auto* wt = userLive[o].load()) state.setProperty (key, wt->toBase64(), nullptr);
        else state.removeProperty (key, nullptr);
    }
    if (auto* s = sampleLive.load()) state.setProperty ("samplePath", s->path, nullptr);
    else state.removeProperty ("samplePath", nullptr);
    state.setProperty ("macroNames", macroNames.joinIntoString ("|"), nullptr);
    state.setProperty ("randomLocks", (int) randomLocks.load(), nullptr);
    state.setProperty ("uiScale", uiScale, nullptr);
    for (int l = 0; l < numMxLanes; ++l) state.setProperty ("mxLane" + juce::String (l), laneToString (lanes[l]), nullptr);
    state.setProperty ("matrixName", currentMatrixName, nullptr);
    state.setProperty ("rackName", currentRackName, nullptr);
    state.setProperty ("category", currentCategoryName, nullptr);
}

static juce::ValueTree createToTree (const XOSTProcessor::CreateState& c, const XOSTProcessor::OneShotOptions& o)
{
    juce::ValueTree t ("Create");
    t.setProperty ("prompt", c.prompt, nullptr);
    t.setProperty ("selected", c.selected, nullptr);
    t.setProperty ("osNote", o.note, nullptr); t.setProperty ("osLength", o.length, nullptr);
    t.setProperty ("osNorm", o.normalise, nullptr); t.setProperty ("osOct", o.middleCOctave, nullptr);
    for (auto& v : c.variations)
    {
        juce::ValueTree n ("Variation");
        n.setProperty ("name", v.name, nullptr); n.setProperty ("values", v.values, nullptr); n.setProperty ("kind", v.kind, nullptr);
        n.setProperty ("category", v.category, nullptr); n.setProperty ("seed", v.seedName, nullptr); n.setProperty ("summary", v.summary, nullptr);
        t.appendChild (n, nullptr);
    }
    for (auto& h : c.history)
    {
        juce::ValueTree n ("History");
        n.setProperty ("prompt", h.prompt, nullptr); n.setProperty ("name", h.name, nullptr);
        n.setProperty ("values", h.values, nullptr); n.setProperty ("category", h.category, nullptr);
        t.appendChild (n, nullptr);
    }
    return t;
}

static void createFromTree (const juce::ValueTree& t, XOSTProcessor::CreateState& c, XOSTProcessor::OneShotOptions& o)
{
    if (! t.isValid()) return;
    c = {};
    c.prompt = t.getProperty ("prompt").toString();
    c.selected = (int) t.getProperty ("selected", -1);
    o.note = (int) t.getProperty ("osNote", -1); o.length = (int) t.getProperty ("osLength", -1);
    o.normalise = (bool) t.getProperty ("osNorm", true); o.middleCOctave = (int) t.getProperty ("osOct", 5);
    for (auto n : t)
    {
        if (n.hasType ("Variation"))
        {
            xost::Variation v;
            v.name = n["name"].toString(); v.values = n["values"].toString(); v.kind = n["kind"].toString();
            v.category = n["category"].toString(); v.seedName = n["seed"].toString(); v.summary = n["summary"].toString();
            c.variations.push_back (v);
        }
        else if (n.hasType ("History"))
            c.history.push_back ({ n["prompt"].toString(), n["name"].toString(), n["values"].toString(), n["category"].toString() });
    }
}

void XOSTProcessor::restoreExtras (const juce::ValueTree& state)
{
    juce::StringArray order; order.addTokens (state.getProperty ("fxOrder", "0,1,2,3,4,5,6,7").toString(), ",", "");
    std::array<int, numFx> o = defaultFxOrder();
    if (order.size() == numFx) for (int i = 0; i < numFx; ++i) o[(size_t) i] = juce::jlimit (0, numFx - 1, order[i].getIntValue());
    setFxOrder (o);
    for (int k = 0; k < numOsc; ++k)
    {
        const auto data = state.getProperty ("userTable" + juce::String (k)).toString();
        if (data.isNotEmpty()) { auto wt = std::make_unique<Wavetable>(); if (wt->fromBase64 (data)) setUserWavetable (k, std::move (wt)); }
        else if (userOwned[k]) setUserWavetable (k, nullptr);
    }
    juce::StringArray names; names.addTokens (state.getProperty ("macroNames", "").toString(), "|", "");
    if (names.size() == numMacros) macroNames = names;
    randomLocks = (uint32_t) (int) state.getProperty ("randomLocks", (int) randomLocks.load());
    uiScale = (double) state.getProperty ("uiScale", uiScale);
    for (int l = 0; l < numMxLanes; ++l)
    {
        const auto str = state.getProperty ("mxLane" + juce::String (l)).toString();
        lanes[l] = str.isNotEmpty() ? laneFromString (str) : defaultLane (l);
    }
    rebuildShapes();
    currentMatrixName = state.getProperty ("matrixName", "Init").toString();
    currentRackName = state.getProperty ("rackName", "Init").toString();
    const auto path = state.getProperty ("samplePath").toString();
    if (path.isNotEmpty() && juce::File (path).existsAsFile()) loadSampleFile (juce::File (path));
}

bool XOSTProcessor::saveUserPreset (const juce::File& fileIn, const juce::String& category, const juce::String& tags, const juce::String& description)
{
    auto file = fileIn.withFileExtension ("xostpreset");
    auto state = apvts.copyState();
    storeExtras (state);
    state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    state.setProperty ("category", category.isNotEmpty() ? category : juce::String ("User"), nullptr);
    state.setProperty ("tags", tags, nullptr);
    state.setProperty ("description", description, nullptr);
    state.setProperty ("author", "User", nullptr);
    if (auto xml = state.createXml())
        if (xml->writeTo (file))
        {
            currentPresetName = file.getFileNameWithoutExtension();
            currentProgram = -1;
            ++presetChangeCounter;
            return true;
        }
    return false;
}

bool XOSTProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto state = juce::ValueTree::fromXml (*xml);
    const float q = apvts.getRawParameterValue ("quality")->load();
    undo.beginNewTransaction ("Load " + file.getFileNameWithoutExtension());
    apvts.replaceState (state);
    if (auto* p = apvts.getParameter ("quality")) p->setValueNotifyingHost (p->convertTo0to1 (q));
    restoreExtras (state);
    currentPresetName = file.getFileNameWithoutExtension();
    currentCategoryName = state.getProperty ("category", "User").toString();
    currentProgram = -1;
    ++presetChangeCounter;
    return true;
}

void XOSTProcessor::initPatch()
{
    undo.beginNewTransaction ("Init");
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            if (rp->getParameterID() != "quality") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (int o = 0; o < numOsc; ++o) setUserWavetable (o, nullptr);
    clearSample();
    setFxOrder (defaultFxOrder());
    for (int l = 0; l < numMxLanes; ++l) lanes[l] = defaultLane (l);
    rebuildShapes();
    currentPresetName = "Init";
    currentProgram = 0;
    ++presetChangeCounter;
}

const juce::StringArray& XOSTProcessor::sectionNames()
{
    static const juce::StringArray s { "Osc A", "Osc B", "Osc C", "Sub / Noise / Sampler", "Filters", "Envelopes", "LFOs",
                                       "Mod Matrix", "Effects", "Arp", "Voicing", "Matrix", "Minirack" };
    return s;
}

void XOSTProcessor::randomize (uint32_t locks)
{
    undo.beginNewTransaction ("Randomize");
    auto& R = random;
    auto set = [this] (const juce::String& id, float v) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    auto pick = [&R] (std::initializer_list<float> xs) { auto it = xs.begin(); std::advance (it, R.nextInt ((int) xs.size())); return *it; };
    auto chance = [&R] (float p) { return R.nextFloat() < p; };
    auto rng = [&R] (float a, float b) { return a + (b - a) * R.nextFloat(); };
    auto logr = [&R] (float a, float b) { return a * std::pow (b / a, R.nextFloat()); };
    auto unlocked = [locks] (int section) { return (locks & (1u << section)) == 0; };

    for (int o = 0; o < numOsc; ++o)
    {
        if (! unlocked (o)) continue;
        set (oscId (o, "on"), o == 0 ? 1.0f : (chance (0.6f) ? 1.0f : 0.0f));
        set (oscId (o, "table"), (float) R.nextInt (16));
        set (oscId (o, "pos"), R.nextFloat());
        set (oscId (o, "oct"), pick ({ 0, 0, 0, -1, 1 }));
        set (oscId (o, "semi"), pick ({ 0, 0, 0, 0, 12, -12, 7, 5 }));
        set (oscId (o, "fine"), rng (-8, 8));
        set (oscId (o, "level"), rng (0.45f, 0.85f));
        set (oscId (o, "pan"), rng (-0.2f, 0.2f));
        set (oscId (o, "uni"), pick ({ 1, 1, 2, 3, 4, 5, 7 }));
        set (oscId (o, "detune"), rng (0.05f, 0.45f));
        set (oscId (o, "width"), rng (0.4f, 1.0f));
        set (oscId (o, "warp"), chance (0.35f) ? (float) (1 + R.nextInt (6)) : 0.0f);
        set (oscId (o, "warpamt"), rng (0.0f, 0.6f));
        set (oscId (o, "mod"), chance (0.25f) ? (float) (1 + R.nextInt (5)) : 0.0f);
        set (oscId (o, "modamt"), rng (0.0f, 0.45f));
        set (oscId (o, "route"), chance (0.85f) ? 0.0f : pick ({ 1, 2, 3 }));
    }
    if (unlocked (3))
    {
        set ("sub_on", chance (0.3f) ? 1.0f : 0.0f); set ("sub_shape", (float) R.nextInt (4)); set ("sub_level", rng (0.2f, 0.6f));
        set ("noise_on", chance (0.15f) ? 1.0f : 0.0f); set ("noise_type", (float) R.nextInt (4)); set ("noise_level", rng (0.05f, 0.25f));
        set ("noise_color", R.nextFloat());
        if (sampleLive.load() != nullptr) { set ("smp_mode", (float) R.nextInt (3)); set ("smp_gpos", R.nextFloat()); set ("smp_gsize", logr (20, 300)); }
    }
    if (unlocked (4))
    {
        set ("f1_on", chance (0.85f) ? 1.0f : 0.0f);
        set ("f1_type", pick ({ 0, 1, 1, 2, 2, 3, 5, 7, 8, 9 }));
        set ("f1_cutoff", logr (300, 14000)); set ("f1_reso", rng (0, 0.6f)); set ("f1_drive", rng (0, 0.4f));
        set ("f1_env", rng (-0.2f, 0.7f)); set ("f1_key", rng (0, 0.6f));
        set ("f2_on", chance (0.2f) ? 1.0f : 0.0f); set ("f2_type", pick ({ 3, 4, 5, 6 })); set ("f2_cutoff", logr (60, 2000));
        set ("shp_on", chance (0.25f) ? 1.0f : 0.0f); set ("shp_type", (float) R.nextInt (5)); set ("shp_drive", rng (0.1f, 0.6f));
    }
    if (unlocked (5))
        for (int e = 0; e < numEnv; ++e)
        {
            set (envId (e, "delay"), 0.0f); set (envId (e, "hold"), chance (0.15f) ? logr (5, 200) : 0.0f);
            set (envId (e, "attack"), chance (0.8f) ? logr (0.5f, 40) : logr (40, 1500));
            set (envId (e, "decay"), logr (60, 2500)); set (envId (e, "sustain"), chance (0.4f) ? 0.0f : R.nextFloat());
            set (envId (e, "release"), logr (30, 1800)); set (envId (e, "curve"), rng (-0.2f, 0.8f));
        }
    if (unlocked (6))
        for (int l = 0; l < numLfo; ++l)
        {
            set (lfoId (l, "shape"), (float) R.nextInt (10)); set (lfoId (l, "sync"), chance (0.7f) ? 1.0f : 0.0f);
            set (lfoId (l, "div"), pick ({ 3, 4, 6, 7, 9, 10, 11, 13 })); set (lfoId (l, "rate"), logr (0.1f, 8));
            set (lfoId (l, "mode"), (float) R.nextInt (3));
        }
    if (unlocked (7))
    {
        for (int s = 0; s < numSlots; ++s) { set (slotId (s, "src"), 0); set (slotId (s, "dst"), 0); set (slotId (s, "amt"), 0); }
        const int routes[][3] = { { srcEnv2, dstF1Cutoff, 1 }, { srcLfo1, dstAPos, 1 }, { srcLfo2, dstAPitch, 0 }, { srcEnv3, dstAPos, 1 },
                                  { srcVelocity, dstF1Cutoff, 1 }, { srcLfo1, dstF1Cutoff, 1 }, { srcEnv3, dstBPos, 1 }, { srcLfo2, dstAmp, 1 },
                                  { srcModWheel, dstAWarp, 1 }, { srcMacro1, dstF1Cutoff, 1 }, { srcMacro1 + 1, dstReverbMix, 1 }, { srcLfo3, dstVoicePan, 1 } };
        const int count = R.nextInt (5);
        for (int s = 0; s < count; ++s)
        {
            const auto& rt = routes[R.nextInt ((int) std::size (routes))];
            set (slotId (s, "src"), (float) rt[0]); set (slotId (s, "dst"), (float) rt[1]);
            set (slotId (s, "amt"), rt[2] ? rng (-0.6f, 0.6f) : rng (-0.03f, 0.03f));
        }
    }
    if (unlocked (8))
    {
        const char* ons[] = { "dist_on", "ch_on", "ph_on", "fl_on", "dly_on", "rev_on", "eq_on", "comp_on", "tape_on" };
        const float probs[] = { 0.2f, 0.35f, 0.1f, 0.1f, 0.45f, 0.6f, 0.1f, 0.2f, 0.4f };
        for (int k = 0; k < numFx; ++k) set (ons[k], chance (probs[k]) ? 1.0f : 0.0f);
        set ("dist_drive", rng (0.1f, 0.5f)); set ("ch_mix", rng (0.2f, 0.5f)); set ("dly_mix", rng (0.1f, 0.35f));
        set ("dly_fb", rng (0.2f, 0.55f)); set ("rev_mix", rng (0.1f, 0.4f)); set ("rev_size", R.nextFloat()); set ("rev_decay", logr (0.8f, 6));
        set ("tape_wow", rng (0.0f, 0.6f)); set ("tape_flutter", rng (0.0f, 0.5f)); set ("tape_sat", rng (0.1f, 0.6f));
        set ("tape_hiss", rng (0.0f, 0.3f)); set ("tape_age", rng (0.1f, 0.7f)); set ("tape_mix", 1.0f);
    }
    if (unlocked (9))
    {
        set ("arp_on", chance (0.25f) ? 1.0f : 0.0f); set ("arp_mode", (float) R.nextInt (7)); set ("arp_rate", pick ({ 10, 11, 13, 14 }));
        set ("arp_oct", pick ({ 1, 1, 2, 3 })); set ("arp_gate", rng (0.2f, 0.9f));
        for (int s = 0; s < numSteps; ++s) { set (stepId (s, "on"), chance (0.8f) ? 1.0f : 0.0f); set (stepId (s, "tr"), pick ({ 0, 0, 0, 12, 7, -12, 5 })); }
    }
    if (unlocked (11))
    {
        set ("mx_on", chance (0.5f) ? 1.0f : 0.0f);
        for (int l = 0; l < numMxLanes; ++l) set (juce::String ("mx_") + mxLaneIds[l] + "_on", chance (l == 0 ? 0.7f : 0.3f) ? 1.0f : 0.0f);
        const auto& bank = Presets::matrixBank();
        if (! bank.empty()) applyValues (bank[(size_t) R.nextInt ((int) bank.size())].values, Scope::matrix);
    }
    if (unlocked (12))
    {
        const auto& bank = Presets::rackBank();
        if (! bank.empty() && chance (0.6f)) applyValues (bank[(size_t) R.nextInt ((int) bank.size())].values, Scope::rack);
        else set ("rk_on", 0.0f);
    }
    if (unlocked (10))
    {
        set ("voice_mode", chance (0.75f) ? 0.0f : pick ({ 1, 2 }));
        set ("glide", chance (0.5f) ? 0.0f : logr (20, 300));
        set ("drift", chance (0.5f) ? rng (0.05f, 0.5f) : 0.0f);
    }
    currentPresetName = "Random";
    currentProgram = -1;
    ++presetChangeCounter;
}

// ================================================================== state
void XOSTProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    storeExtras (state);
    state.removeChild (state.getChildWithName ("Create"), nullptr);
    {
        const juce::ScopedLock sl (createLock);
        state.appendChild (createToTree (create, oneShot), nullptr);
    }
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void XOSTProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = (int) state.getProperty ("program", 0);
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    {
        const juce::ScopedLock sl (createLock);
        const int counter = create.counter;
        createFromTree (state.getChildWithName ("Create"), create, oneShot);
        create.counter = counter + 1;
    }
    state.removeChild (state.getChildWithName ("Create"), nullptr);
    apvts.replaceState (state);
    restoreExtras (state);
    currentCategoryName = state.getProperty ("category", "Init").toString();
    undo.clearUndoHistory();
    ++presetChangeCounter;
}

juce::AudioProcessorEditor* XOSTProcessor::createEditor() { return new XOSTEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new XOSTProcessor(); }

// ================================================================== value strings
bool XOSTProcessor::inScope (const juce::String& id, Scope scope)
{
    if (scope == Scope::all) return id != "quality";
    if (scope == Scope::matrix) return id.startsWith ("mx_");
    return id.startsWith ("rk");
}

std::map<juce::String, float> XOSTProcessor::normalisedFromValues (const juce::String& values) const
{
    std::map<juce::String, float> out;
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap)) out[rp->getParameterID()] = rp->getDefaultValue();
    for (const auto& t : juce::StringArray::fromTokens (values, " ", ""))
    {
        const int eq = t.indexOfChar ('=');
        if (eq <= 0) continue;
        const auto id = t.substring (0, eq);
        if (auto* rp = apvts.getParameter (id)) out[id] = rp->convertTo0to1 (t.substring (eq + 1).getFloatValue());
    }
    return out;
}

void XOSTProcessor::applyValues (const juce::String& values, Scope scope)
{
    std::map<juce::String, juce::String> tok;
    for (const auto& t : juce::StringArray::fromTokens (values, " ", ""))
    {
        const int eq = t.indexOfChar ('=');
        if (eq > 0) tok[t.substring (0, eq)] = t.substring (eq + 1);
    }
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
        {
            const auto id = rp->getParameterID();
            if (! inScope (id, scope)) continue;
            const auto it = tok.find (id);
            const float v = it != tok.end() ? rp->convertTo0to1 (it->second.getFloatValue()) : rp->getDefaultValue();
            if (std::abs (rp->getValue() - v) > 1.0e-6f) rp->setValueNotifyingHost (v);
        }
    if (scope != Scope::rack)
    {
        for (int l = 0; l < numMxLanes; ++l)
        {
            const auto it = tok.find ("mx" + juce::String (l));
            lanes[l] = it != tok.end() ? laneFromString (it->second) : defaultLane (l);
        }
        rebuildShapes();
    }
}

juce::String XOSTProcessor::captureValues (Scope scope) const
{
    juce::StringArray parts;
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
        {
            const auto id = rp->getParameterID();
            if (! inScope (id, scope)) continue;
            parts.add (id + "=" + juce::String (rp->convertFrom0to1 (rp->getValue()), 4));
        }
    if (scope != Scope::rack)
        for (int l = 0; l < numMxLanes; ++l) parts.add ("mx" + juce::String (l) + "=" + laneToString (lanes[l]));
    return parts.joinIntoString (" ");
}

// ================================================================== Matrix lanes
std::vector<Breakpoint> XOSTProcessor::defaultLane (int lane)
{
    switch (lane)
    {
        case 0: // 1/16 trance gate, every 4th step closed
        {
            std::vector<Breakpoint> p;
            for (int k = 0; k < 16; ++k)
            {
                const float x = (float) k / 16.0f, open = (k % 4 == 3) ? 0.0f : 1.0f;
                p.push_back ({ x, open, 0.0f });
                p.push_back ({ x + 0.6f / 16.0f, open, 0.0f });
                p.push_back ({ x + 0.6f / 16.0f + 0.002f, 0.0f, 0.0f });
                p.push_back ({ x + 1.0f / 16.0f - 0.002f, 0.0f, 0.0f });
            }
            return p;
        }
        case 1: return { { 0.0f, 0.2f, 0.4f }, { 0.5f, 0.85f, -0.4f }, { 0.999f, 0.2f, 0.0f } };
        case 2: return { { 0.0f, 0.5f, 0.0f }, { 0.25f, 0.9f, 0.0f }, { 0.75f, 0.1f, 0.0f } };
        default: return { { 0.0f, 0.0f, 0.0f }, { 0.75f, 0.0f, 0.0f }, { 0.76f, 0.5f, 0.0f } };
    }
}

juce::String XOSTProcessor::laneToString (const std::vector<Breakpoint>& pts)
{
    juce::StringArray s;
    for (const auto& p : pts) s.add (juce::String (p.x, 4) + "," + juce::String (p.y, 4) + "," + juce::String (p.curve, 3));
    return s.joinIntoString (";");
}

std::vector<Breakpoint> XOSTProcessor::laneFromString (const juce::String& str)
{
    std::vector<Breakpoint> pts;
    for (const auto& item : juce::StringArray::fromTokens (str, ";", ""))
    {
        auto f = juce::StringArray::fromTokens (item, ",", "");
        if (f.size() < 2) continue;
        pts.push_back ({ juce::jlimit (0.0f, 0.9999f, f[0].getFloatValue()), juce::jlimit (0.0f, 1.0f, f[1].getFloatValue()),
                         f.size() > 2 ? juce::jlimit (-1.0f, 1.0f, f[2].getFloatValue()) : 0.0f });
    }
    std::sort (pts.begin(), pts.end(), [] (const Breakpoint& a, const Breakpoint& b) { return a.x < b.x; });
    if (pts.empty()) pts.push_back ({ 0.0f, 1.0f, 0.0f });
    return pts;
}

void XOSTProcessor::rebuildShapes()
{
    auto sh = std::make_unique<MatrixShapes>();
    for (int l = 0; l < numMxLanes; ++l) renderLane (lanes[l], sh->lane[l]);
    engine.shapes = sh.get();
    if (! scratchMode) graveyard.push_back ({ nullptr, nullptr, juce::Time::getMillisecondCounter(), std::move (shapesOwned) });
    shapesOwned = std::move (sh);
    ++laneCounter;
}

struct LaneAction : juce::UndoableAction
{
    LaneAction (XOSTProcessor& p, int l, std::vector<Breakpoint> before, std::vector<Breakpoint> after)
        : proc (p), lane (l), a (std::move (before)), b (std::move (after)) {}
    bool perform() override { proc.lanes[(size_t) lane] = b; proc.rebuildShapes(); return true; }
    bool undo() override    { proc.lanes[(size_t) lane] = a; proc.rebuildShapes(); return true; }
    int getSizeInUnits() override { return (int) (a.size() + b.size()) * 12 + 16; }
    XOSTProcessor& proc; int lane; std::vector<Breakpoint> a, b;
};

void XOSTProcessor::setLane (int lane, std::vector<Breakpoint> pts, bool undoable)
{
    if (! juce::isPositiveAndBelow (lane, numMxLanes)) return;
    std::sort (pts.begin(), pts.end(), [] (const Breakpoint& a, const Breakpoint& b) { return a.x < b.x; });
    if (undoable) undo.perform (new LaneAction (*this, lane, lanes[(size_t) lane], std::move (pts)));
    else { lanes[(size_t) lane] = std::move (pts); rebuildShapes(); }
}

void XOSTProcessor::commitLane (int lane, std::vector<Breakpoint> before)
{
    if (! juce::isPositiveAndBelow (lane, numMxLanes)) return;
    undo.beginNewTransaction ("Edit Matrix lane");
    undo.perform (new LaneAction (*this, lane, std::move (before), lanes[(size_t) lane]));
}

// ================================================================== Matrix / Minirack presets
bool XOSTProcessor::saveToolPreset (Scope scope, const juce::File& fileIn, const juce::String& category)
{
    const auto ext = scope == Scope::matrix ? "xostmatrix" : "xostrack";
    auto file = fileIn.withFileExtension (ext);
    const auto text = file.getFileNameWithoutExtension() + "\t" + (category.isNotEmpty() ? category : juce::String ("User"))
                      + "\t\tUser preset\t" + captureValues (scope) + "\n";
    if (! file.replaceWithText (text)) return false;
    (scope == Scope::matrix ? currentMatrixName : currentRackName) = file.getFileNameWithoutExtension();
    return true;
}

bool XOSTProcessor::loadToolPreset (Scope scope, const juce::File& file)
{
    auto f = juce::StringArray::fromTokens (file.loadFileAsString().upToFirstOccurrenceOf ("\n", false, false), "\t", "");
    if (f.size() < 5) return false;
    undo.beginNewTransaction ("Load " + f[0]);
    applyValues (f[4], scope);
    (scope == Scope::matrix ? currentMatrixName : currentRackName) = f[0];
    return true;
}

void XOSTProcessor::loadToolFactory (Scope scope, int index)
{
    const auto& bank = scope == Scope::matrix ? Presets::matrixBank() : Presets::rackBank();
    if (! juce::isPositiveAndBelow (index, (int) bank.size())) return;
    undo.beginNewTransaction ("Load " + bank[(size_t) index].name);
    applyValues (bank[(size_t) index].values, scope);
    (scope == Scope::matrix ? currentMatrixName : currentRackName) = bank[(size_t) index].name;
}

// ================================================================== Mutate / blend / breed
int XOSTProcessor::sectionFor (const juce::String& id)
{
    if (id == "quality" || id == "master_vol" || id.startsWith ("macro") || id == "mpe" || id == "bend_range") return -1;
    if (id.startsWith ("oa_")) return 0;
    if (id.startsWith ("ob_")) return 1;
    if (id.startsWith ("oc_")) return 2;
    if (id.startsWith ("sub_") || id.startsWith ("noise_") || id.startsWith ("smp_")) return 3;
    if (id.startsWith ("f1_") || id.startsWith ("f2_") || id.startsWith ("flt_") || id.startsWith ("shp_")) return 4;
    if (id.startsWith ("e1_") || id.startsWith ("e2_") || id.startsWith ("e3_")) return 5;
    if (id.startsWith ("l1_") || id.startsWith ("l2_") || id.startsWith ("l3_") || id.startsWith ("l4_")) return 6;
    if (id.startsWith ("mm")) return 7;
    if (id.startsWith ("arp_") || (id.startsWith ("st") && id.containsChar ('_')) || id.startsWith ("chord_")) return 9;
    if (id.startsWith ("mx_")) return 11;
    if (id.startsWith ("rk")) return 12;
    if (id == "voice_mode" || id.startsWith ("glide") || id == "polyphony" || id == "vel_sens" || id.startsWith ("amp_") || id == "drift") return 10;
    return 8;   // effects, width
}

void XOSTProcessor::mutate (float amount, uint32_t locks)
{
    undo.beginNewTransaction ("Mutate");
    mutateInternal (amount, locks);
}

void XOSTProcessor::mutateInternal (float amount, uint32_t locks)
{
    auto& R = random;
    auto gauss = [&R] { return (R.nextFloat() + R.nextFloat() + R.nextFloat() - 1.5f) * 1.15f; };
    for (auto* ap : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap);
        if (rp == nullptr) continue;
        const auto id = rp->getParameterID();
        const int sec = sectionFor (id);
        if (sec < 0 || (locks & (1u << sec)) != 0) continue;
        if (id.endsWith ("_src") || id.endsWith ("_dst") || id.contains ("_dst") || id == "polyphony") continue;
        const bool isChoice = dynamic_cast<juce::AudioParameterChoice*> (rp) != nullptr;
        const bool isSwitch = id.endsWith ("_on");
        float v = rp->getValue();
        if (isSwitch) { if (R.nextFloat() < amount * 0.06f) v = v > 0.5f ? 0.0f : 1.0f; }
        else if (isChoice) { if (R.nextFloat() < amount * 0.18f) v = R.nextFloat(); }
        else
        {
            // mod amounts only move while their slot is patched
            if (id.endsWith ("_amt") && id.startsWith ("mm"))
                if (auto* src = apvts.getParameter (id.replace ("_amt", "_src"))) if (src->getValue() < 0.001f) continue;
            v = juce::jlimit (0.0f, 1.0f, v + gauss() * amount * 0.3f);
        }
        if (std::abs (v - rp->getValue()) > 1.0e-6f) rp->setValueNotifyingHost (v);
    }
    // never leave the patch without an oscillator
    bool any = false;
    for (int o = 0; o < numOsc; ++o) any = any || apvts.getRawParameterValue (oscId (o, "on"))->load() > 0.5f;
    if (! any) apvts.getParameter (oscId (0, "on"))->setValueNotifyingHost (1.0f);
    currentPresetName = currentPresetName.upToFirstOccurrenceOf (" *", false, false) + " *";
    currentProgram = -1;
    ++presetChangeCounter;
}

void XOSTProcessor::blendWith (const juce::String& otherValues, float mix, uint32_t locks, bool morph, const juce::String& label, bool newTransaction)
{
    if (newTransaction) undo.beginNewTransaction (morph ? "Morph" : "Breed");
    auto other = normalisedFromValues (otherValues);
    auto& R = random;
    bool takeSection[16];
    for (int k = 0; k < 16; ++k) takeSection[k] = R.nextFloat() < mix;
    for (auto* ap : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap);
        if (rp == nullptr) continue;
        const auto id = rp->getParameterID();
        const int sec = sectionFor (id);
        if (sec < 0 || (locks & (1u << sec)) != 0) continue;
        const float a = rp->getValue(), b = other[id];
        const bool discrete = dynamic_cast<juce::AudioParameterChoice*> (rp) != nullptr || rp->getNumSteps() < 200;
        float v;
        if (morph) v = discrete ? (mix < 0.5f ? a : b) : a + (b - a) * mix;
        else v = takeSection[sec] ? b : a;
        if (std::abs (v - a) > 1.0e-6f) rp->setValueNotifyingHost (v);
    }
    if ((locks & (1u << 11)) == 0 && (morph ? mix >= 0.5f : takeSection[11]))
    {
        std::map<juce::String, juce::String> tok;
        for (const auto& t : juce::StringArray::fromTokens (otherValues, " ", ""))
            if (t.startsWith ("mx") && t.indexOfChar ('=') > 0 && ! t.startsWith ("mx_")) tok[t.upToFirstOccurrenceOf ("=", false, false)] = t.fromFirstOccurrenceOf ("=", false, false);
        for (int l = 0; l < numMxLanes; ++l)
        {
            const auto it = tok.find ("mx" + juce::String (l));
            lanes[l] = it != tok.end() ? laneFromString (it->second) : defaultLane (l);
        }
        rebuildShapes();
    }
    if (! morph) mutateInternal (0.08f, locks);
    currentPresetName = label;
    currentProgram = -1;
    ++presetChangeCounter;
}
