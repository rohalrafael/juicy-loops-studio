#include "plugins/plugin_host.h"

#include <tuple>

namespace jls::plugins
{
namespace
{

std::string saveStateOf(juce::AudioPluginInstance& plugin)
{
    juce::MemoryBlock data;
    plugin.getStateInformation(data);
    return data.toBase64Encoding().toStdString();
}

} // namespace

// ----------------------------------------------------------------------------- HostPlayHead

juce::Optional<juce::AudioPlayHead::PositionInfo> HostPlayHead::getPosition() const
{
    PositionInfo info;
    info.setIsPlaying(position.playing);
    info.setBpm(position.bpm);
    info.setTimeSignature(TimeSignature{position.beatsPerBar, 4});
    info.setPpqPosition(position.beat);
    info.setPpqPositionOfLastBarStart(position.barStartBeat());
    info.setBarCount(position.beatsPerBar > 0
                         ? static_cast<juce::int64>(position.beat / position.beatsPerBar)
                         : 0);
    info.setTimeInSeconds(position.seconds);
    info.setTimeInSamples(position.sample);
    info.setIsLooping(position.loopLengthBeats > 0.0);
    if (position.loopLengthBeats > 0.0)
        info.setLoopPoints(LoopPoints{0.0, position.loopLengthBeats});
    return info;
}

// ----------------------------------------------------------------------------- PluginEffect

PluginEffect::PluginEffect(std::unique_ptr<juce::AudioPluginInstance> pluginInstance)
    : instance(std::move(pluginInstance))
{
    instance->setPlayHead(&playHead);
    // Ask for stereo in/out; plugins that can't do it keep their own layout and we adapt.
    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add(juce::AudioChannelSet::stereo());
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    if (instance->checkBusesLayoutSupported(stereo))
        instance->setBusesLayout(stereo);
}

PluginEffect::~PluginEffect()
{
    if (prepared)
        instance->releaseResources();
}

void PluginEffect::prepare(double sampleRate, int maxBlockSize)
{
    if (prepared)
        instance->releaseResources();
    instance->setRateAndBufferSizeDetails(sampleRate, maxBlockSize);
    instance->prepareToPlay(sampleRate, maxBlockSize);
    const int channels =
        std::max({2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels()});
    scratch.setSize(channels, maxBlockSize);
    midi.ensureSize(256);
    prepared = true;
}

void PluginEffect::reset()
{
    instance->reset();
}

void PluginEffect::process(float* left, float* right, int numSamples, std::span<const float>)
{
    if (!prepared || numSamples > scratch.getNumSamples())
        return;

    const int channels = scratch.getNumChannels();
    scratch.clear(0, numSamples);
    scratch.copyFrom(0, 0, left, numSamples);
    scratch.copyFrom(1, 0, right, numSamples);

    // A view of the first numSamples of the scratch buffer (no allocation).
    juce::AudioBuffer<float> view(scratch.getArrayOfWritePointers(), channels, numSamples);
    midi.clear();
    instance->processBlock(view, midi);

    const bool monoOut = instance->getTotalNumOutputChannels() == 1;
    std::copy_n(view.getReadPointer(0), numSamples, left);
    std::copy_n(view.getReadPointer(monoOut ? 0 : 1), numSamples, right);
}

std::string PluginEffect::saveState() const
{
    return saveStateOf(*instance);
}

// ------------------------------------------------------------------------- PluginInstrument

PluginInstrument::PluginInstrument(std::unique_ptr<juce::AudioPluginInstance> pluginInstance)
    : instance(std::move(pluginInstance))
{
    instance->setPlayHead(&playHead);
    // No audio input, stereo out. Plugins that insist on their own layout keep it; we use the
    // first two output channels (or the only one, for mono synths).
    juce::AudioProcessor::BusesLayout layout;
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    for (int i = 1; i < instance->getBusCount(false); ++i)
        layout.outputBuses.add(juce::AudioChannelSet::disabled());
    for (int i = 0; i < instance->getBusCount(true); ++i)
        layout.inputBuses.add(juce::AudioChannelSet::disabled());
    if (instance->checkBusesLayoutSupported(layout))
        instance->setBusesLayout(layout);
}

PluginInstrument::~PluginInstrument()
{
    if (prepared)
        instance->releaseResources();
}

void PluginInstrument::prepare(double sampleRate, int maxBlockSize)
{
    if (prepared)
        instance->releaseResources();
    instance->setRateAndBufferSizeDetails(sampleRate, maxBlockSize);
    instance->prepareToPlay(sampleRate, maxBlockSize);
    const int channels =
        std::max({2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels()});
    scratch.setSize(channels, maxBlockSize);
    midi.ensureSize(8192); // room for a block's worth of events without allocating
    prepared = true;
}

void PluginInstrument::reset()
{
    instance->reset();
}

void PluginInstrument::process(std::span<const engine::NoteEvent> events, float* left, float* right,
                               int numSamples)
{
    if (!prepared || numSamples > scratch.getNumSamples())
    {
        std::fill_n(left, numSamples, 0.0f);
        std::fill_n(right, numSamples, 0.0f);
        return;
    }

    midi.clear();
    for (const auto& e : events)
    {
        const int pitch = std::clamp(e.pitch, 0, 127);
        switch (e.kind)
        {
        case engine::NoteEvent::Kind::noteOn:
            // Velocity 0 would mean note-off in MIDI, so the softest note is 1.
            midi.addEvent(juce::MidiMessage::noteOn(
                              1, pitch,
                              static_cast<juce::uint8>(std::clamp(
                                  static_cast<int>(std::lround(e.velocity * 127.0f)), 1, 127))),
                          e.sampleOffset);
            break;
        case engine::NoteEvent::Kind::noteOff:
            midi.addEvent(juce::MidiMessage::noteOff(1, pitch), e.sampleOffset);
            break;
        case engine::NoteEvent::Kind::allNotesOff:
            // All-notes-off leaves sustained notes ringing, so lift the pedal first.
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), e.sampleOffset);
            midi.addEvent(juce::MidiMessage::allNotesOff(1), e.sampleOffset);
            break;
        case engine::NoteEvent::Kind::controller:
            midi.addEvent(
                juce::MidiMessage::controllerEvent(
                    1, pitch,
                    std::clamp(static_cast<int>(std::lround(e.velocity * 127.0f)), 0, 127)),
                e.sampleOffset);
            break;
        case engine::NoteEvent::Kind::pitchBend:
            midi.addEvent(
                juce::MidiMessage::pitchWheel(
                    1, std::clamp(static_cast<int>(std::lround(8192.0f + e.velocity * 8192.0f)), 0,
                                  16383)),
                e.sampleOffset);
            break;
        }
    }

    const int channels = scratch.getNumChannels();
    scratch.clear(0, numSamples);
    juce::AudioBuffer<float> view(scratch.getArrayOfWritePointers(), channels, numSamples);
    instance->processBlock(view, midi);

    const int outputs = instance->getTotalNumOutputChannels();
    if (outputs <= 0)
    {
        std::fill_n(left, numSamples, 0.0f);
        std::fill_n(right, numSamples, 0.0f);
        return;
    }
    std::copy_n(view.getReadPointer(0), numSamples, left);
    std::copy_n(view.getReadPointer(outputs == 1 ? 0 : 1), numSamples, right);
}

