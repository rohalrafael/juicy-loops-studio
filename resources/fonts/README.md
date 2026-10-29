# Bundled fonts

The UI's fonts, the same in every theme, compiled into the app (`juce_add_binary_data` in
`src/app/CMakeLists.txt`). Each is licensed under the SIL Open Font License 1.1; the
licence text is in the `OFL.txt` next to each font.

| Font          | Used for                         | Source                                         |
|---------------|----------------------------------|------------------------------------------------|
| Inter         | All UI text                      | google/fonts `ofl/inter`, static instances     |
| IBM Plex Mono | Displays (tempo, clock, numbers) | Google Fonts, static Medium (500)              |

Inter comes as **Regular (400)**, **Medium (500)** and **SemiBold (600)**, cut from the
google/fonts variable font (`Inter[opsz,wght].ttf`) with fontTools in the sandbox, at the
optical size for small text:

```sh
python3 -m fontTools.varLib.instancer -o Inter-Medium.ttf "Inter[opsz,wght].ttf" wght=500 opsz=14
```

The files keep the family's name, so the app loads them as one family with three weights
(`ThemeManager`) and `LookAndFeel::getTypefaceForFont` picks one by the font's style ("Medium",
"SemiBold").

Static instances are used on purpose. The variable versions load at their default axis values,
which for some families is Thin, and that made text hard to read.
