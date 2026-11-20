MFG FIX NG LE - 1.0.13 LE TEST BUILD 5
======================================

Legendary Edition adaptation by Mhacks79.
Original Mfg Fix NG by KrisV777. Original Mfg Fix by andrelo.

REQUIREMENTS
Skyrim Legendary Edition 1.9.32.0.8 and SKSE 1.7.3 (32-bit).
No Address Library, ESP, SkyUI, FNIS or Nemesis is required.

INSTALLATION
Install the ZIP as a normal mod in Mod Organizer 2 or your mod manager.
The archive root is the Data directory: SKSE/Plugins and Scripts.
This plugin replaces Mfg Console (MfgConsole.dll). To use it that way:
- keep Mfg Console's Interface\console.swf and console_alias.ini installed (they also
  come with SexLab 1.62): that is the console window, this plugin feeds it;
- hide SKSE\Plugins\MfgConsole.dll in MO2 (right click > Hide, in the Mfg Console mod
  and in SexLab if it carries one), or untick the Mfg Console mod if it only had the DLL;
- place this mod BELOW Mfg Console and SexLab in MO2's left pane, so its
  MfgConsoleFunc.pex wins the file conflict.
Then this plugin provides everything Mfg Console did (see MFG CONSOLE REPLACEMENT).
If MfgConsole.dll is still loaded, it keeps its console job exactly as before and this
plugin only provides the Papyrus functions, as in test build 4.
Do not install mfgfix.dll (the old LE Mfg Fix) next to this one. Do not install the
supplied Special Edition DLL on Legendary Edition.
Start the game with skse_loader.exe through your usual mod manager.

MFG CONSOLE REPLACEMENT
Without MfgConsole.dll, this plugin answers console.swf the same way it did:
- the info window when you click a reference: name, type, form and base IDs, plugin,
  defining plugin, and the plugins that overwrite it (last one wins);
- the phoneme / modifier / expression table of a selected actor, live;
- the extra info window (enableextrainfo, on by default): base, race, level, spells,
  shouts, active effects, actor values and equipment of an NPC, details of armour,
  weapons, books, potions, spells, magic effects, shouts, form lists and more;
- mfg phoneme / modifier / reset applying at once (and mp / mm / me aliases), with the
  same fixes Mfg Console made to the vanilla LE command;
- playerblink start / stop, getmoditem mod|vanilla|dlc, savebat, and its messages.
Two things are done better than the old DLL: the "overwritten by" list maps each
plugin's masters properly, and getmoditem no longer skips forearm armour.
Settings are in the [MfgConsole] section of the INI.

WHAT THIS LE BUILD PROVIDES
- MfgConsoleFunc: SetPhonemeModifier and GetPhonemeModifier plus original wrappers.
- MfgConsoleFuncExt: smooth phonemes, modifiers and expression changes; smooth
  resets; 32-float expression presets; dialogue status and player speech target.
- Morph strengths from 0 to 200 percent, including expression ID 0.
- Main-thread command dispatch, validated face-data links, engine face locking,
  save/load request cancellation and transition cleanup on face destruction.
- Existing console mfg commands plus the smoke-test commands below.

LE-SPECIFIC BEHAVIOR AND LIMITS
This is an LE adaptation of the expression API, not a byte-for-byte port of the
Special Edition engine replacement. LE's original dialogue, blinking, eye tracking
and frozen-actor handling remain in charge. The SE EyesBlinking, EyesMovement,
Dialogue and MFGFIX_Settings overrides/patches are not implemented by this build;
their upstream INI is not installed. Only the Transition setting below is supported.
This does not remove the IsInDialogue or GetPlayerSpeechTarget Papyrus functions;
both are supplied. Conditional Expressions Extended 2.3.0's MFG calls match this
API, but its SE ESP needs conversion and LE dependencies/runtime behavior require
separate validation. Its bundled fTrackEyeZ eye tuning is not applied here.
Presets interpolate source morphs before the native LE merge; timing/composition
can differ from NG's interpolation of the final SE morphs.
Speed is transition time per unit strength: smaller is faster, zero is immediate.
Up to 256 simultaneous face transitions are tracked, with no allocation per tick.
Papyrus setters return whether a request was queued; an unloaded face or capacity
limit may prevent it from being applied later. Existing saves can be used; this
plugin writes no cosave records. Compatibility with individual expression mods
still needs in-game testing on LE; SE plugins do not become LE-compatible by this API.

