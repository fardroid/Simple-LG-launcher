# Third-party notices

This project is a modified version of [partridgeworks/webos-launcher](https://github.com/partridgeworks/webos-launcher) (MIT licence, see [LICENSE](LICENSE)).

The MIT licence in [LICENSE](LICENSE) covers this project's own code and its app icon
(`public/icon.png`, `public/largeIcon.png`). The following material belongs to others and
is **not** covered by that licence.

## Bundled software

| Component | Licence | Where |
|---|---|---|
| [Preact](https://preactjs.com) | MIT, © Jason Miller | compiled into `assets/app.js`; licence text in [public/licenses/preact-MIT.txt](public/licenses/preact-MIT.txt) |
| [Inter](https://rsms.me/inter/) typeface | SIL Open Font License 1.1, © The Inter Project Authors | `src/assets/inter-latin-wght.woff2`; licence text in [public/licenses/Inter-OFL.txt](public/licenses/Inter-OFL.txt) |
| Poppins typeface | SIL Open Font License 1.1, © The Poppins Project Authors | `src/assets/poppins-latin-*.woff2`; licence text in [public/licenses/Poppins-OFL.txt](public/licenses/Poppins-OFL.txt) |
| Oswald typeface | SIL Open Font License 1.1, © The Oswald Project Authors | `src/assets/oswald-latin-*.woff2`; licence text in [public/licenses/Oswald-OFL.txt](public/licenses/Oswald-OFL.txt) |
| Roboto Mono typeface | SIL Open Font License 1.1, © The Roboto Mono Project Authors | `src/assets/roboto-mono-latin-*.woff2`; licence text in [public/licenses/RobotoMono-OFL.txt](public/licenses/RobotoMono-OFL.txt) |
| Cormorant Garamond typeface | SIL Open Font License 1.1, © The Cormorant Garamond Project Authors | `src/assets/cormorant-garamond-latin-*.woff2`; licence text in [public/licenses/CormorantGaramond-OFL.txt](public/licenses/CormorantGaramond-OFL.txt) |

The licence texts are included in the installed app under `licenses/`.

## App and input icons

`public/icons/apps/` contains the icons of third-party TV apps, and `public/icons/inputs/`
contains input-type icons from LG's webOS software. They are trademarks or copyright of
their respective owners. This project is not affiliated with, endorsed by or sponsored by
any of them.

They are included in good faith, only so the launcher can show recognisable tiles for apps
and inputs already installed on the user's own TV. Rights holders who would like an icon
removed can open an issue on this repository and it will be removed. Every app tile falls
back to a plain text tile without its icon, so removal doesn't break anything.

"LG" and "webOS" are trademarks of LG Electronics.
