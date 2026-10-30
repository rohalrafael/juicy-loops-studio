#!/usr/bin/env python3
"""The showcase project for the README's screenshots (see docs/development.md).

Writes "Neon Ruins.jlp", a trap beat in C# minor at 150 BPM, and "Neon Ruins Kit", the sample
folder the Browser shows: the nine sounds the beat uses, synthesised here (original sounds, no
sample content from anywhere), plus silent placeholders with kit-style names so the folders look
like a real kit. Everything is generated, so the repo holds no audio.

    python3 tools/showcase-project.py OUT_DIR
"""
import json, math, os, random, struct, sys, wave

root = sys.argv[1]
kit = os.path.join(root, "Neon Ruins Kit")
rate = 44100


def write(path, samples):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    peak = max(1e-9, max(abs(s) for s in samples))
    gain = 0.95 / peak if peak > 0.95 else 1.0
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s * gain)) * 32000)) for s in samples))


def onepole(samples, cutoff):
    a = math.exp(-2 * math.pi * cutoff / rate)
    y = 0.0
    out = []
    for s in samples:
        y = (1 - a) * s + a * y
        out.append(y)
    return out


def highpass(samples, cutoff):
    low = onepole(samples, cutoff)
    return [s - l for s, l in zip(samples, low)]


def kick():
    n = int(rate * 0.5)
    phase = 0.0
    out = []
    for i in range(n):
        t = i / rate
        phase += 2 * math.pi * (52 + 160 * math.exp(-t * 35)) / rate
        click = math.exp(-t * 400) * 0.5
        out.append(math.tanh(2.2 * math.sin(phase)) * math.exp(-t * 7) + click)
    return out


def snare():
    random.seed(2)
    n = int(rate * 0.32)
    noise = highpass([random.uniform(-1, 1) for _ in range(n)], 1200)
    out = []
    for i in range(n):
        t = i / rate
        body = math.sin(2 * math.pi * 185 * t) * math.exp(-t * 30)
        out.append(0.8 * body + noise[i] * math.exp(-t * 14))
    return out


def clap():
    random.seed(3)
    n = int(rate * 0.35)
    noise = highpass(onepole([random.uniform(-1, 1) for _ in range(n)], 5000), 800)
    out = []
    for i in range(n):
        t = i / rate
        env = sum(math.exp(-(t - d) * 180) for d in (0.0, 0.011, 0.022) if t >= d)
        env += 0.6 * math.exp(-max(0, t - 0.03) * 16) if t >= 0.03 else 0
        out.append(noise[i] * env)
    return out


def hat(length, decay):
    n = int(rate * length)
    freqs = [205.3, 304.4, 369.6, 522.7, 540.0, 800.0]
    raw = []
    for i in range(n):
        t = i / rate
        s = sum(1 if math.sin(2 * math.pi * f * t) > 0 else -1 for f in freqs) / 6
        raw.append(s)
    hp = highpass(highpass(raw, 7000), 7000)
    return [s * math.exp(-i / rate * decay) for i, s in enumerate(hp)]


def bass808():
    n = int(rate * 2.2)
    phase = 0.0
    out = []
    f = 65.41  # C2: notes play it relative to 60
    for i in range(n):
        t = i / rate
        phase += 2 * math.pi * f * (1 + 0.9 * math.exp(-t * 28)) / rate
        out.append(math.tanh(1.6 * math.sin(phase)) * min(1, t * 400) * math.exp(-t * 1.1))
    return out


def pluck():
    # Karplus-Strong at C4.
    random.seed(4)
    period = rate / 261.63
    size = int(period)
    buf = [random.uniform(-1, 1) for _ in range(size)]
    out = []
    idx = 0
    for i in range(int(rate * 1.6)):
        a = buf[idx % size]
        b = buf[(idx + 1) % size]
        v = 0.996 * 0.5 * (a + b)
        buf[idx % size] = v
        out.append(a)
        idx += 1
    bright = onepole(out, 6000)
    return [s * min(1, i / 40) for i, s in enumerate(bright)]