std::string PluginInstrument::saveState() const
{
    return saveStateOf(*instance);
}

// ----------------------------------------------------------------------------- PluginWindow

PluginWindow::PluginWindow(std::shared_ptr<void> owner, juce::AudioPluginInstance& plugin,
                           const juce::String& title)
    : juce::DocumentWindow(title, juce::Colours::black, juce::DocumentWindow::closeButton),
      keepAlive(std::move(owner))
{
    juce::AudioProcessorEditor* editor =
        plugin.hasEditor() ? plugin.createEditorAndMakeActive() : nullptr;
    if (editor == nullptr)
        editor = new juce::GenericAudioProcessorEditor(plugin);

    setUsingNativeTitleBar(true);
    setContentOwned(editor, true);
    setResizable(editor->isResizable(), false);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
}

PluginWindow::~PluginWindow()
{
    // Delete the editor before releasing our hold on the plugin it belongs to.
    clearContentComponent();
}

void PluginWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}

// ------------------------------------------------------------------------------- PluginHost

PluginHost::PluginHost(const juce::File& settingsFolder)
    : listFile(settingsFolder.getChildFile("plugins.xml")),
      crashFile(settingsFolder.getChildFile("plugin-scan-crash.txt"))
{
    juce::addDefaultFormatsToManager(formats);

    juce::PropertiesFile::Options options;
    options.applicationName = "JuicyLoopsStudio";
    options.filenameSuffix = ".settings";
    scanSettings = std::make_unique<juce::PropertiesFile>(
        settingsFolder.getChildFile("plugin-scan.settings"), options);

    if (auto xml = juce::parseXML(listFile))
        known.recreateFromXml(*xml);
    known.addChangeListener(this);
}

