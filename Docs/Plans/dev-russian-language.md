# Dev-only Russian language with runtime switch

On approval this file is copied to `Docs/Plans/dev-russian-language.md` (the fable-plan contract) and the steps are
handed to implementers: `routine` → Sonnet, `tricky` → Opus, then a Fable review.

Goal: Russian-speaking developers can switch the whole game UI to Russian at runtime (title-screen button, F10, or
console), and the jam submission build contains no Russian data and shows no language switch — without anyone
having to remember to remove it.
Out of scope: the UE localization pipeline (NSLOCTEXT / gather / .po / .locres), UMG, new fonts, layout redesign,
translating log messages, any language other than Russian, changing English wording.

## Context

- The UE project is in `UE/` (not repo root). One C++ module: `UE/Source/TheSagaCutter/`.
- **All** player text (~190 strings) is hardcoded English `TEXT("...")` in two files: `TSCHUD.cpp` (~51) and
  `TSCGameMode.cpp` (~130). No FText, no localization setup, no widgets, no data files.
- **Every** on-screen string passes through one choke point: `ATSCHUD::Txt(const FString&, ...)`
  (`TSCHUD.cpp:69`), and `ATSCHUD::Wrapped(...)` (`TSCHUD.cpp:85`) which splits on spaces and calls `Txt` per line.
  `Button(...)` labels also end in `Txt`.
- Font is engine Roboto (`MakeFont`, `TSCHUD.cpp:30`), which has Cyrillic — no font work expected.
- Jam rules require a packaged **Development** build, so `UE_BUILD_SHIPPING` / `DisabledCultures="ru;Shipping"`
  cannot be the strip mechanism. No packaging script exists yet; nothing has ever been packaged.

### Design (why this shape)

1. **Translate at draw time, keyed by the English string.** `Txt`/`Wrapped` look the string up in a table and draw
   the Russian value; a miss draws English. Almost no call sites change, so the jam code path is untouched, and
   switching language mid-game also re-translates stored strings (toasts, banners, scenes, epilogue).
2. **Numbers are handled by normalization, not by call-site changes.** Before lookup, each run of ASCII digits in
   the string is replaced by `#` and remembered; the Russian value refers to them as `{0}`, `{1}`… So
   `FString::Printf(TEXT("%d CUTS LEFT"), n)` stays as is and matches the key `# CUTS LEFT`.
3. **Only the few `%s` / concatenation sites get explicit `Tr()` calls** (6 places, listed in S3).
4. **Automatic removal = the data never ships.** The table is a plain file `UE/DevLoc/ru.tsv`, outside `Content/`,
   so cook/stage never picks it up. No Cyrillic exists in C++ source. If the file is absent the feature is inert:
   no button, F10 and the console command do nothing. `Scripts/package_jam.sh` builds the submission and **fails**
   if anything Russian is found in the output.

## Steps

### S1 — Add the `TSCLoc` translation table ✅   [routine]   deps: none
Files: `UE/Source/TheSagaCutter/TSCLoc.h` (new), `UE/Source/TheSagaCutter/TSCLoc.cpp` (new)
Do: plain C++ namespace, no UObject.
```cpp
namespace TSCLoc
{
	bool IsAvailable();                 // table file was found and has >0 entries (lazy-loads on first call)
	bool IsRussian();                   // current language
	void SetRussian(bool bOn);          // no-op if !IsAvailable(); persists the choice
	void Toggle();
	FString Tr(const FString& En);      // returns En unchanged when !IsRussian() or on a miss
	FString Fmt(const TCHAR* EnPattern, const FStringFormatOrderedArguments& Args); // Tr(pattern) then FString::Format
	void DumpMisses();                  // writes <ProjectSavedDir>/DevLoc_missing.txt, logs the path to LogSaga
}
```
- File: `FPaths::Combine(FPaths::ProjectDir(), TEXT("DevLoc/ru.tsv"))`. Load with `FFileHelper::LoadFileToStringArray`.
  Line format `English<TAB>Russian`; skip empty lines and lines starting with `#`; split on the **first** tab only.
  Keys must compare **case-sensitively** (`"CUT!"` vs `"Cut!"`), and the default `TMap<FString,...>` key compare is
  case-insensitive. Use a custom KeyFuncs (`ESearchCase::CaseSensitive` equality, `FCrc::StrCrc32` hash) for both the
  table and the miss set, or an equivalent that is truly case-sensitive.
- `Tr`: if not Russian or `En` empty → return `En`. (1) exact lookup. (2) normalized lookup: replace each maximal
  run of `0-9` with `#`, collecting the runs in order; if the normalized string differs from `En` and is found,
  return `FString::Format(*Value, Args)` with the runs as ordered args. (3) miss → add `En` (normalized form if it
  had digits) to a `TSet<FString>` of misses, return `En`. Do not record strings with no letters (`FChar::IsAlpha`).