def pad():
    n = int(rate * 3.0)
    f = 261.63
    detune = [-0.012, -0.004, 0.0, 0.005, 0.011]
    phases = [random.random() for _ in detune]
    raw = []
    for i in range(n):
        t = i / rate
        s = 0.0
        for k, d in enumerate(detune):
            phases[k] = (phases[k] + f * (1 + d) / rate) % 1.0
            s += 2 * phases[k] - 1
        env = min(1, t / 0.6) * (1 if t < 2.2 else max(0, 1 - (t - 2.2) / 0.8))
        raw.append(s / len(detune) * env)
    return onepole(onepole(raw, 1800), 2500)


def riser():
    random.seed(7)
    n = int(rate * 2.4)
    phase = 0.0
    out = []
    for i in range(n):
        t = i / n
        phase += 2 * math.pi * (150 + 2200 * t * t) / rate
        out.append((0.5 * math.sin(phase) + 0.5 * random.uniform(-1, 1)) * t * t)
    return onepole(out, 6000)


real = {
    "808s/808 Afterglow.wav": bass808(),
    "Claps/Clap Neon.wav": clap(),
    "FX/Riser Overdrive.wav": riser(),
    "Hi-Hats/Hat Razor.wav": hat(0.07, 70),
    "Kicks/Kick Blackout.wav": kick(),
    "Melodics/Pluck Ice (C).wav": pluck(),
    "Melodics/Pad Ghost Choir (C).wav": pad(),
    "Open Hats/Open Hat Smoke.wav": hat(0.45, 9),
    "Snares/Snare Halo.wav": snare(),
}
fake = {
    "808s": ["Basement", "Chrome Heart", "Dead Signal", "Glass Floor", "Long Night", "Pink Noise",
             "Static Slide", "Velvet Rope", "Zero Hour"],
    "Claps": ["Afterhours", "Concrete", "Laser Tag", "Night Shift", "Snap Back"],
    "FX": ["Downlifter Ash", "Impact Neon", "Reverse Crash", "Tape Stop", "White Out"],
    "Hi-Hats": ["Ghost", "Ice Pick", "Lo-Fi", "Needle", "Shimmer", "Tick Tock"],
    "Kicks": ["Concrete", "Drip", "Flatline", "Heavy Rotation", "Rumble", "Smoke"],
    "Melodics": ["Bell Frozen (C)", "Choir Void (C)", "Keys Moonlight (C)"],
    "Open Hats": ["Sizzle", "Spray", "Wash"],
    "Percs": ["Rim Chrome", "Shaker Dust", "Tom Low", "Tom Mid", "Woodblock Pink"],
    "Snares": ["Crush", "Glitter", "Rimshot", "Slam", "Static"],
    "Vox": ["Adlib Hey", "Adlib Yeah", "Chant Lo", "Vox Chop 1", "Vox Chop 2"],
}
for path, samples in real.items():
    write(os.path.join(kit, path), samples)
singular = {"808s": "808", "Claps": "Clap", "FX": "", "Hi-Hats": "Hat", "Kicks": "Kick",
            "Melodics": "", "Open Hats": "Open Hat", "Percs": "", "Snares": "Snare", "Vox": ""}
silence = [0.0] * int(rate * 0.1)
for folder, names in fake.items():
    for name in names:
        prefix = singular[folder]
        write(os.path.join(kit, folder, f"{prefix} {name}".strip() + ".wav"), silence)

# ------------------------------------------------------------------------------- The beat
TICKS = 24  # per step


def sample(rel):
    full = os.path.join(kit, rel)
    return {"path": full, "relativePath": os.path.relpath(full, root)}


