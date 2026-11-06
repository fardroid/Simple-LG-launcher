# Third-party notices

This project is a modified version of [partridgeworks/webos-launcher](https://github.com/partridgeworks/webos-launcher) (MIT licence, see [LICENSE](LICENSE)).

The MIT licence in [LICENSE](LICENSE) covers this project's own code and its app icon
(`assets/app/icon.png`, `assets/app/largeIcon.png`). The following material belongs to others and
is **not** covered by that licence.

## Bundled software

| Component | Licence | Where |
|---|---|---|
| [SDL2](https://github.com/webosbrew/SDL-webOS) (webosbrew's SDL-webOS 2.30.12) and SDL2_ttf | zlib | the program links the bundled `libSDL2` and `libSDL2_ttf`; licence text in [licenses/SDL2.txt](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt) of the installed app |
| [FreeType](https://freetype.org) | FreeType License (or GPL 2) | bundled `libfreetype` (draws the text) |
| [libjpeg-turbo](https://libjpeg-turbo.org) | BSD-3-Clause, IJG and zlib | bundled `libturbojpeg` (reads pictures) |
| [curl](https://curl.se) | curl licence (MIT-like) | bundled `libcurl` (the network) |
| [OpenSSL](https://www.openssl.org) 1.1.1 | OpenSSL and SSLeay licences | bundled `libssl` and `libcrypto` |
| [bzip2](https://sourceware.org/bzip2/) | bzip2 licence (BSD-like) | bundled `libbz2` (needed by FreeType) |
| libatomic, libstdc++ and libgcc (GCC) | GPL 3 with the GCC Runtime Library Exception | `libatomic` bundled; the C++ runtime is linked into the program |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT, © Niels Lohmann | [third_party/nlohmann](third_party/nlohmann/LICENSE.MIT), compiled in |
| [pugixml](https://pugixml.org) | MIT, © Arseny Kapoulkine | [third_party/pugixml](third_party/pugixml/LICENSE.md), compiled in (reads RSS and Atom) |
| [QR Code generator](https://www.nayuki.io/page/qr-code-generator-library) | MIT, © Project Nayuki | [third_party/qrcodegen](third_party/qrcodegen/LICENSE.txt), compiled in (the QR code of a news headline) |
| [Inter](https://rsms.me/inter/) typeface | SIL Open Font License 1.1, © The Inter Project Authors | `fonts/inter-*.ttf`; licence text in [fonts/OFL-inter.txt](fonts/OFL-inter.txt) |
| Poppins typeface | SIL Open Font License 1.1, © The Poppins Project Authors | `fonts/poppins-*.ttf`; licence text in [fonts/OFL-poppins.txt](fonts/OFL-poppins.txt) |
| Oswald typeface | SIL Open Font License 1.1, © The Oswald Project Authors | `fonts/oswald-*.ttf`; licence text in [fonts/OFL-oswald.txt](fonts/OFL-oswald.txt) |
| Roboto Mono typeface | SIL Open Font License 1.1, © The Roboto Mono Project Authors | `fonts/roboto-mono-*.ttf`; licence text in [fonts/OFL-robotomono.txt](fonts/OFL-robotomono.txt) |
| Cormorant Garamond typeface | SIL Open Font License 1.1, © The Cormorant Garamond Project Authors | `fonts/cormorant-*.ttf`; licence text in [fonts/OFL-cormorantgaramond.txt](fonts/OFL-cormorantgaramond.txt) |

The licence texts of the project's own code, the fonts, SDL2 and nlohmann/json are included in the installed app under `licenses/`.

## App and input icons

`assets/icons/apps/` contains the icons of third-party TV apps, and `assets/icons/inputs/`
contains input-type icons from LG's webOS software. They are trademarks or copyright of
their respective owners. This project is not affiliated with, endorsed by or sponsored by
any of them.

They are included in good faith, only so the launcher can show recognisable tiles for apps
and inputs already installed on the user's own TV. Rights holders who would like an icon
removed can open an issue on this repository and it will be removed. Every app tile falls
back to a plain text tile without its icon, so removal doesn't break anything.

"LG" and "webOS" are trademarks of LG Electronics.
