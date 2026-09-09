# IV-SDK (mingw copy)

Vendored copy of [Zolika1351/iv-sdk](https://github.com/Zolika1351/iv-sdk)
`include/` (GPLv3, see LICENSE), header-only, included as one translation
unit via `IVSDK.cpp`. Supports GTAIV.exe 1.0.7.0 and 1.0.8.0 only (EFIGS).

Upstream is written for MSVC; these files were changed so `i686-w64-mingw32-g++`
compiles it (everything else is untouched):

- `Hooks.h` — the nine `__declspec(naked)` / `__asm { }` trampolines are
  rewritten as GCC naked functions with AT&T asm. `Run()`, `returnAddress`,
  `callAddress` and `thisParam` get explicit assembler names so the basic-asm
  bodies can reference them. Original kept as `Hooks.h.upstream`.
- `CTheScripts.h` — `FindNativeAddress` used MSVC inline asm + SEH
  (`__try/__except`); now GCC extended asm (hash in `esi`, result in `eax`), no
  exception guard.
- `Scripting/NativeInvoke.h` — `_cdecl` → `__cdecl`.
- `Scripting/Types.h` — `DllExport` defined empty (it imported from Aru's
  ScriptHook.dll, which is not used here).
- `NewAddressSet.h` — `reinterpret_cast<T*>(nullptr)` → `static_cast`
  (g++ rejects reinterpret_cast from `nullptr_t`).

Link notes: `-lversion` (GetFileVersionInfo for game version detection) and
`-Wl,--exclude-all-symbols` (mingw's auto-export trips over the asm-labelled
hook symbols; an .asi exports nothing anyway).
