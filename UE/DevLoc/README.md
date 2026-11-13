# DevLoc — dev-only Russian UI

`ru.tsv` is the Russian table read by `TSCLoc` (`Source/TheSagaCutter/TSCLoc.cpp`) at runtime. It is for developers only
and is **never shipped**: keep this folder outside `Content/` so cook/stage never picks it up
(`Scripts/package_jam.sh` fails the build if any of it reaches `Packaged/`). Without this file the feature is inert.

- Switch: the **RU/EN** button on the title screen, **F10** in game, or the console command `TSCLang ru` (`TSCLang en`).
- Find gaps: run `TSCLocMisses`; untranslated strings are written to `UE/Saved/DevLoc_missing.txt`.
- Format: UTF-8 with BOM, LF, one `English<TAB>Russian` per line. Any line **without a TAB** is a comment
  (`#` is not a comment marker: number keys start with it). Spaces at either end of key and value are kept.
- Keys are the exact English string as drawn (case-sensitive). Strings whose numbers vary use `#` for each digit run,
  and the value refers to them as `{0}`, `{1}`… (e.g. `# CUTS LEFT<TAB>ОСТАЛОСЬ СКЛЕЕК: {0}`). Exact keys are tried
  first, so strings with fixed digits (`START DAY 1`) are written literally. `{0}` keys are the `TSCLoc::Fmt` patterns.
