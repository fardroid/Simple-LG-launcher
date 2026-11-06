# Clock fonts

The clock's typefaces other than the TV's own (lib/clockfont.ts of the web version), as static TTFs: `<family>-<weight>.ttf`.
All are under the SIL Open Font License (`OFL-*.txt`), from github.com/google/fonts:

- Inter, Oswald, Roboto Mono, Cormorant Garamond: static instances made from the variable fonts with fontTools'
  `varLib.instancer` (Inter at optical size 32, as a browser picks for large text);
- Poppins: the static files as published.

Each is cut down to Latin, Cyrillic and Greek with punctuation (`pyftsubset`); other characters come from the TV's fonts.