channels = [
    ("Kick", "Kicks/Kick Blackout.wav", 0.9, 1),
    ("Snare", "Snares/Snare Halo.wav", 0.75, 2),
    ("Clap", "Claps/Clap Neon.wav", 0.6, 2),
    ("Hat", "Hi-Hats/Hat Razor.wav", 0.55, 3),
    ("Open hat", "Open Hats/Open Hat Smoke.wav", 0.45, 3),
    ("808", "808s/808 Afterglow.wav", 0.85, 4),
    ("Pluck", "Melodics/Pluck Ice (C).wav", 0.7, 5),
    ("Pad", "Melodics/Pad Ghost Choir (C).wav", 0.55, 6),
    ("Riser", "FX/Riser Overdrive.wav", 0.5, 7),
]
ids = {name: i + 1 for i, (name, *_rest) in enumerate(channels)}


def steps(seq, pitch=60, length=1, vel=0.8):
    return [[s * TICKS, length * TICKS, pitch, vel] for s in seq]


def melodic(seq):  # (step, length in steps, pitch[, velocity])
    return [[s * TICKS, l * TICKS, p, (rest[0] if rest else 0.8)] for s, l, p, *rest in seq]


kick = steps([0, 10, 16, 19, 26], vel=0.9)
snare = steps([8, 24], vel=0.85)
clap = steps([8, 24], vel=0.7)
hats = []  # on steps only, so the Channel Rack shows them as steps (16ths into each snare)
for st in list(range(0, 12, 2)) + [12, 13, 14, 15] + list(range(16, 28, 2)) + [28, 29, 30, 31]:
    hats.append([st * TICKS, TICKS, 60, 0.8 if st % 4 == 0 else 0.6])
open_hat = steps([6, 22], vel=0.6)

lead_bars = [  # (step, length, pitch): one bar each, over C#m, A, E, B
    [(0, 3, 80), (3, 3, 76), (6, 2, 73), (8, 3, 75), (11, 3, 76), (14, 2, 73)],
    [(0, 3, 76), (3, 3, 73), (6, 2, 69), (8, 3, 71), (11, 3, 73), (14, 2, 76)],
    [(0, 3, 80), (3, 3, 76), (6, 2, 71), (8, 3, 75), (11, 3, 76), (14, 2, 83)],
    [(0, 4, 78), (4, 2, 75), (6, 2, 71), (8, 4, 75), (12, 2, 73), (14, 2, 71)],
]
scale = [1, 3, 4, 6, 8, 9, 11]  # C# minor


def third_below(p):
    i = scale.index(p % 12)
    lower = scale[(i - 2) % 7]
    return p - ((p % 12 - lower) % 12)


random.seed(11)
lead = []
for b, bar in enumerate(lead_bars):
    for s_, l, p in bar:
        accent = 0.9 if s_ % 8 == 0 else (0.75 if s_ % 4 == 0 else 0.62)
        vel = round(min(1, accent + random.uniform(-0.05, 0.05)), 3)
        lead.append([(b * 16 + s_) * TICKS, l * TICKS, p, vel])
        if l >= 3:  # a diatonic third under the longer notes
            lead.append([(b * 16 + s_) * TICKS, l * TICKS, third_below(p), round(vel * 0.8, 3)])
chords = [[61, 64, 68], [57, 61, 64], [56, 59, 64], [59, 63, 66]]
pad_notes = melodic([(b * 16, 16, p, 0.6) for b, chord in enumerate(chords) for p in chord])
bass_bars = [
    [(0, 6, 61), (7, 3, 61), (10, 6, 61)],
    [(0, 6, 57), (7, 3, 57), (10, 4, 57), (14, 2, 61)],
    [(0, 6, 64), (7, 3, 64), (10, 6, 64)],
    [(0, 6, 59), (7, 2, 59), (10, 3, 59), (13, 3, 71)],
]
bass = melodic([(b * 16 + s, l, p, 0.9) for b, bar in enumerate(bass_bars) for s, l, p in bar])
bass_long = melodic([(b * 16, 16, p, 0.8) for b, p in enumerate([61, 57, 64, 59])])


def pattern(pid, name, grid, parts):
    return {"id": pid, "name": name, "gridLength": grid,
            "notes": [{"channel": ids[ch], "notes": notes} for ch, notes in parts]}


