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

## Wallpaper collection

The photos of the wallpaper collection (Settings → Wallpaper → Picture → *Collection*) are not in this repository or in the
package: they are files of the [`wallpapers-2`](https://github.com/fardroid/Simple-LG-launcher/releases/tag/wallpapers-2) release (their previews, of `wallpapers-1`),
listed in [`repo/wallpapers.json`](repo/wallpapers.json), and the TV downloads only the ones you tick. They come from
[Wikimedia Commons](https://commons.wikimedia.org/) (its featured pictures) and keep their own licences, below. They were
scaled and cropped to 1920×1080 and sharpened a little (an unsharp mask); the licences marked SA ask that these copies stay under the same licence. Each one's
author, licence and source are listed below. None of the authors endorses this project. The collection's fourth group, *Abstract*,
holds this project's own pictures from the [`wallpapers`](wallpapers) folder, under its MIT licence.

### Mountains

- [Matterhorn from Lake Grindji](https://commons.wikimedia.org/wiki/File:CH.VS.Zermatt_Sunnegga_Grindjisee_Matterhorn_9034_16x9-R_16K.jpg), Roy Egloff, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Piza Scotoni, Dolomites](https://commons.wikimedia.org/wiki/File:Piza_Scotoni_Piza_Fanes_de_Medo_Val_Badia.jpg), Wolfgang Moroder, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Lake Louise, Banff](https://commons.wikimedia.org/wiki/File:1_lake_louise_pano_2019.jpg), Chensiyuan, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Langkofel from Plan de Cunfin](https://commons.wikimedia.org/wiki/File:Saslonch_da_Plan_de_Cunfin.jpg), Wolfgang Moroder, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Sella group from the Gardena Pass](https://commons.wikimedia.org/wiki/File:Frea_pass_Sella_group_Pisciadu_Dolomites_South_Tyrol.jpg), Wolfgang Moroder, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Five Fingers Peak, Zhangjiajie](https://commons.wikimedia.org/wiki/File:1_zhangjiajie_huangshizhai_wulingyuan_panorama_2012.jpg), Chensiyuan, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Odle peaks and Swiss pines](https://commons.wikimedia.org/wiki/File:Odles_Cisles_y_Mastle_pinus_Cembra.jpg), Wolfgang Moroder, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Odle peaks from Seceda](https://commons.wikimedia.org/wiki/File:Odles_da_Sec%C3%ABda_Gherd%C3%ABina.jpg), Wolfgang Moroder, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Falzarego Pass and Sass de Stria](https://commons.wikimedia.org/wiki/File:Cappella_Passo_Falzarego_Sass_Stria.jpg), Wolfgang Moroder, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Ustyurt plateau, Bozjyra](https://commons.wikimedia.org/wiki/File:%D0%A7%D0%B8%D0%BD%D0%BA%D0%B8_%D0%BF%D0%BB%D0%B0%D1%82%D0%BE_%D0%A3%D1%81%D1%82%D1%8E%D1%80%D1%82._%D0%A3%D1%80%D0%BE%D1%87%D0%B8%D1%89%D0%B5_%D0%91%D0%BE%D0%B7%D0%B6%D1%8B%D1%80%D0%B0.jpg), Maksat79, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Juac meadows and the Odle](https://commons.wikimedia.org/wiki/File:Juac_y_Odles.jpg), Wolfgang Moroder, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Lac de Roy, Haute-Savoie](https://commons.wikimedia.org/wiki/File:Lac_de_Roy_10.jpg), Krzysztof Golik, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Säuleck, Hohe Tauern](https://commons.wikimedia.org/wiki/File:Mallnitz_S%C3%A4uleck_01.jpg), Uoaei1, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Teide, Tenerife](https://commons.wikimedia.org/wiki/File:Teide_von_Nordosten_%28Zuschnitt_1%29.jpg), Thomas Wolf, [CC BY-SA 3.0 de](https://creativecommons.org/licenses/by-sa/3.0/de/)
- [Watzmann above the fog](https://commons.wikimedia.org/wiki/File:Watzmann_%28Stangga%C3%9F%29.jpg), Jörg Braukmann, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Tianzi Mountain, Zhangjiajie](https://commons.wikimedia.org/wiki/File:1_tianzishan_wulingyuan_zhangjiajie_2012.jpg), Chensiyuan, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Ararat and Khor Virap](https://commons.wikimedia.org/wiki/File:Monasterio_Khor_Virap,_Armenia,_2016-10-01,_DD_25.jpg), Diego Delso, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Vrtača from Kosiak, Karawanks](https://commons.wikimedia.org/wiki/File:Kosiak_-_Wertatscha_20230714.jpg), Uoaei1, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Chiemgau Alps at sunrise](https://commons.wikimedia.org/wiki/File:Hochries,_Alpes_del_Chiemgau,_Alemania,_2024-10-19,_DD_34-36_HDR.jpg), Diego Delso, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Tatra Mountains in winter](https://commons.wikimedia.org/wiki/File:Winter_in_Tatry_Mountains_-_Poland.jpg), Radek Kucharski, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- [Watzmann, west face](https://commons.wikimedia.org/wiki/File:Watzmann_%28Westseite%29.jpg), Jörg Braukmann, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Spullersee, Lechquellen Mountains](https://commons.wikimedia.org/wiki/File:2015_Spullersee_im_Lechquellengebirge.jpg), Böhringer, [CC BY-SA 3.0 at](https://creativecommons.org/licenses/by-sa/3.0/at/)
- [View west from Piz Boè](https://commons.wikimedia.org/wiki/File:West_view_from_Piz_Boe_16to9.jpg), Wolfgang Moroder, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Tauern valley, Mallnitz](https://commons.wikimedia.org/wiki/File:Mallnitz_Tauerntal_Zedelnig_20160807.jpg), Uoaei1, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Farmhouses in Colfosco](https://commons.wikimedia.org/wiki/File:Sor%C3%A0_y_Zecca_da_Ruatsch_a_Calfosch_da_pert_de_mesdi.jpg), Wolfgang Moroder, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Cogne valley from Colle Tsa Sètse](https://commons.wikimedia.org/wiki/File:Bergtocht_van_Gimillan_%281805m.%29_naar_Colle_Tsa_S%C3%A8tse_in_Cogne_Valley_%28Itali%C3%AB%29._Zicht_op_de_omringende_alpentoppen_van_Gran_Paradiso_01.jpg), Dominicus Johannes Bergsma, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Piz Nuna from Motta Naluns](https://commons.wikimedia.org/wiki/File:Scuol-Motta_Naluns,_11-10-2024._%28actm.%29_21.jpg), Agnes Monkelbaan, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Dusk in Val Sinestra](https://commons.wikimedia.org/wiki/File:Val_Sinestra._11-10-2024._%28d.j.b.%29_03.jpg), Dominicus Johannes Bergsma, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Gran Paradiso massif from Cogne](https://commons.wikimedia.org/wiki/File:Bergtocht_van_Gimillan_%281805m.%29_naar_Colle_Tsa_S%C3%A8tse_in_Cogne_Valley_%28Itali%C3%AB%29._Zicht_op_de_omringende_alpen_01.jpg), Dominicus Johannes Bergsma, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)

### Nature

- [Boulders on the Pärispea coast](https://commons.wikimedia.org/wiki/File:Eesti_p%C3%B5hjarannik.jpg), Abrget47j, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Grose Valley, Blue Mountains](https://commons.wikimedia.org/wiki/File:Grose_Valley,_NSW,_Australia_-_April_2013.jpg), Diliff, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Lower Urungach lake](https://commons.wikimedia.org/wiki/File:%D0%92%D0%B8%D0%B4_%D0%9D%D0%B8%D0%B6%D0%BD%D0%B8%D0%B9_%D0%A3%D1%80%D1%83%D0%BD%D0%B3%D0%B0%D1%87.jpg), Arina Pan, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Sunrise over Langtang](https://commons.wikimedia.org/wiki/File:Paints_of_sunrise_on_Langtang_National_Park.jpg), Q-lieb-in, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Bryce Canyon, Inspiration Point](https://commons.wikimedia.org/wiki/File:Inspiration_Point_Bryce_Canyon_November_2018_panorama.jpg), King of Hearts, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Lake Tahoe in the morning](https://commons.wikimedia.org/wiki/File:Lake_Tahoe_at_morning_-_7-2023.jpg), John D., [CC BY 2.0](https://creativecommons.org/licenses/by/2.0/)
- [Buachaille Etive Mòr, Glen Coe](https://commons.wikimedia.org/wiki/File:Lagangarbh_cottage_with_Buachaille_Etive_M%C3%B2r.jpg), Colin, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Salar de Uyuni at sunrise](https://commons.wikimedia.org/wiki/File:Salar_de_Uyuni,_Bolivia,_2016-02-04,_DD_10-12_HDR.JPG), Diego Delso, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Upper Yosemite Falls](https://commons.wikimedia.org/wiki/File:Yosemite_Falls_from_trail,_Yosemite_NP,_CA,_US_-_Diliff.jpg), Diliff, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Kakerdaja bog in winter](https://commons.wikimedia.org/wiki/File:Kakerdaja_raba_talvine_maastik.jpg), Abrget47j, [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Nosy Iranja, Madagascar](https://commons.wikimedia.org/wiki/File:Nosy_Iranja,_Madagascar,_2025-09-17,_DD_21.jpg), Diego Delso, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Driftwood on Les Aresquiers beach](https://commons.wikimedia.org/wiki/File:Tronc_d%27arbre_aux_Aresquiers.jpg), Christian Ferrer, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- [After sunset, Les Aresquiers](https://commons.wikimedia.org/wiki/File:Sunset_on_the_Aresquiers_beach.jpg), Christian Ferrer, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- [Raftsundet, Lofoten](https://commons.wikimedia.org/wiki/File:Coastline_of_Raftsundet_at_Digermulen_in_V%C3%A5gan,_Nordland,_Norway,_2022_June.jpg), Ximonic (Simo Räsänen), [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Étang de Thau](https://commons.wikimedia.org/wiki/File:%C3%89tang_de_Thau_and_S%C3%A8te.jpg), Christian Ferrer, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- [Morning mist over a bog lake](https://commons.wikimedia.org/wiki/File:Udu_lahkub_j%C3%A4rvelt.jpg), Kylli28, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Strymba in winter, Carpathians](https://commons.wikimedia.org/wiki/File:%D0%93%D0%BE%D1%80%D0%B0_%D0%A1%D1%82%D1%80%D0%B8%D0%BC%D0%B1%D0%B0_%D0%B2%D0%B7%D0%B8%D0%BC%D0%BA%D1%83.jpg), Ryzhkov Sergey, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Geirangerfjord](https://commons.wikimedia.org/wiki/File:Geirangerfjord_from_%C3%98rnesvingen,_2013_June.jpg), Ximonic (Simo Räsänen), [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
- [Birks of Aberfeldy in autumn](https://commons.wikimedia.org/wiki/File:Birks_of_Aberfeldy_%2855085667062%29.jpg), Eric Kilby, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Waimea Canyon, Kauaʻi](https://commons.wikimedia.org/wiki/File:Waimea_Canyon,_Kaua%CA%BBi.jpg), Frank Schulenburg, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Monument Valley, Route 163](https://commons.wikimedia.org/wiki/File:Forrest_Gump_Point_Monument_Valley_November_2018_001.jpg), King of Hearts, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Coast of Santa Catarina](https://commons.wikimedia.org/wiki/File:%C3%81rea_de_Prote%C3%A7%C3%A3o_Ambiental_da_Baleia_Franca_Claudio_Seelig_%2801%29.jpg), Claudio Seelig, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Evening sun in Fair Haven, Svalbard](https://commons.wikimedia.org/wiki/File:Evening_sun_in_Fair_Haven,_Svalbard.jpg), AWeith, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Windswept tree, Guadeloupe](https://commons.wikimedia.org/wiki/File:Tabebuia_heterophylla_near_Plage_de_l%27Anse_du_Petit_Fort_%282%29.jpg), Tournasol7, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- [First light on a winter path](https://commons.wikimedia.org/wiki/File:Eerste_zonnestralen_strijken_over_een_winters_landschap._Pad_tussen_Put_van_Nederhorst_en_Langweerderwielen._Locatie,_Langweerderwielen_%28Langwarder_Wielen%29_en_omgeving_01.jpg), Dominicus Johannes Bergsma, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Pride of Madeira](https://commons.wikimedia.org/wiki/File:Echium_candicans_at_Montado_do_Pared%C3%A3o_in_Curral_das_Freiras,_C%C3%A2mara_de_Lobos,_Madeira,_2023_May.jpg), Ximonic (Simo Räsänen), [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Pine forest in Yyteri](https://commons.wikimedia.org/wiki/File:Spring_forest_Yyteri_3.jpg), Kallerna, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Forest road in autumn, Dülmen](https://commons.wikimedia.org/wiki/File:D%C3%BClmen,_B%C3%B6rnste,_Waldweg_--_2015_--_4649.jpg), Dietmar Rabich, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Snowy forest road, Slavne](https://commons.wikimedia.org/wiki/File:Forest_road_Slavne_2017_BW_G9.jpg), George Chernilevsky, public domain

### Space

- [Spiral galaxy M81](https://commons.wikimedia.org/wiki/File:Messier_81_HST.jpg), NASA, ESA, Hubble Heritage Team, public domain
- [Triangulum Galaxy](https://commons.wikimedia.org/wiki/File:M33-Hamois-30-11-2024-Luc-Viatour.jpg), Luc Viatour, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Sombrero Galaxy](https://commons.wikimedia.org/wiki/File:M104_ngc4594_sombrero_galaxy_hi-res.jpg), NASA, ESA, Hubble Heritage Team, public domain
- [Southern Pinwheel Galaxy](https://commons.wikimedia.org/wiki/File:Messier83_-_Heic1403a.jpg), NASA, ESA, Hubble Heritage Team, public domain
- [Saturn from Cassini](https://commons.wikimedia.org/wiki/File:Saturn_from_Cassini_Orbiter_%282004-10-06%29.jpg), NASA, JPL, Space Science Institute, public domain
- [Andromeda Galaxy](https://commons.wikimedia.org/wiki/File:Galaxie_d%27Androm%C3%A8de_M31.jpg), Luc Viatour, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Northern lights over Maardu](https://commons.wikimedia.org/wiki/File:Maardu_Northern_lights_%E2%80%94_Estonia,_2022.jpg), Maxim Bilovitskiy, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- [Northern lights over Mývatn](https://commons.wikimedia.org/wiki/File:039_Northern_lights_over_M%C3%BDvatn_%28Iceland%29_Photo_by_Giles_Laurent.jpg), Giles Laurent, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Barred spiral galaxy NGC 1300](https://commons.wikimedia.org/wiki/File:Hubble2005-01-barred-spiral-galaxy-NGC1300.jpg), NASA, ESA, Hubble Heritage Team, public domain
- [Endeavour over Earth's limb](https://commons.wikimedia.org/wiki/File:Endeavour_silhouette_STS-130.jpg), NASA, public domain
- [A farewell to Saturn](https://commons.wikimedia.org/wiki/File:PIA17218_%E2%80%93_A_Farewell_to_Saturn,_Brightened_Version.jpg), NASA, JPL-Caltech, Space Science Institute, public domain
- [Aurora and Perseids, Norderney](https://commons.wikimedia.org/wiki/File:Aurora_and_perseids.jpg), Stephan Sprinz, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- [Star trails at La Hague](https://commons.wikimedia.org/wiki/File:2017-04_Circumpolar_trails_sunset_at_La_Hague_lighthouse.jpg), Antoine Lamielle, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Gas and dust in the Large Magellanic Cloud](https://commons.wikimedia.org/wiki/File:Hubble_Peers_into_the_Storm_%2829563971405%29.jpg), NASA, ESA, [CC BY 2.0](https://creativecommons.org/licenses/by/2.0/)
- [Milky Way over Pedra Azul](https://commons.wikimedia.org/wiki/File:Pedra_Azul_Milky_Way.jpg), Eduardo M. S. Neves, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Hunga Tonga plume from orbit](https://commons.wikimedia.org/wiki/File:ISS-66_Atmospheric_plume_from_2022_Hunga_Tonga_eruption_-_edited.jpg), NASA, Kayla Barron, public domain
- [Earthset over the Moon](https://commons.wikimedia.org/wiki/File:A_New_View_of_the_Moon.jpg), NASA, public domain
- [Total solar eclipse, 2026](https://commons.wikimedia.org/wiki/File:Eclipse-totale-08-16-2026-Espagne-prutuberances-Luc-Viatour.jpg), Luc Viatour, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Aurora over Lofoten](https://commons.wikimedia.org/wiki/File:Lofoten,_Norway_%28Unsplash%29.jpg), Johannes Groll, [CC0](https://creativecommons.org/publicdomain/zero/1.0/)
- [Clouds over the Philippine Sea](https://commons.wikimedia.org/wiki/File:Cloudscape_Over_the_Philippine_Sea.jpg), NASA, public domain
- [Hurricane Florence from orbit](https://commons.wikimedia.org/wiki/File:Dramatic_Views_of_Hurricane_Florence_from_the_International_Space_Station_From_9_12_%2842828603210%29_%28cropped%29.jpg), NASA, [CC BY 2.0](https://creativecommons.org/licenses/by/2.0/)
- [Milky Way over Cerro Paranal](https://commons.wikimedia.org/wiki/File:Guisard_-_Milky_Way.jpg), ESO, Stéphane Guisard, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- [Milky Way over the Estonian coast](https://commons.wikimedia.org/wiki/File:Linnutee_-_Milky_Way_%289%29_copy.jpg), Kristian Pikner, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- [Spiral galaxy NGC 2683](https://commons.wikimedia.org/wiki/File:NGC_2683_Spiral_galaxy.jpg), NASA, ESA, public domain
- [Aurora over Alaska](https://commons.wikimedia.org/wiki/File:Aurora_borealis_over_Eielson_Air_Force_Base,_Alaska.jpg), U.S. Air Force, Joshua Strang, public domain
- [Galaxies NGC 2207 and IC 2163](https://commons.wikimedia.org/wiki/File:NGC2207%2BIC2163.jpg), NASA, ESA, Hubble Heritage Team, public domain
