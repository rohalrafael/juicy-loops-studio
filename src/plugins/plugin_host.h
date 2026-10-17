#pragma once

// Hosting third-party plugins (VST3, LV2) through JUCE: scanning and the list of known plugins,
// creating instances as engine effects and instruments, and their editor windows. UI thread,
// except the process() functions, which run on the audio thread.

#include "engine/effects.h"
#include "engine/instrument.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <string>
#include <vector>

namespace jls::plugins
{

// A hosted plugin used as an effect. Preallocates everything in prepare(), so process() only
// hands buffers to the plugin (whose own real-time behaviour is out of our hands).
class PluginEffect final : public engine::EffectProcessor
{
  public:
    explicit PluginEffect(std::unique_ptr<juce::AudioPluginInstance> instance);
    ~PluginEffect() override;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(float* left, float* right, int numSamples, std::span<const float>) override;
    std::string saveState() const override; // base64 of the plugin's state

    juce::AudioPluginInstance& plugin() { return *instance; }

  private:
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::AudioBuffer<float> scratch; // the plugin may have more than two channels
    juce::MidiBuffer midi;            // effects get no MIDI, but processBlock needs a buffer
    bool prepared = false;
};

// A hosted plugin used as a channel's instrument. Note events become MIDI messages at their
// sample offsets; the MIDI buffer and audio scratch are preallocated in prepare().
class PluginInstrument final : public engine::InstrumentProcessor
{
  public:
    explicit PluginInstrument(std::unique_ptr<juce::AudioPluginInstance> instance);
    ~PluginInstrument() override;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(std::span<const engine::NoteEvent> events, float* left, float* right,
                 int numSamples) override;
    std::string saveState() const override; // base64 of the plugin's state

    juce::AudioPluginInstance& plugin() { return *instance; }

  private:
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer midi;
    bool prepared = false;
};

// A plugin's own editor (or a generic one if it has none) in a separate window. Holds the
// plugin's owner (a PluginEffect or PluginInstrument) so the plugin outlives its editor.
class PluginWindow final : public juce::DocumentWindow
{
  public:
    PluginWindow(std::shared_ptr<void> owner, juce::AudioPluginInstance& plugin,
                 const juce::String& title);
    ~PluginWindow() override;

    std::function<void()> onClose;
    void closeButtonPressed() override;
    // What the window holds on to, to tell whether its plugin was replaced.
    const void* owner() const { return keepAlive.get(); }

  private:
    std::shared_ptr<void> keepAlive;
};

// Known plugins (persisted), scanning, and creating instances.
class PluginHost final : private juce::ChangeListener
{
  public:
    // `settingsFolder`: where the plugin list and scan state are kept.
    explicit PluginHost(const juce::File& settingsFolder);
    ~PluginHost() override;

    struct Entry
    {
        std::string id;           // stored in projects (PluginDescription::createIdentifierString)
        std::string name;         // the plugin's name
        std::string manufacturer; // with the format, e.g. "LSP VST3 (VST3)", for menu grouping
        std::string category;     // main category, e.g. "Delay" from "Fx|Delay|Stereo"
    };
    // Known effect plugins (not instruments), sorted by manufacturer, category, name.
    std::vector<Entry> effects() const;
    // Known instrument plugins, sorted the same way.
    std::vector<Entry> instruments() const;

    // Creates and restores a plugin effect; null if it isn't known or fails to load.
    std::unique_ptr<engine::EffectProcessor>
    createEffect(const std::string& id, const std::string& state, double sampleRate, int blockSize);
    // The same for instruments.
    std::unique_ptr<engine::InstrumentProcessor> createInstrument(const std::string& id,
                                                                  const std::string& state,
                                                                  double sampleRate, int blockSize);

    // Options > Plugins: the list of known plugins with scanning (JUCE's PluginListComponent).
    void showManager();

  private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    std::vector<Entry> list(bool instrumentPlugins) const;
    std::unique_ptr<juce::AudioPluginInstance>
    instantiate(const std::string& id, const std::string& state, double sampleRate, int blockSize);

    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList known;
    juce::File listFile;
    juce::File crashFile; // "dead man's pedal": a plugin that crashes the scan is skipped next time
    std::unique_ptr<juce::PropertiesFile> scanSettings;
};

} // namespace jls::plugins