patterns = [
    pattern(1, "Intro", 16, [("Pluck", lead), ("Pad", pad_notes)]),
    pattern(2, "Beat", 32, [("Kick", kick), ("Snare", snare), ("Clap", clap), ("Hat", hats),
                            ("Open hat", open_hat), ("808", bass), ("Pluck", lead),
                            ("Pad", pad_notes)]),
    pattern(3, "Break", 16, [("Hat", [h for h in hats if h[0] < 16 * TICKS]),
                             ("808", bass_long), ("Pad", pad_notes)]),
    pattern(4, "Riser", 16, [("Riser", [[0, 32 * TICKS, 60, 0.8]])]),
]

clips = []
for start, pid in [(0, 1), (64, 2), (128, 2), (192, 3), (256, 2), (320, 2)]:
    clips.append({"pattern": pid, "track": 0, "start": start, "length": 64})
for start in (32, 224):
    clips.append({"pattern": 4, "track": 1, "start": start, "length": 32})


def fx(eid, kind, params):
    return {"id": eid, "type": kind, "enabled": True, "params": params}


def track(name, db, effects=(), pan=0.0, width=1.0):
    return {"name": name, "volumeDb": db, "pan": pan, "width": width, "muted": False,
            "solo": False, "effects": list(effects)}


mixer = [
    track("Master", 0.0, [fx(1, "compressor", {"Threshold": -12.0, "Ratio": 3.0, "Attack": 10.0,
                                               "Release": 120.0, "Makeup": 2.0}),
                          fx(2, "softclip", {"Drive": 2.0, "Ceiling": -0.3})]),
    track("Kick", -2.0, [fx(3, "eq", {"Low": 3.0, "Mid": -2.0, "Mid freq": 400.0, "High": 1.0})]),
    track("Snare", -3.0, [fx(4, "reverb", {"Size": 0.4, "Damping": 0.5, "Width": 1.0, "Mix": 0.15})]),
    track("Hats", -6.0, [fx(5, "filter", {"Cutoff": 600.0, "Resonance": 0.1, "Mode": 2.0})], width=1.4),
    track("808", -1.0, [fx(6, "softclip", {"Drive": 6.0, "Ceiling": -1.0})]),
    track("Pluck", -5.0, [fx(7, "delay", {"Time": 300.0, "Feedback": 0.35, "Mix": 0.25}),
                          fx(8, "reverb", {"Size": 0.7, "Damping": 0.4, "Width": 1.0, "Mix": 0.3})],
          pan=0.1),
    track("Pad", -9.0, [fx(9, "filter", {"Cutoff": 3000.0, "Resonance": 0.2, "Mode": 0.0}),
                        fx(10, "reverb", {"Size": 0.85, "Damping": 0.5, "Width": 1.0, "Mix": 0.4})],
          width=1.6),
    track("FX", -8.0),
] + [track(f"Insert {i}", 0.0) for i in range(8, 17)]

project = {
    "format": "juicy-loops-studio-project",
    "version": 1,
    "tempo": 150.0,
    "beatsPerBar": 4,
    "loopChannels": True,
    "channels": [
        {"id": ids[name], "name": name, "volume": vol, "pan": 0.0, "muted": False,
         "mixerTrack": insert, "loop": True, "sample": sample(rel)}
        for name, rel, vol, insert in channels
    ],
    "patterns": patterns,
    "playlist": {"tracks": 16, "clips": clips},
    "mixer": {"tracks": mixer},
    "view": {"currentPattern": 2, "selectedChannel": ids["Pluck"], "selectedMixerTrack": 5,
             "songMode": True, "scaleOn": True, "scale": "minor", "scaleRoot": 1},
    "nextIds": {"channel": len(channels) + 1, "pattern": 5, "effect": 11},
}
with open(os.path.join(root, "Neon Ruins.jlp"), "w") as f:
    json.dump(project, f, indent=2)
print("wrote", root)
