#include "plugins/plugin_host.h"

namespace jls::plugins
{

// ----------------------------------------------------------------------------- PluginEffect

PluginEffect::PluginEffect(std::unique_ptr<juce::AudioPluginInstance> pluginInstance)
    : instance(std::move(pluginInstance))
{
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
    juce::MemoryBlock data;
    instance->getStateInformation(data);
    return data.toBase64Encoding().toStdString();
}

// ----------------------------------------------------------------------------- PluginWindow

PluginWindow::PluginWindow(std::shared_ptr<engine::EffectProcessor> processor,
                           const juce::String& title)
    : juce::DocumentWindow(title, juce::Colours::black, juce::DocumentWindow::closeButton),
      keepAlive(std::move(processor))
{
    auto* effect = dynamic_cast<PluginEffect*>(keepAlive.get());
    jassert(effect != nullptr);
    auto& plugin = effect->plugin();
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
    std::vector<Entry> list;
    for (const auto& d : known.getTypes())
        if (!d.isInstrument)
            list.push_back({d.createIdentifierString().toStdString(),
                            (d.name + " (" + d.manufacturerName + ", " + d.pluginFormatName + ")")
                                .toStdString()});
    std::sort(list.begin(), list.end(),
              [](const Entry& a, const Entry& b) { return a.name < b.name; });
    return list;
}

std::unique_ptr<engine::EffectProcessor> PluginHost::createEffect(const std::string& id,
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
    return std::make_unique<PluginEffect>(std::move(instance));
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
