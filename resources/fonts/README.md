# Bundled fonts

Used by the built-in themes and compiled into the app (`juce_add_binary_data` in
`src/app/CMakeLists.txt`). Each is licensed under the SIL Open Font License 1.1; the
licence text is in the `OFL.txt` next to each font.

| Font                | Used by    | Source                                  |
|---------------------|------------|-----------------------------------------|
| IBM Plex Sans       | Juicy      | Google Fonts, static Regular (400)      |
| Bricolage Grotesque | Citrus     | Google Fonts, static Regular (400)      |
| Sora                | Watermelon | Google Fonts, static Regular (400)      |
| Outfit              | Blueberry, Blackberry | Google Fonts, static Regular (400) |
| IBM Plex Mono       | Displays (tempo, clock) in every theme | Google Fonts, static Medium (500) |

Each family also has **Medium (500)** and **SemiBold (600)** files for button labels and titles,
cut from the google/fonts variable fonts with fontTools (in the sandbox):

```sh
fonttools varLib.instancer -o IBMPlexSans-Medium.ttf "IBMPlexSans[wdth,wght].ttf" wght=500 wdth=100
fonttools varLib.instancer -o Outfit-SemiBold.ttf "Outfit[wght].ttf" wght=600
# Bricolage Grotesque: wght=... wdth=drop opsz=drop
```

They keep the family's name, so the app files them by family and weight (`ThemeManager`) and
`LookAndFeel::getTypefaceForFont` picks one by the font's style ("Medium", "SemiBold").

Static instances are used on purpose. The variable versions (as in the google/fonts
repo) load at their default axis values, which for some families is Thin, and that made
text hard to read.
