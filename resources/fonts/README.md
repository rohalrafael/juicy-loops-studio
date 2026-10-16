# Bundled fonts

Used by the built-in themes and compiled into the app (`juce_add_binary_data` in
`src/app/CMakeLists.txt`). Each is licensed under the SIL Open Font License 1.1; the
licence text is in the `OFL.txt` next to each font.

| Font                | Used by    | Source                                  |
|---------------------|------------|-----------------------------------------|
| IBM Plex Sans       | Juicy      | Google Fonts, static Regular (400)      |
| Bricolage Grotesque | Citrus     | Google Fonts, static Regular (400)      |
| Sora                | Watermelon | Google Fonts, static Regular (400)      |
| Outfit              | Blueberry  | Google Fonts, static Regular (400)      |

Static instances are used on purpose. The variable versions (as in the google/fonts
repo) load at their default axis values, which for some families is Thin, and that made
text hard to read.
