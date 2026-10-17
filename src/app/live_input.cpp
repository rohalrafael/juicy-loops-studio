// Key state is polled with KeyPress::isKeyCurrentlyDown() whenever JUCE reports a key change
// (like juce::MidiKeyboardComponent), so auto-repeat doesn't retrigger notes. Each held key
// remembers the pitch it started; the engine matches note-offs by pitch, so a note ends on the
// channel it started on even if the selection changes meanwhile.

#include "app/live_input.h"

namespace jls
{
namespace
{

// FL's default velocity, which plays samples at unity.
constexpr float keyboardVelocity = 100.0f / 128.0f;

engine::LiveEvent noteOn(std::uint32_t channel, int pitch, float velocity)
{
    return {engine::LiveEvent::Kind::noteOn, channel, pitch, velocity};
}

engine::LiveEvent noteOff(int pitch)
{
    return {engine::LiveEvent::Kind::noteOff, 0, pitch, 0.0f};
}

} // namespace

LiveInput::LiveInput(engine::Engine& e, juce::AudioDeviceManager& d)
    : engine(e), devices(d),
      deviceListConnection(juce::MidiDeviceListConnection::make([this] { enableNewMidiDevices(); }))
{
    // An empty identifier = messages from every enabled MIDI input.
    devices.addMidiInputDeviceCallback({}, this);
    enableNewMidiDevices();
}

LiveInput::~LiveInput()
{
    devices.removeMidiInputDeviceCallback({}, this);
}

const std::array<LiveInput::Key, 29>& LiveInput::keys()
{
    static const std::array<Key, 29> table = []
    {
        std::array<Key, 29> t{};
        const char* lower = "zsxdcvgbhnjm";      // C5..B5
        const char* upper = "q2w3er5t6y7ui9o0p"; // C6..E7
        int i = 0;
        for (int k = 0; lower[k] != 0; ++k, ++i)
            t[static_cast<std::size_t>(i)] = {lower[k], 60 + k};
        for (int k = 0; upper[k] != 0; ++k, ++i)
            t[static_cast<std::size_t>(i)] = {upper[k], 72 + k};
        return t;
    }();
    return table;
}

void LiveInput::setTypingEnabled(bool enabled)
{
    if (!enabled)
        releaseKeys();
    typing = enabled;
}

bool LiveInput::isNoteKey(const juce::KeyPress& key) const
{
    // Keys with a modifier are shortcuts (Ctrl+Z, Shift+D...), never notes.
    if (!typing || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() ||
        key.getModifiers().isAltDown() || key.getModifiers().isShiftDown())
        return false;
    // Only plain character keys: special keys (arrows, F-keys...) have codes whose low byte
    // can look like a letter (Down ends in 'T'), which made them count as note keys.
    const int code = key.getKeyCode();
    if (code <= 0 || code >= 128)
        return false;
    const auto c = static_cast<char>(
        juce::CharacterFunctions::toLowerCase(static_cast<juce::juce_wchar>(code)));
    for (const auto& k : keys())
        if (k.character == c)
            return true;
    return false;
}

bool LiveInput::updateKeys()
{
    if (!typing)
        return false;
    // Shortcuts like Ctrl+Z share keys with notes: don't start notes while a modifier is held.
    const auto mods = juce::ModifierKeys::currentModifiers;
    const bool canStart =
        !mods.isCommandDown() && !mods.isCtrlDown() && !mods.isAltDown() && !mods.isShiftDown();

    bool changed = false;
    for (std::size_t i = 0; i < keys().size(); ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown(keys()[i].character);
        if (down && keyPitch[i] == 0 && canStart)
        {
            keyPitch[i] = keys()[i].pitch;
            engine.sendLive(noteOn(target.load(), keyPitch[i], keyboardVelocity));
            changed = true;
        }
        else if (!down && keyPitch[i] != 0)
        {
            engine.sendLive(noteOff(keyPitch[i]));
            keyPitch[i] = 0;
            changed = true;
        }
    }
    return changed;
}

void LiveInput::releaseKeys()
{
    for (auto& pitch : keyPitch)
        if (pitch != 0)
        {
            engine.sendLive(noteOff(pitch));
            pitch = 0;
        }
}

bool LiveInput::anyKeyDown() const
{
    return std::any_of(keyPitch.begin(), keyPitch.end(), [](int p) { return p != 0; });
}

void LiveInput::enableNewMidiDevices()
{
    // Plug and play: devices are enabled the first time they're seen. One the user switched
    // off in Audio settings stays off (until the app restarts).
    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (seenMidiDevices.insert(device.identifier).second)
            devices.setMidiInputDeviceEnabled(device.identifier, true);
}

void LiveInput::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m)
{
    // MIDI thread. All MIDI channels play the selected channel (omni).
    using Kind = engine::LiveEvent::Kind;
    if (m.isNoteOn())
        engine.sendLive(noteOn(target.load(), m.getNoteNumber(), m.getFloatVelocity()));
    else if (m.isNoteOff())
        engine.sendLive(noteOff(m.getNoteNumber()));
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.sendLive({Kind::releaseAll, 0, 0, 0.0f});
    else if (m.isController())
        engine.sendLive({Kind::controller, target.load(), m.getControllerNumber(),
                         static_cast<float>(m.getControllerValue()) / 127.0f});
    else if (m.isPitchWheel())
        engine.sendLive({Kind::pitchBend, target.load(), 0,
                         static_cast<float>(m.getPitchWheelValue() - 8192) / 8192.0f});
}

} // namespace jls
