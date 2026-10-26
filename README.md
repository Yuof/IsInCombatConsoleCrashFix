# IsInCombat Console Crash Fix

SKSE plugin for Skyrim Special Edition (SE 1.5.97 and Anniversary Edition runtimes; built with VR support, untested
there). Fixes a vanilla crash while a console command runs: the `IsInCombat` condition prints its result to the
console with its subject's name, and when the subject is a reference that is not an actor the name is read from a null
pointer (`SkyrimSE.exe+0CC90CF` on 1.6.1170, `BSReadWriteLock::LockForRead` under
`TESObjectREFR::GetDisplayFullName`). It happens with `IsInCombat` typed in the console on a non-actor, and when a
console command (`coc`, `cow`, a mod's ConsoleUtil call) runs quest conditions that test `IsInCombat` on non-actors.

The condition function sets its subject only for an actor; its "is in combat" message uses it only then, but its
"is not in combat" message uses it unchecked. The plugin wraps `IsInCombat`'s console handler and condition function
in the script command table: in console mode a reference that is not an actor gets 0, the game's own answer, and the
console handler prints its name; every other call goes to the game's functions unchanged.

The command is found by its name in the script command table, so no code offsets are used. If it is not found as
expected, the plugin stays inactive and says so in `IsInCombatConsoleCrashFix.log`.

## Requirements
- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## Building
Visual Studio 2022 (C++), CMake 3.24+, [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set.

```
git clone --recurse-submodules https://github.com/Yuof/IsInCombatConsoleCrashFix
cd IsInCombatConsoleCrashFix
cmake --preset release
cmake --build --preset release
```

The DLL is `build/Release/IsInCombatConsoleCrashFix.dll`.

## License
[GPL-3.0-or-later](LICENSE), as required by [CommonLibSSE NG](https://github.com/alandtse/CommonLibVR) (GPL-3.0-or-later
with its modding exceptions), which this plugin links statically.
