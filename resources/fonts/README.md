# Bundled fonts

The UI's fonts, the same in every theme, compiled into the app (`juce_add_binary_data` in
`src/app/CMakeLists.txt`). Each is licensed under the SIL Open Font License 1.1; the
licence text is in the `OFL.txt` next to each font.

| Font          | Used for                         | Source                                 |
|---------------|----------------------------------|----------------------------------------|
| IBM Plex Sans | All UI text                      | Google Fonts, static Regular (400)     |
| IBM Plex Mono | Displays (tempo, clock, numbers) | Google Fonts, static Medium (500)      |

IBM Plex Sans also has **Medium (500)** and **SemiBold (600)** files for button labels and
titles, cut from the google/fonts variable font with fontTools (in the sandbox):

```sh
fonttools varLib.instancer -o IBMPlexSans-Medium.ttf "IBMPlexSans[wdth,wght].ttf" wght=500 wdth=100
```

They keep the family's name, so the app loads them as one family with three weights
(`ThemeManager`) and `LookAndFeel::getTypefaceForFont` picks one by the font's style ("Medium",
"SemiBold").

Static instances are used on purpose. The variable versions (as in the google/fonts
repo) load at their default axis values, which for some families is Thin, and that made
text hard to read.