PluginHost::~PluginHost()
{
    known.removeChangeListener(this);
}

void PluginHost::changeListenerCallback(juce::ChangeBroadcaster*)
{
    // Remember scan results between runs.
    if (auto xml = known.createXml())
    {
        listFile.getParentDirectory().createDirectory();
        xml->writeTo(listFile);
    }
}

std::vector<PluginHost::Entry> PluginHost::effects() const
{
    return list(false);
}

std::vector<PluginHost::Entry> PluginHost::instruments() const
{
    return list(true);
}

std::vector<PluginHost::Entry> PluginHost::list(bool instrumentPlugins) const
{
    std::vector<Entry> entries;
    for (const auto& d : known.getTypes())
    {
        if (d.isInstrument != instrumentPlugins)
            continue;
        // "Fx|Delay|Stereo" -> "Delay"; plain "Delay" stays; empty -> "Other".
        auto parts = juce::StringArray::fromTokens(d.category, "|", "");
        parts.removeEmptyStrings();
        if (parts.size() > 1 &&
            (parts[0].equalsIgnoreCase("Fx") || parts[0].equalsIgnoreCase("Instrument")))
            parts.remove(0);
        auto category = parts.isEmpty() ? juce::String("Other") : parts[0];
        category = category.trimCharactersAtEnd(" ").upToLastOccurrenceOf(" Plugin", false, false);
        if (category.isEmpty()) // LV2 classes are named "Delay Plugin", "Reverb Plugin", ...
            category = parts[0];
        const auto maker =
            d.manufacturerName.isEmpty() ? juce::String("Unknown") : d.manufacturerName;
        entries.push_back({d.createIdentifierString().toStdString(), d.name.toStdString(),
                           (maker + " (" + d.pluginFormatName + ")").toStdString(),
                           category.toStdString()});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b)
              {
                  return std::tie(a.manufacturer, a.category, a.name) <
                         std::tie(b.manufacturer, b.category, b.name);
              });
    return entries;
}

std::unique_ptr<engine::EffectProcessor> PluginHost::createEffect(const std::string& id,
                                                                  const std::string& state,
                                                                  double sampleRate, int blockSize)
{
    auto instance = instantiate(id, state, sampleRate, blockSize);
    return instance == nullptr ? nullptr : std::make_unique<PluginEffect>(std::move(instance));
}

std::unique_ptr<engine::InstrumentProcessor> PluginHost::createInstrument(const std::string& id,
                                                                          const std::string& state,
                                                                          double sampleRate,
                                                                          int blockSize)
{
    auto instance = instantiate(id, state, sampleRate, blockSize);
    return instance == nullptr ? nullptr : std::make_unique<PluginInstrument>(std::move(instance));
}

std::unique_ptr<juce::AudioPluginInstance> PluginHost::instantiate(const std::string& id,
                                                                   const std::string& state,
                                                                   double sampleRate, int blockSize)
{
    const auto description = known.getTypeForIdentifierString(juce::String::fromUTF8(id.c_str()));
    if (description == nullptr)
        return nullptr;

    juce::String error;
    auto instance = formats.createPluginInstance(*description, sampleRate, blockSize, error);
    if (instance == nullptr)
    {
        DBG("Couldn't load plugin " << description->name << ": " << error);
        return nullptr;
    }
    if (!state.empty())
    {
        juce::MemoryBlock data;
        if (data.fromBase64Encoding(juce::String::fromUTF8(state.c_str())))
            instance->setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    }
    return instance;
}

void PluginHost::showManager()
{
    auto list = std::make_unique<juce::PluginListComponent>(formats, known, crashFile,
                                                            scanSettings.get(), false);
    list->setSize(720, 480);

    juce::DialogWindow::LaunchOptions options;
    // JUCE dialogs default to light grey; follow the theme instead.
    options.dialogBackgroundColour = juce::LookAndFeel::getDefaultLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId);
    options.content.setOwned(list.release());
    options.dialogTitle = "Plugins";
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.escapeKeyTriggersCloseButton = true;
    options.launchAsync();
}

} // namespace jls::plugins