CONFIGURATION
SKSE/Plugins/MfgFixNGLE.ini:
[Transition]
fDefaultSpeed=0.00
[Console]
bPassToMfgConsole=1
[MfgConsole]
bReplace=1
bPlayerBlink=1
bExtraInfo=1

Default 0 preserves immediate legacy calls and ordinary console changes.
Set 0.75 to smooth those calls. New Ext calls supply their own speed. The ng test
commands below always use 1.5 seconds. Values are clamped to 0..10; decimal dot
and comma are accepted. Restart Skyrim after changing the INI.

CHANGES IN TEST BUILD 5 (1.0.13)
Mfg Console replacement: with MfgConsole.dll hidden, this plugin now drives
console.swf itself (info table, expression table, extra info window, player blink,
getmoditem, savebat) and fixes the mfg console command the way Mfg Console did, so one
DLL does the whole job. Nothing changes for Papyrus scripts.

CHANGES IN TEST BUILD 4 (1.0.12)
Test build 3 asked to disable Mfg Console, which removed its console table and extra
info window. Mfg Console is now supported next to this plugin: its console commands
are handed back to it when it is loaded (bPassToMfgConsole=1), so mp / me / mm edit the
face at once and show in its table again. Papyrus calls are unchanged.

CHANGES FROM TEST BUILD 1
Manual expressions now use the explicit LE expression source rather than the
engine's transition-dependent primary source. Reset requests force final morph
caches to rebuild, including when an immediate reset precedes a zero-delta update.
mfg status and mfg statut are aliases of mfg ngstatus. The log records console
requests, applied actor/source details and unavailable actors or face arrays.
The first test build's return-only engine stub missed this native merge behavior.

QUICK IN-GAME TEST
1. Load a save, switch to third person and enter tfc (without 1). Move the free
   camera in front of the player. Keep the game running when closing the console.
2. Enter prid 14 to select the player, then mfg ngstatus. It should report ready.
3. Enter mfg ngsmile, close the console and watch the face for about two seconds.
4. Repeat with mfg nganger and mfg ngmoodzero. Each should transition gradually.
5. Enter mfg ngreset, close the console and allow the expression to reset.
6. Separately try mfg ngmouth and mfg ngbrows, resetting after each test.
7. Repeat on a living NPC by clicking the NPC in the console, then using the same
   commands. Finish with mfg ngreset before continuing normal play.
8. Test a conversation, natural blinking and lip sync after resetting. Also load
   another save while a transition is pending; no old request should carry over.
9. Enter tfc again to leave the free camera.

Console pauses the game: close it to see queued commands and transitions run.
Do not use tfc 1 or other pause/time-freeze commands for this test.
CallGlobalFunction/cgf is not in vanilla LE's command tables and is not used here.

LOGS AND CONFLICTS
Nothing is logged by default, not even an empty file. Set EnableLog=1 under [Debug]
in SKSE/Plugins/MfgFixNGLE.ini and start the game again: then
Documents/My Games/Skyrim/SKSE/MfgFixNGLE.log records startup, callback thread and
faults. The usual skse.log reports whether the DLL was accepted by SKSE.
This build refuses unknown modifications to its two hook prologues rather than
copying an unsafe trampoline. A conflict is logged by name/address. Other allowed
signature differences are logged; changing data globals are checked for readability.
After an internal fault it disables its own writes and keeps the native engine
running. It never takes player controls. Report the log if it is disabled or fails
to load; a clean offline test does not establish in-game compatibility.

VALIDATION STATUS
Built and checked as x86 with the two LE SKSE exports and static CRT.
Native code tested against a fake LE engine with final-array merge behavior
modeled from TESV decompilation; Papyrus interfaces compiled by the LE compiler.
The model covers source-to-final copying and zero-reset caches, not the full engine.
Test build 1 loaded in game but the user reported failed expressions/resets.
Test build 2 rendering, real lip sync, eye behavior and other mods still require
in-game confirmation. See VALIDATION.txt for results.

SOURCE AND LICENSE
Complete port sources, harness and build inputs are under Source.
COPYING and EXCEPTIONS.md are the upstream GPLv3 license and linking permissions.
Original source notices and SKSE/common licenses are retained with those sources.