- `Fmt`: `FString::Format(*Tr(EnPattern), Args)` — in English this is just `FString::Format(EnPattern, Args)`.
- Persist: `GConfig->SetBool/GetBool(TEXT("TSC"), TEXT("bRussian"), ..., GGameUserSettingsIni)` + `GConfig->Flush`.
  Read once at lazy-load; if the file is missing the stored value is ignored.
- No `#if` guards, no Build.cs changes (`Core` is enough).
Accept: `bash Scripts/build_editor.sh` prints a successful Result with no errors.

### S2 — Route HUD text through `TSCLoc` and add the switch ✅   [routine]   deps: S1
Files: `UE/Source/TheSagaCutter/TSCHUD.h`, `UE/Source/TheSagaCutter/TSCHUD.cpp`,
`UE/Source/TheSagaCutter/TSCPlayerController.cpp`
Do:
- Rename the body of `ATSCHUD::Txt` to a new private `TxtRaw(...)` (same signature). New `Txt` =
  `TxtRaw(TSCLoc::Tr(S), ...)`. `Measure` is unchanged (it is called inside `TxtRaw` on the already-translated string).
- `Wrapped`: first line `const FString T = TSCLoc::Tr(S);`, wrap `T`, and call `TxtRaw` for each line (not `Txt`).
- Anywhere the HUD calls `Measure(...)` on a literal/label to size or position something (e.g. inside `Button`,
  tooltip width), wrap the argument in `TSCLoc::Tr(...)` so layout follows the drawn text. Do not otherwise change layout.
- `TSCHUD.cpp:685`: `Txt(TEXT("The world is frozen.  ") + S.Title, ...)` →
  `TxtRaw(TSCLoc::Tr(TEXT("The world is frozen.  ")) + TSCLoc::Tr(S.Title), ...)`.
- Language button: add `IdLang` to the enum at `TSCHUD.cpp:38` (value 8). In `DrawTitle` (line ~739), only
  `if (TSCLoc::IsAvailable())`, draw `Button(IdLang, SW - 48.f - 290.f - 16.f - 90.f, SH - Bar + 20.f, 90.f, 56.f, TSCLoc::IsRussian() ? TEXT("EN") : TEXT("RU"), true, 22.f)`
  (left of START DAY 1). In `Act(Id)` handle `IdLang` → `TSCLoc::Toggle()`. The label is ASCII; no table entry.
- `TSCPlayerController::SetupInputComponent` (next to the existing `BindKey` calls ~line 112): bind `EKeys::F10`,
  `IE_Pressed`, to a new `OnLangKey()` that calls `TSCLoc::Toggle()`.
Accept: builds; with no `UE/DevLoc/ru.tsv` present the title screen is pixel-identical to before (no RU button).

### S3 — Translate the composed strings in the game mode; add console commands ✅   [routine]   deps: S1
Files: `UE/Source/TheSagaCutter/TSCGameMode.h`, `UE/Source/TheSagaCutter/TSCGameMode.cpp`
Do: only these sites; English output must stay byte-identical when Russian is off.
- `:1421` → `Toast(TSCLoc::Fmt(TEXT("Your people begin raising {0}, tile by tile."), { TSCLoc::Tr(Names[(int32)Type]) }));`
- `:2000` → `TSCLoc::Fmt(TEXT("{0} has fallen."), { TSCLoc::Tr(U->DisplayName) })`
- `:1949` (`"%d wood braces the gate (%d strength).%s%s"`): keep the `Printf` for the numeric head with the two
  `%s` removed, then append `TSCLoc::Tr(<each suffix literal>)`; i.e. `TSCLoc::Tr(Head) + TSCLoc::Tr(SuffixA) + TSCLoc::Tr(SuffixB)`.
- `:2173`–`:2180`: `Tr()` each building noun phrase and the `" and "` joiner; `Why` clauses each via `Tr()`;
  final line `TSCLoc::Fmt(TEXT("On the empty ground you raised {0}.{1}"), { B, Why })`.
- `:2218`–`:2222`: `Tr()` each surviving name and keep `", "`; replace the `Printf` with
  `TSCLoc::Fmt(TEXT(" Of the four you knew by name, {0} still sit by the fire."), { Living })`; `Tr()` the "none are left" literal and `Became`.
- New `UFUNCTION(Exec)` next to `TSCAuto` (`TSCGameMode.h:206`): `void TSCLang(const FString& Lang);`
  (`"ru"` → `SetRussian(true)`, anything else → false) and `void TSCLocMisses();` → `TSCLoc::DumpMisses()`.
- Strings composed here are stored already translated, so they keep the language active when they were created
  (a later switch does not re-translate them). This is accepted.
Accept: builds; a `-game` run in English shows the same toasts/epilogue text as before.

