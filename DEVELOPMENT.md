# Development

Thanks for considering contributing to TM-CE!
I really appreciate all the help.
Melee is a pretty complicated game, but that doesn't make TM-CE hard to contribute to!
Here are a few things you should know before contributing.

- [Join the discord here](https://discord.gg/2Khb8CVP7A).
- Ping me in the dev-discussion channel before starting a contribution, I will guide you in the right direction.
- Keep contributions small and focused.

If you have any other questions, feel free to ping me (Aitch) in the dev-discussion channel.

## Compilation

### Windows
1. [Install DevKitPro](https://github.com/devkitPro/installer/releases/latest). Install the Gamecube (aka PPC or PowerPC) package.
2. Run the 'windows_setup.bat' file.
3. Run the command `./build.sh path-to-melee.iso` in the console.

### Linux / MacOS / WSL / MSYS2
1. [Install DevKitPro](https://devkitpro.org/wiki/Getting_Started#Unix-like_platforms). Install the Gamecube (gamecube-dev) package.
2. Install xdelta3. This should be simple to install through your package manager.
3. Run the command `./build.sh path-to-melee.iso` in the console.
    - If the provided binaries fail (possibly due to libc issues), you can compile your own binaries from my repos:
[gc_fst](https://github.com/AlexanderHarrison/gc_fst), [hmex](https://github.com/AlexanderHarrison/cdat), [hgecko](https://github.com/AlexanderHarrison/hgecko)

### NixOS
1. Make sure you have the [Flakes](https://nixos.wiki/wiki/Flakes) feature enabled.
2. Enter the development shell by running `nix develop` to load all the necessary dependencies.
3. Run the command `./build.sh path-to-melee.iso` in the console.

Alternatively, if you have [Direnv](https://nixos.wiki/wiki/Direnv) installed, you can run `direnv allow` to enter the development shell automatically when you enter this project directory.

### Build Mode
The build script takes an optional additional mode argument called the mode - `build.sh iso [mode]`.
This allows building an optimized release, or fine-grained recompilation.
Examples:
- `build.sh iso`: debug build from scratch. **You probably want to use this**.
- `build.sh iso release`: release build from scratch.
- `build.sh iso build/codes.gct`: only rebuild asm.
- `build.sh iso build/edgeguard.dat`: only rebuild edgeguard event. You can use any dat file here.

## Tyro Version and Save Identity

Edit **[version.h](version.h)** at the repository root to update `TM_VERSION`, for example `V1.4.1T2`. This one value supplies the in-game short/long labels, disc/banner titles, memory-card caption, ISO filename, release folder/ZIP, and packaged patchers. Keep the newest version's implemented changes in the [README changelog](README.md#tyro-branch-changelog).

`TM_GAME_ID` is Tyro's stable identity, **`TYRE01`**. Keep it unchanged between releases. Its `TYRE` game code differs from upstream's `GTME`, so Tyro uses its own native save namespace. Tyro starts with a fresh save by default; there is no automatic overwrite, retagging, or migration of the legacy upstream save. Existing `GTME` recordings also remain in their old namespace until an explicit compatible importer is implemented. Per-game Dolphin settings associated with the old ID may need deliberate copying.

Run builds from the repository root:

```sh
./build.sh --version
./build.sh melee.iso
./build.sh melee.iso release
```

Outputs use `TM-Tyro-${TM_VERSION}.iso` and, for releases, `TM-Tyro-${TM_VERSION}.zip`. Release contents are staged under `build/releases/` and include patchers with generated filename configuration and the Tyro symbol map. Windows packaging uses `zip` when installed, or built-in PowerShell compression otherwise. Generated banner metadata is written to `build/opening.bnr`; root `opening.bnr` remains the source artwork/credits.

Changing version/identity metadata forces a full rebuild even when a single-module build was requested. Assembly identity constants and the save caption are generated in `build/tyro-identity.s` before compilation; use the build script to generate this include before invoking `hgecko` manually. `clean.sh` removes build scratch and the current Tyro version's ISO/ZIP, preserving legacy `TM-CE.iso` and other versioned outputs.

## Saved Settings and Migration

`src/settings.c` is the portable byte codec; `src/settings_game.c` owns the shared runtime service. Event modules use `TM_GetSetting` / `TM_SetSetting` through `EventVars.settings`. Native hooks use the register-preserving `SettingsRead`, `SettingsWrite`, and `SettingsToggle` macros in `ASM/Globals.s`; macro register arguments are numeric register indices (or absolute numeric aliases), not `r3`-style register tokens. Do not write the saved fields directly.

The record stays at `0x1F24..0x1F4F`. It uses explicit big-endian mask bytes, control/actor nibbles, three-bit OSD palette slots, `TY` signature bytes at `0x1F4A`, and version 1 in the high two bits of `0x1F2E`. Four bytes at `0x1F4C..0x1F4F` remain reserved. `src/settings.h` defines the stable fields/IDs and counts; C/assembly ABI-header changes invalidate partial builds.

Validation runs lazily after data is available. Only the exact running Tyro game/maker ID may migrate the record. Missing-signature legacy Tyro lists are copied before repacking; disabled/invalid pairs are skipped and duplicate valid groups retain the last value. Existing enable bits and valid controls are preserved, enabled OSDs receive White, and new flags default Off. Changes mark the native save as dirty and use its existing serialization/checksum path. Future format versions must retain the `TY` signature. An unsupported version or foreign identity leaves all 44 persisted bytes untouched and uses mutable in-memory defaults instead.

Earlier unversioned `TYRE01` builds from step 0 cannot understand this packed record. Once migrated, avoid using those builds with the same Tyro save; the separate upstream/T1 `GTME01` namespace remains unaffected. Version-aware newer builds preserve unsupported settings, but this does not make arbitrary older game executables or recording formats compatible. No automatic upstream-save copy/import is implemented.

Run the actual PowerPC codec, runtime bridge and native-hook tests from the repository root:

```sh
python -m pip install --target build/test-deps unicorn==2.1.4
bash tests/run_settings_tests.sh
```

The tests compile a freestanding big-endian PowerPC image with devkitPro, then emulate it. They cover legacy/duplicate/invalid migration, all overlay slots, cross-byte palettes, flags/control nibbles, surrounding canaries, reserved bytes, future/foreign ownership, save-dirty behavior, and hook GPR/FPR/CR/CTR/XER preservation. This is independent of the native card's on-disk checksums; save/reload and downgrade behavior should also be checked in Dolphin on a test card before distributing the format change.

## OSD Identities and Styling

`src/osd_style.c` defines the pure message-tag decoder, palette, replacement-key rule and best-frame timing colors; `src/osd_style_game.c` applies the style at GX time after legacy callers recolor their subtexts. Configurable messages explicitly supply a settings identity. Untagged event feedback keeps its own colors and visibility. The GOBJ return contract stays valid when a category is Off; no caller is given a NULL pointer.

Native producers use `Message_DisplayOSD id, arg, line, inline, best` in `ASM/Globals.s`. The original low-byte kind is retained for queue behavior; metadata separately identifies the setting, the first/second/third integer vararg to snapshot, its text line, an optional Wavedash first-line layout, and the best displayed frame (default 1). C producers use `OSD_MessageTag`. Queue replacement matches both kind and settings ID, preserving alternate IDs and the -1 always-new convention. Do not infer identities or timing semantics by parsing formatted strings.

Timing color compares the unchanged displayed measurement with the declared best frame: best=Cyan, best+1=Green, best+2=White, earlier/later=Red. Instant double jump retains its original best interval of five and still displays five; it is not relabeled Frame 1. No-timing metadata leaves success rates, GALINT, advantage, SDI counts, angle/hop quality, early/late diagnostics, lockout state and shortening-window outcomes under their existing rules. L-cancel adds a separate outcome line so its input timing does not redefine whether the cancel succeeded.

Wavedash uses two positioned first-row subtexts for its title and frame value, retaining Angle and optional Hop as rows two/three. The native combo-end callback finds the canonical Combo Counter in the shared C queue, rather than looking up an obsolete native kind-13 text array. MsgData's original prefix/text pointer offset and existing function-export indices are preserved; extensions are appended and ABI fingerprints force full builds.

Both editors select Off, White, Red, Green, Blue, Yellow, Cyan and Magenta. In the L-button menu, B cycles forward and Z backward; boolean global rows toggle with either button. Stick/D-pad navigation, X/Y position selection and A/Start exit remain available. Each row shows its value and a color preview. Lab changes only the selected category through its existing eight-choice rows.

Both menus expose TURN OSDS OFF (On hides messages, Off restores normal display). It uses flag bit 0; native row ID 6 maps to that flag rather than enable-mask bit 6. Suppression is applied to tagged configurable messages at GX time, including existing text and backgrounds while paused. Individual palette choices and trail flags remain intact; untagged event feedback is unaffected. Message lifetimes continue normally, so expired messages do not return when suppression is lifted. No extra saved bytes or format-version bump is needed; the four reserved bytes remain free.

`src/osd_editor_game.c` maps native cursor order to sparse settings IDs and updates the existing row subtexts. The native hook intercepts B/Z before the RSS Boolean toggle path. It updates the selected Boolean in the rules struct while leaving the row snapshot unchanged until native animation copies it; the exit save then preserves the selected nonzero palette. Untitled/reserved rows cannot be edited. Editor exports are appended after existing function indices and ABI fingerprints force a full rebuild. PowerPC tests cover palette cycling/wrapping, snapshot saves, untouched reserved bits, master/trail independence, future-format fallback and fresh-service round trips, in addition to styling tests. Font placement, native menu clicks and real memory-card save/reload require Dolphin verification.

## Project Structure
### Shared hitbox trails

`src/trails.c` owns the bounded trail model, fade curves, profile priority and player palette. `src/trails_game.c` captures/draws once through the common match-start service; Lab and Eggs-ercise configure it through `EventVars.trails`. The bank holds 128 forty-byte samples (5,120 bytes), plus twelve bytes of indices/clock state. This is runtime RAM; the two global preferences use their existing flag bits, with no save-format change or consumption of the four reserved bytes.

The global Very Fast/Instant toggles override event-local Enable/Decay; both On selects the Very Fast union once. Local menus display the active global override. Very Very Fast holds one tick and fades by 50 per tick, for five nominal visible ticks. Current alpha is 200; history is capped at 72 and normalized to retain the preset's lifetime. Off under Decay means no fading, not disabled trails. A dense match can overwrite samples before their full nominal lifetime.

Colors use player-block accent indices (including teams), with CPU kind explicitly gray. Item ownership is checked against live fighter objects; unmatched owners are neutral gray. Lab body overlays no longer replace trail colors. The shared callback collects all six slots and both subfighter indices, after fighter/item/event updates, and uses a simulation tick with the native match-frame key for pause/double-capture/rewind detection. Native DOL inspection confirms the frame-key field and its pause/frame-advance gates. Scene changes, successful common savestate loads and Ledgedash repositioning clear the bank. Stationary identical samples are refreshed instead of repeatedly blended.

Native L-menu row IDs 2 and 4 now map explicitly to the Very Fast/Instant flags through `TM_SETTING_NATIVE_ROW`; row 6 maps to the OSD master-off flag, and other rows map to ordinary OSD enables. The two rows do not use bits 2/4 of the enable mask. `SettingsReadIndexed` preserves the hook register context while supplying the native row index. The existing PowerPC test runner now also covers profile union/priority, alpha endpoints, palette mapping, pause/deduplication/rewind, ring bounds and native row mapping.

Native row constants and both labels live in `src/settings.h`. The build generates their ASM label macros alongside the identity include, so L-menu and Lab labels stay consistent. Lab's two change callbacks update only the selected flag, then refresh both menu values from the shared service. Local menus distinguish Off, Very Fast, Instant and Both On; Both On retains Very Fast's single-render union. Tests round-trip every combination into a new emulated service instance and verify that row edits preserve the sibling flag, OSD colors/mask, overlays and other global flags.

Runtime checks should cover both global toggles, all local decays, paused/frame-advanced/slowed play, recordings and restores, Nana, owned/reflected/neutral items, team colors, dense projectile loads, and native hitbox/body-overlay combinations. The model tests and build do not verify visual brightness or callback timing in Dolphin.

There are a few important directories to know about:
1. `src/`: this directory contains the source for the C events, as well as some setup code for the event in `events.c`.
2. `MexTK/`:
    - The `include/` subdirectory contains headers for internal melee functions. Calling these will call native ssbm code.
    - The `melee.link` file maps native melee symbols to addresses.
    - The `.txt` files contain symbols that we want called by the m-ex system.
        For example, C events will want their `Event_Init` and `Event_Think` functions called.
        We only use the evFunction and cssFunction modes.
3. `ASM/`: This huge directory contains gecko codes for various things like UCF, old events, OSDs, etc.
Every file has a injection address at the start.
When the game boots up, it will overwrite the instruction at that address and replace it with a branch to the asm contained in the file.
The `.asm` files will be injected and run, while the `.s` files contain include macros and will not be assembled by hgecko.
4. `dats/`: The dat file format is used by SSBM for storing data, such as models and animations.
This directory contains HUD models and animations for events.
You will need to use [HSDRawViewer](https://github.com/Ploaj/HSDLib) to view these.
5. `bin/`: This directory contains binaries used in compilation.
    - `hgecko` is a reimplementation of Fizzi's `gecko` tool for better performance. This compiles asm files into gecko codes.
    - `hmex` is a reimplementation of Ploaj's `MexTK` tool for better performance. This compiles C code into DLLs run by m-ex.
    - `gc_fst` allows modifying the ISO filesystem.

## Melee Stuff

### HSDRaw and Dat Files

[A dat file, or an HSD_Archive](https://github.com/doldecomp/melee/blob/master/src/sysdolphin/baselib/archive.c) is the file format for data in ssbm.
Everything is stored in dat files - models, animations, code, textures, etc. Only cutscenes and music are stored differently.

[You can open, view, and edit dat files with HSDRawViewer](https://github.com/Ploaj/HSDLib).

The `dats/` directory contains some of these files.
They contain event specific objects, mostly menu models with some random other data.

### Objects

- **GOBJ** - game object. This is a very generic object.
They can have a model, an update function (think function in melee), pointer to arbitrary data, etc.
Most everything is a GOBJ.
- **JOBJ/JOBJDesc** - joint object (models).
Each JOBJ has a sibling and a child, forming a tree of joints.
Each joint can have a DOBJ, forming a large tree of models.
Technically, HSDRaw only deals with JOBJDescs, as JOBJs are only created at runtime from a JOBJDesc.
However, it calls them JOBJs for whatever reason. So JOBJDescs in training mode are JOBJs in HSDraw.
This same pattern holds for a lot (but not all!) HSDRaw objects.
Almost every object in the training mode dat files are JOBJDescs (node will be 0x40 in length).
You'll need to right click on the node -> Open As -> JOBJ in HSDRaw in order to view the model.
- **DOBJ** - display object. These contain meshes, textures, a material, etc.
- **MOBJ** - material object. Lots of stuff here, but I don't know much about them.
- **HSD_Material**. This contains colouring information. Most objects are coloured by setting the diffuse field in these.
- **TOBJ** - texture object. Images that will be displayed on a mesh.
- **POBJ** - polygon object. Contains a mesh.

## How To Do Things

- If you want to alter an event written in C (easy):
    - The training lab, lcancel, ledgedash, wavedash, edgeguard, and powershield events are written in c.
    This makes them much easier to modify than the other events. Poke around in their source in `src/`.
- If you want to alter an event written in asm (big knowledge check):
    - You will need to know a bit of Power PC asm.
    - Read `ASM/Readme.md`
    - Go to `ASM/training-mode/Custom Events/Custom Event Code - Rewrite.asm` and search for the event you want to modify.
    - These will A LOT trickier to modify than the other events. Prefer making a new event or modifying the lab.
    - There are a lot of random loads from random offsets there. Grep for that address in MexTK/include to see where it points.
    Feel free to put a comment there indicating the source!
- If you want to make a new event (tricky, but super flexible):
    - Add a file and header to the `src/`.
    - Add the `EventDesc` and `EventMatchData` structs to `events.c` and add a reference to them in the `General_Events`, `Minigames_Events`, or `Spacie_Events` array.
    - Implement the `Event_Init`, `Event_Update`, `Event_Think` methods and `Event_Menu` pointer in your c file.
    - Add the required compilation steps in `build.sh`. Follow the same structure as the other events. You can skip the dat copy if you don't have any models attached to the event (you won't), like the powershield event.
Poke around the other events to figure out how to implement these.
The powershield event is the simplest and easiest to learn from.
- If you want to create a new OSD (simple-ish):
    - Add your function logic to `src/osds.c`.
    - Add the OSD to the OSD list in `ASM/training-mode/Globals.s`. OSD ids are weird, I don't know exactly how to do this.
    - Add your OSD to the corresponding slot in `ASM/training-mode/Onscreen Display/Toggle UI/Load Alt Text When Loaded With L.asm`.
- If you want to draw graphics (easy):
    - Use the `GFX_Start` and `GFX_AddVtx` functions. Search around to see the specific usage.
    - You can use the `HUD_*` functions for higher level drawing.
    - Drawing from ASM is currently difficult. Check out `ASM/.../Custom ESS Button Actions.asm` for a possible method.

## Debugging Tips
- Development builds enable logging! Call `TMLOG(...)` to print to the dolphin console and the onscreen console. L/R+Z toggles console visibility.
- **Use the dolphin debugger!** Make sure you have the latest version of dolphin for debugging.
    - To set a breakpoint, use the `bp()` fn call in C or the `SetBreakpoint` macro in ASM (which will clobber r3). Then when you boot up dolphin, put a breakpoint on the `bp` symbol.
    - **For Tyro, load `build/TYRE01.map` with Symbols->Load Other Map File.** Or copy it to the Maps/ directory in the Dolphin data directory. The release ZIP includes this map; `GTME01.map` remains its source symbol map in the repository.
