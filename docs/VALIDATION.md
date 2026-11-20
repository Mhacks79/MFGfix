# Test Build 5 — Validation status

**Source:** `VALIDATION.txt` supplied in `MfgFixNGLE-1.0.13-LE-test5.zip` (2026-11-20).

## Latest build: v1.0.13-test5

- Addition: **integrated Mfg Console replacement**.
- Implemented replacement of `GetObjectData`, `GetObjectExtraData`, `EnableExtraInfo`, `PlayerBlinkSet`, `GetModItems`, `SaveBatFile`, `ConsolePrint`, plus three legacy `ModifyFaceGen` call-site fixes, conditional on still matching the vanilla LE calls.
- Replacement is active only when the original **MfgConsole.dll is absent** and `bReplace=1`.
- **Offline harness:** 2,217 checks, 0 failures (104 new).
- **Mutation tests:** 21/21 replacement mutants detected; 7/7 prior engine mutants detected.
- **Loader probe:** default logging produces no log file; enabling logging creates a log.
- **Preflight:** zero fatal findings.
- **Limitation:** the replacement was **not run in game** for Test Build 5.

## Earlier builds

- **v1.0.12-test4** (Nov 19): Original Mfg Console DLL coexistence; ordinary `mfg` commands handed to original DLL if present. 2,113 offline checks, 0 failures. Not tested in game as this build.
- **v1.0.11-test3** (Nov 6): Logging disabled by default; 2,099 offline checks, 0 failures.
- **v1.0.10-test2** (Oct 12): Corrects LE expression source and reset cache handling; 2,099 offline checks. Creator confirmed basic MFG and converted CEE functioning in-game Oct 12–13.

**Important:** fake-engine/harness results are not proofs of real Skyrim rendering, lip sync, compatibility with every NPC mod, or all-console functionality. These must be tested in game.

## Logs

Set `[Debug] EnableLog=1` in `SKSE/Plugins/MfgFixNGLE.ini`, restart Skyrim, then see:

```text
Documents/My Games/Skyrim/SKSE/MfgFixNGLE.log
```

Also inspect `skse.log` for plugin loading errors. Report any hook signature conflict or disabled plugin state with your game/mod list.

The full machine-generated and research evidence is supplied with the distribution archive.