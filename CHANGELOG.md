# Changelog

## 1.0.13 — Test Build 5 (2026-11-20)
- Integrated LE-native replacement for **Mfg Console** when `MfgConsole.dll` is not loaded.
- Added live facial data, object info, extra info, plugin overwrite info, player blinking, `getmoditem`, and `savebat` integration.
- Implemented legacy face-command fixes conditionally; restored familiar console behavior.
- Improved override/master mapping and forearm armor enumeration.
- Offline suite: **2,217 passes, 0 failures**; **21/21 replacement mutations** detected.
- **New replacement still awaiting live in-game testing**.

## 1.0.12 — Test Build 4 (2026-11-19)
- Allow original `MfgConsole.dll` to coexist with NG LE and retain its console UI/commands.
- Add `[Console] bPassToMfgConsole=1`.
- 2,113 offline checks.

## 1.0.11 — Test Build 3 (2026-11-06)
- Disable diagnostic logging by default; create no log unless `[Debug] EnableLog=1`.
- 2,099 offline checks.

## 1.0.10 — Test Build 2 (2026-10-12)
- Fix LE explicit expression source selection and zero-strength reset handling.
- Improve request and update logging and console status aliases.
- 2,099 modeled engine checks, 7/7 regression mutations detected.
- Creator later confirmed functional basic MFG and CEE behavior in-game.

## Earlier test builds
- Initial native LE adaptation of Mfg Fix NG facial expressions, Papyrus APIs, transition handling, and console smoke tests.