### S4 — Write the Russian table ✅   [tricky]   deps: S2, S3
Files: `UE/DevLoc/ru.tsv` (new), `UE/DevLoc/README.md` (new, ~10 lines)
Do: read every `TEXT("...")` that reaches the screen in `TSCHUD.cpp` and `TSCGameMode.cpp` (after S2/S3) and add
one line per string: exact English key, TAB, Russian. Save as **UTF-8 with BOM**, LF line endings.
- Keys for `Printf("%d …")` strings use `#` per number and `{0}`,`{1}` in the value, e.g.
  `# CUTS LEFT<TAB>ОСТАЛОСЬ ДУБЛЕЙ: {0}`. Phrase Russian so no plural agreement is needed (use "N: …" / "… — N" forms).
- Keys for `Fmt` patterns are the `{0}` patterns from S3 verbatim.
- Include: 5 building names + 5 tooltips, the building noun phrases (`a house` …), 4 prologue captions, phase
  labels, all banners and toasts, 24 scene strings, all epilogue lines and 4 saga titles, the 4 full villager names
  **and** their first words (`Hakon` → `Хакон`, the HUD shows only the first word), `" and "`.
- Tone: saga / skald voice, same register as the English. Keep UPPERCASE where the English is uppercase.
  Keep strings close to English length; the HUD has fixed widths (job panel 330, tooltip 520).
- `README.md`: what the folder is, that it is dev-only and must never move under `Content/`, how to switch
  (RU button on title, F10, `TSCLang ru`), how to find gaps (`TSCLocMisses`).
Accept: start the game (`UnrealEditor.exe UE/TheSagaCutter.uproject -game -windowed -ExecCmds="TSCLang ru"`), play
through with `TSCAuto`, run `TSCLocMisses`; `UE/Saved/DevLoc_missing.txt` is empty or lists only strings
deliberately left English. Screenshot of title, HUD, CUT! screen and epilogue show Cyrillic, no tofu boxes, no
text running outside its panel.

### S5 — Jam packaging script with a Russian gate ✅   [routine]   deps: none
Files: `Scripts/package_jam.sh` (new), `.gitignore`
Do: bash script, same style as `Scripts/build_editor.sh` (`UE_ROOT` default `C:\Programs\UE_5.9`). Usage:
`Scripts/package_jam.sh <TeamName>`.
1. `RunUAT.bat BuildCookRun -project=<repo>/UE/TheSagaCutter.uproject -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=<repo>/Packaged -nop4 -utf8output`
2. Gate — exit 1 with a clear message if any of these match under `Packaged/`: a path containing `DevLoc`; any
   `*.tsv`; any directory named `ru` or `ru-*` under a `Localization` directory; any loose text file
   (`*.ini *.txt *.json *.csv`) containing bytes in the Cyrillic UTF-8 range (`grep -rlP '[\xD0-\xD3][\x80-\xBF]'`).
3. Zip `Packaged/Windows` to `Packaged/<TeamName>_TheSagaCutter.zip` (PowerShell `Compress-Archive`), print size,
   warn if ≥ 1 GB.
- `.gitignore`: add `Packaged/`.
Accept: `bash -n Scripts/package_jam.sh` passes; gate logic tested by pointing it at a temp dir that contains a
`DevLoc/ru.tsv` (must fail) and one that does not (must pass). The full package run is the Final check.

Parallel groups: wave 1 = S1 + S5; wave 2 = S2 + S3; wave 3 = S4.

## Final check
1. `bash Scripts/build_editor.sh` — clean.
2. English regression: run `-game` with `UE/DevLoc` temporarily renamed → no RU button, F10 does nothing, text as before.
3. Russian: S4's Accept run (screenshots + empty misses file); toggle with F10 mid-day and confirm HUD flips both ways.
4. `bash Scripts/package_jam.sh TeamName` — package succeeds, gate passes, the packaged exe launches in English
   with no RU button. (Long: ~20–40 min; first package of this project, so cook errors unrelated to this work may surface.)

## Risks / open questions
- **Team name for the zip** is unknown → it is a script argument; nothing is hardcoded.
- **First-ever package** may hit unrelated cook/plugin problems (many editor-only toolset plugins are enabled in
  the .uproject). Default: report them, do not fix inside this plan.
- **UTF-8 loading**: if `LoadFileToStringArray` mis-decodes, default is to keep the BOM and, if still wrong, load
  bytes and convert with `FUTF8ToTCHAR`.
- **Overflowing Russian text** in fixed-width panels. Default: shorten the Russian wording; do not touch layout.
- **Russian devs who want it in a packaged build**: copy `UE/DevLoc/` next to the packaged `TheSagaCutter/`
  folder by hand. Not automated on purpose.
- A compile-time strip of the ~100 lines of `TSCLoc` code was considered and left out: the jam build must be
  Development, and an env-var define in Build.cs risks stale UBT state two days before the deadline. The code is
  inert without the data file.
