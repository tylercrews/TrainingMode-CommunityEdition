# Global OSDs, hitbox trails, and Ledgedash: investigation and implementation plan

Investigated October 4, 2026, against checkout `b3d6700`. This is a planning document; gameplay code and the ISO have not been changed.

The requested phase-1 changes are feasible. A compact save format can accommodate the new global toggles, including Infinite Shields, OSD title colors, and a reserved phase-2 overlay inside the existing 44-byte settings area, with **four bytes still reserved for future use**. No current training feature needs to be removed for that design. If persistent event high scores are deliberately retired, their **204-byte region** can become a settings extension after its score access/reset paths are changed; section 17 explains the budget and work involved. Before changing the serialized format, give Tyro Edition its own stable game/save identity so switching to upstream cannot reinterpret Tyro settings. The yellow last-non-actionable-frame overlay is feasible for specific, understood action states; an accurate implementation covering every character and action needs separate research.

**1. What “permanent memory” means in this project**

There are three different budgets:

| Budget | What occupies it | Effect of these requests |
| --- | --- | --- |
| Persistent save data | Settings, scores, unlocks, records, names | We need to store small choices and flags here. |
| Runtime RAM | Trail samples, fighter tracking, message objects, loaded event modules | Trails use RAM while a match runs. They do not need to be saved. |
| Disc/code size | C and assembly code, menu labels, fixed palettes, assets | New logic and fixed colors live here and are loaded into RAM. They do not consume settings bytes. |

The game is not limited to a save file's capacity for all its memory. A new feature can contain substantial code while its persistent preference occupies only one bit.

The local SDK calls the structure `Memcard`, located at RAM address `0x8045A6C0`; the pointer to the larger native game-data object is at `0x804D3EE0`. The header describes its first `0x2000` bytes, but **that is neither a free 8 KB settings pool nor the complete native game-data object**.

Sources: [memcard.h](MexTK/include/memcard.h), especially lines 2040–2052 and 2828; [Globals.s](ASM/Globals.s), lines 39–44.

I also inspected the save manifest in both `build/ISOStart.dol` and `build/Start.dol`:

- The main serialized payload is `0x1790` = **6,032 bytes**, starting at game-data offset `0x1868` and ending just before `0x2FF8`.
- Seven name-data banks are separately declared at `0x1F2C` = **7,980 bytes each**, or **55,860 bytes combined**.
- Those declared logical payloads total **61,892 bytes**, before the icon/header entry and card-format overhead. This is not the final on-card file size.
- The main manifest is at native address `0x803BAB74`. The getter at `0x8015CC40` returns base + `0x1868`; the next getter returns base + `0x2FF8` for name data.
- Card operations require a sector size of `0x2000` = 8,192 bytes. File metadata, alignment, checksums and storage behavior must be accounted for separately.

This agrees with the primary [Melee save manifest](https://github.com/doldecomp/melee/blob/master/src/melee/lb/lbcardgame.c) and [card loader](https://github.com/doldecomp/melee/blob/master/src/melee/lb/lbcardnew.c). No `.gci` or raw memory-card image was found in the workspace, so I have not measured the actual save file's allocated blocks or the card's free capacity.

The native main-save filename passed to the card library is `SuperSmashBros0110290334`; the displayed name/banner is customized separately. This report inventories the fields the build stores, not the particular enabled values on your own card, which was not available for inspection.

The main payload includes native unlock/progress information, trophy data, preferences, event scores and fighter records. Much of the local header is named `unk...`, but that does not mean the bytes are unused. The native [save-data types](https://github.com/doldecomp/melee/blob/master/src/melee/gm/types.h) identify fighter records beginning at the area the mod already partly reuses. In particular, the space immediately after `0x1F50` belongs to native records; it is not confirmed free storage.

The GALE01-specific [name-region save patch](ASM/training-mode/Misc/Do%20Not%20Save%20Nametag%20Region%20to%20Memcard%20for%20GALE01.asm) skips chunks 2 and 3 only when the running disc identifies as GALE. The normal build changes the disc ID to GTME01. We should not count those name chunks as available settings storage for a normal TM-CE ISO.

**2. Exact current settings costs**

Offsets below are relative to the RAM game-data base, not raw offsets into a checksummed/encoded card file. The main save payload starts at relative offset `0x1868`, so the OSD bitfield is `0x6BC` bytes into that logical payload.

| Offset | Current field | Allocated bytes | Contents |
| --- | --- | ---: | --- |
| `0x1F24–0x1F27` | `TM_OSDEnabled` | 4 | 32-bit enable mask. Twenty OSD IDs are declared; nineteen are normal selectable rows. |
| `0x1F28` | `TM_OSDPosition` | 1 | HUD, sides, top left, top right. Old assembly names still call this “MaxWindows.” |
| `0x1F29` | `TM_EventPage` | 1 | Last/current event page. |
| `0x1F2A` | `TM_OSDRecommended` | 1 | Legacy recommended/event-OSD override behavior; still read by assembly. |
| `0x1F2B` | `TM_LabFrameAdvanceButton` | 1 | Advance and decrement button choices, one nibble each. |
| `0x1F2C` | `TM_LabDPadUD` | 1 | Up/down shortcut choices, one nibble each. |
| `0x1F2D` | `TM_LabDPadLR` | 1 | Left/right shortcut choices, one nibble each. |
| `0x1F2E` | `unused1F2E` | 1 | Explicitly unused by the current mod. |
| `0x1F2F` | `TM_LabCPUInputDisplay` | 1 | Saved input-display choice. |
| `0x1F30–0x1F3F` | HMN saved overlays | 16 | Eight `(group, color/effect)` pairs, two bytes per pair. |
| `0x1F40–0x1F4F` | CPU saved overlays | 16 | Same eight-pair format for CPU. |
| **Total** | | **44** | **43 assigned; one explicitly unused.** |

I checked the sizes and offsets using the installed PowerPC compiler, rather than assuming the C comments were accurate. `sizeof(Memcard)` is 8,192; `sizeof(OverlaySave)` is 2; all settings offsets above match. Compiler output is in ignored build scratch file `build/investigation/layout-audit.s`.

Current overlays support **17 conditions** and **11 choices**: None, Red, Green, Blue, Yellow, White, Black, Remove Overlay, Show Collision, Invisible, and Play Sound. These choices are effects as well as colors. Although all conditions can be configured in RAM, the current save routine stores only the first eight enabled conditions for each side, in group order. Later enabled conditions can be lost after reload. Removing a row or turning off an overlay does not shrink the fixed 32-byte allocation automatically.

The legacy recommended byte is not immediately reclaimable: `Custom Event Code - Rewrite.asm`, around line 11820, still reads it and may OR event-specific OSD bits into the saved mask. Removing its menu row did not remove that behavior.

There is another potential source of space: the project currently lists **28 events** (5 + 16 + 7) and assigns four-byte score positions starting at `0x1A70`. That reserves 112 bytes through `0x1ADF`; it does not mean every event actually has a nonzero score. The native event-score span extends to just before `0x1B3C`, leaving **92 bytes corresponding to 23 currently unassigned score positions**. Treat this as a candidate requiring changes to access/reset paths, and reserve capacity for future events if persistent scores are retained. It is not included in the conservative plan below. If all persistent event scores are retired, the combined candidate is **204 bytes**, not 204 plus 92; see section 17.

Thus the honest remaining-space answer is: **one immediately identifiable unused settings byte; 92 candidate score bytes; no justified claim that the rest of the header or payload is free**. The compact layout below creates four explicitly reserved bytes without extending the area already used by settings.

**3. Recommended persistent format: everything fits in the existing area**

An overlay's group can be implied by its array position. Eleven possible choices require four bits per actor per group. Store HMN and CPU choices in the two nibbles of one byte per condition:

- Seventeen existing conditions need 17 bytes total.
- Reserve an eighteenth condition for the phase-2 overlay, making **18 bytes total**, versus 32 today.
- This saves **14 bytes** and permits all conditions to survive reload, eliminating the eight-enabled-overlays-per-side limit.

Use a small OSD choice enum: `Off, White, Red, Green, Blue, Yellow, Cyan, Magenta`. Eight values fit in three bits. Nineteen selectable rows need 57 bits, rounded to **eight bytes**. Eight bytes also fit a twentieth compact slot, with four bits spare. Keep a fixed ID-to-slot mapping in code; do not index the compact array directly with sparse OSD IDs.

Retain the existing four-byte enable mask for assembly compatibility. Menu setters synchronize it with the compact choices. This modest duplication avoids changing every existing enable check at once.

| Proposed allocation | Bytes |
| --- | ---: |
| Existing enable mask | 4 |
| Existing position, page, legacy behavior, advance/decrement, D-pad U/D, D-pad L/R, input display | 7 |
| Version and global flags in formerly unused `0x1F2E` | 1 |
| Packed overlays, including phase-2 slot | 18 |
| Packed OSD choices | 8 |
| Format signature | 2 |
| Unallocated reserve | 4 |
| **Total** | **44** |

Suggested byte placement:

| Offsets | Proposed purpose |
| --- | --- |
| `0x1F24–0x1F2D` and `0x1F2F` | Preserve current fields and their offsets. |
| `0x1F2E` | Two version bits and six flag bits. |
| `0x1F30–0x1F41` | Eighteen packed overlay bytes. |
| `0x1F42–0x1F49` | Eight packed OSD-choice bytes. |
| `0x1F4A–0x1F4B` | Format signature. |
| `0x1F4C–0x1F4F` | Four reserved bytes. |

Six flag bits cover TURN OSDS OFF, Very Fast trails, Instant trails, missed-L-cancel flash, phase-2 yellow flash, and Infinite Shields. Infinite Shields consumes the previously spare flag bit, so no flag bits remain free in this byte; the four reserved bytes remain untouched. A future independent global Very Very Fast toggle would need space elsewhere, although the event-local decay preset still needs no save bytes. The flags plus version occupy one byte as a group. Use explicit masks/byte packing; an ordinary C enum can occupy four bytes, and C bitfield layout should not define a serialized format.

A more direct palette indexed by all 32 bitfield IDs costs 12 bytes instead of eight. That alternative fits too, but consumes the four reserved bytes. A three-bit palette allows eight values total; expanding beyond that would require a format change or additional storage.

Migration must operate on Tyro's separate save or an explicitly created copy of a legacy save, never overwrite the shared upstream save. Read both old overlay lists into temporary RAM **before** overwriting their bytes, validate them, and convert them into the packed arrays. Enabled old OSD rows become White; disabled rows become Off. New flags default off. Write the signature/version only after constructing a complete valid new record. Initialize the whole new configuration on a genuinely new save or no-card boot, and use the signature together with the version to distinguish formats. Validate all enum values during loading.

Older code cannot safely read the repacked settings: it will interpret new overlay bytes as old pairs and may rewrite them. A signature/version only helps readers that understand it; it cannot protect a save from an unmodified upstream build. Isolate Tyro's save identity before migration, as detailed in section 16. Upstream then continues using its original file, while Tyro reads/writes its own. Earlier Tyro releases that still use `GTME01` remain part of the legacy shared identity. Compatibility between future Tyro versions sharing the new ID still requires version-aware loading. The existing card save/checksum pipeline should remain responsible for writing the record; avoid inventing a second raw write path.

**4. Where the global menu and runtime actually live**

The left/L-trigger OSD menu is an assembly customization of Melee's rule/random-stage-selection interface. Analog and digital L presses are handled in [Custom ESS Button Actions.asm](ASM/training-mode/Custom%20Events/Event%20Select%20Screen/Custom%20ESS%20Button%20Actions.asm). It sets the custom rules-menu flag and opens that interface.

The key files are under `ASM/training-mode/Onscreen Display/Toggle UI/`: `Load Alt Text When Loaded With L.asm`, `Load Toggles When Loaded With L .asm`, `Save Toggles When Loaded With L.asm`, and `XY To Adjust OSD Position.asm`. They presently render fixed rows and return/store Boolean values. Changing a row to a color selector requires changing the input/value/display handling, not just its label. There are holes and non-obvious mappings between native rows and OSD IDs; define an explicit shared mapping.

There is a second editor in the Training Lab: [lab.h](src/lab.h), around lines 1678–1803, and `Lab_ChangeOSDs` in [lab.c](src/lab.c), around line 307. Both editors must use the same settings accessors. The current Lab writer reconstructs the entire enable mask from its nineteen rows; adding independent flags into spare mask bits without changing this writer would cause them to be erased.

Shared event code is compiled into `TM/eventMenu.dat` from `events.c`, `menu.c`, `osds.c`, and `savestate_v1.c`. Lab and Eggsercize are separate event modules. C events call `StartOSDs` from `EventLoad`; many assembly events call its exported function themselves. The general match-start hook, `OnStartMelee`, currently initializes messages and tips.

For “global” to cover all gameplay matches, create a common visual-effects service at the shared match-start hook, with idempotent initialization. Preserve/adapt existing event startup calls so we do not register duplicate callbacks or increment IASA tracking twice. Menus/CSS have no fighters and should not run effects. A shared timer must work in assembly events and ordinary matches; `event_vars->game_timer` is currently initialized/incremented by the C-event loader, so simply copying the Lab trail code into the OSD callback is insufficient.

**5. OSD titles, timing colors, and the master switch**

Titles and results can have different colors: `Text_SetColor` already supports separate subtexts, and several OSDs use it. However, some assembly callers recolor text after `Message_Display` returns, including the L-cancel title. A central title-color assignment must either happen after those callers finish or replace their conflicting assignments.

There are also legacy mismatches between enable IDs and message kinds. For example, Fastfall is enabled by ID 20 but sends message kind 7; Act OoHitstun uses enable ID 28 but sends kind 5. Some miscellaneous and alternate IDs exist solely to keep simultaneous messages from replacing each other. Canonicalize the settings identity separately from the queue/deduplication identity, including the fighter-specific alternate ID 64. Do not blindly look up a title color using the current message kind.

Recommended rendering model: store the OSD's settings identity plus title/result metadata on a runtime message, use a shared title-color lookup, and use an explicit timing helper supplied with the actual displayed frame count. It can coexist with the existing message API while callers are migrated. Avoid identifying meaning by parsing formatted strings.

The timing helper should map displayed frame 1 to Cyan, 2 to Green, 3 to White, and 4+ to Red. Several current counters are zero-based and add one when printing; normalize before choosing color. There is **no universal current frame-color rule**: many callers choose green only for perfect timing; others use white for late actions, red for failure, or their own constants. Both C and assembly callers need auditing.

Apply the new scheme to timing results. Non-timing results such as success rate, number of SDIs, frame advantage, angle quality, GALINT, and explicit success/failure have their own meanings. For L-cancel in particular, timing color and whether the cancel succeeded should be distinct; a legitimate later input in the successful window should not be relabeled a failed cancel solely because its timing number is red.

Some titles share a line with the number. Wavedash currently has `Wavedash Frame: %d`, an angle line, and an optional hop line, already reaching the three-line message limit. Use separate positioned subtexts or an appropriate label/result representation within the existing three lines; appending a fourth line exceeds `MSG_LINEMAX`. One-line PNJ messages similarly need a deliberate title/result treatment.

TURN OSDS OFF should preserve every configured color/enable preference and take effect on existing messages immediately. Recommended scope: suppress configurable text OSDs while leaving separately enabled hitbox trails/body flashes operating. Keep event-specific instructions and essential feedback visible. This is a proposed behavior choice and can be broadened if “off” is intended to suppress every visual aid.

Do **not** implement the master switch by returning NULL from `Message_Display`: many assembly and C callers immediately dereference its returned GOBJ. Start with message visibility control in the manager/draw path, identifying configurable OSDs explicitly. Audit the older direct assembly text renderer too; some event text bypasses the C message path. Event startup currently ORs some OSD bits into the mask, so a separate persistent suppression flag is necessary even if startup enables a text category.

**6. Hitbox trails: current behavior and requested changes**

There are two near-duplicate implementations:

- Training Lab: `lab.h`, around line 1923, and `lab.c`, around line 6822.
- Eggsercize: `eggs.h`, around line 49, and `eggs.c`, around line 210. This is the previously ported event implementation.

Each has a 64-entry ring. One sample contains two three-dimensional positions, a radius, a four-byte color, and a creation frame: **36 bytes** verified by the PowerPC compiler. The sample array is **2,304 bytes of runtime RAM**, plus the four-byte ring index, before management objects/settings. These arrays are not part of the memory-card settings. Consolidate them into one shared implementation rather than add another copy per event.

Both implementations collect main fighters in slots 0–3 and item hitboxes. They do not explicitly collect Nana/subfighters; a global implementation should consider all six fighter slots and active subfighters. More simultaneous fighters/projectiles may require a larger ring. At sixteen samples per frame, a 64-entry ring retains only about four frames, regardless of the nominal decay duration. Size and performance must be measured together.

The current color function accepts integer damage, changes the green/blue channels, and always starts alpha at **200/255**, about 78%. Lab can override that color with its running body overlay, often alpha 180. Eggsercize does not have that override. The normal live hitbox visualizer also sets alpha 200. I did not find two configurable “hard hit” and “soft hit” opacity fields in the trail code. The apparent difference could be current versus historical samples, overlapping shapes, or native rendering. Confirm that visual interpretation in Dolphin before changing the wrong path.

The existing decay calculation is:

```text
age = current_simulation_frame - creation_frame
fade = max(0, (age - hold_frames) * fade_per_frame)
drawn_alpha = starting_alpha - fade
skip drawing when fade >= starting_alpha
```

| Mode | Hold | Fade/frame | First invisible age at starting alpha 200 |
| --- | ---: | ---: | ---: |
| Slow | 30 | 2 | 130 |
| Normal | 15 | 4 | 65 |
| Fast | 10 | 8 | 35 |
| Very Fast | 5 | 13 | 21 |
| Instant | 0 | 200 | 1 |
| Off | 0 | 0 | Never fades; overwritten by the ring |
| **Very Very Fast proposal** | **1** | **50** | **5** |

Age zero is the creation frame. These are mathematical lifetimes, not a guarantee the ring retains each sample that long. The menu's current “Off” decay means **no fading**, not disabled trails; the separate Enable toggle controls drawing.

Very Very Fast at hold 1/fade 50 is a starting proposal, giving visible ages 0–4, clearly between Very Fast and Instant. Evaluate it visually before finalizing. A new fixed preset costs **zero save bytes**; its table entries and label occupy code/data space.

For translucency, I recommend separating the current sample from history: retain a clear current hitbox and cap older samples around alpha **64–80/255** as an initial tuning range. These numbers are proposals. If “hard/soft” instead means strong/sour hitboxes, define that classification first; damage alone is not a reliable generic identifier. Reducing starting alpha also shortens the existing lifetime formula, so decide whether to normalize the fade curve to preserve duration or intentionally shorten it. Fixed opacity constants cost no persistent storage. Two saved adjustable alpha values would cost two bytes and could use half the four-byte reserve.

Player-colored trails are feasible without saving per-player colors. Derive identity from player type and `Playerblock.color_accent`: Red, Blue, Yellow, Green, Gray for CPU. Handle teams and CPU explicitly, not `ply == 1` or controller number. One useful SDK function is named `Fighter_GetPlyHUDColor`, but the native implementation at `0x80160968` actually looks up a **color index**, so its header parameter name must not be trusted as meaning fighter slot. See the native [color lookup](https://github.com/doldecomp/melee/blob/master/src/melee/gm/gm_1601.c). The native colors are relatively dark; a brighter equivalent palette may read better on stages.

Give owned projectiles their owner's identity color where ownership is known; neutral items use a consistent fallback. Reflections, destroyed owners, nested item ownership and Ice Climbers need testing. The Lab's current overlay-to-trail color override should be removed or made optional for this mode, or body flashes will defeat the requested player color.

The two global rows can be independent Boolean preferences, costing **two bits total**. If both are enabled, Instant still draws the current frame; it is not inactive. Rendering both identical current samples separately can make them more opaque and wastes work. Recommended behavior: keep both preferences enabled, capture once, and draw the union once. Since Very Fast includes age zero, it naturally supplies the visible output when both are enabled. Both modes can use one ring, rather than two 2,304-byte arrays.

Define interaction with event-local menus explicitly. Recommended initial rule: an enabled global profile supplies the shared trail effect; otherwise a Lab/Eggsercize local override can select the other presets. Show the active global state in those menus and avoid registering/rendering duplicate effects. Clear the shared history on match/event start, retry, savestate load, recording import and timeline rewind so stale samples do not reappear.

**7. Global missed-L-cancel flash**

The Lab already has `OVERLAY_MISSED_LCANCEL`, checked in `CheckOverlay` around line 1130. It recognizes aerial landing states and records whether the trigger timer is at least seven on entry. Its current body overlay can remain active throughout that landing state; it is not necessarily a one-frame flash.

Move the detector into common fighter-visual code and use the actual landing/cancel result where available. Define a short visible flash duration separately from the global enable bit, validate hitlag and landing entry order, and reset per-fighter state on retry/restore. Apply it to every intended player, including active subfighters, rather than Lab's fixed HMN/CPU pair. Pick a documented priority relative to Lab overlays, invincibility, damage flashes, invisibility and the phase-2 yellow cue. Avoid clearing native overlay slots indiscriminately; save/restore or compose the layer the service owns.

Persistent cost is **one enable bit** for fixed color/duration. Detection state and flash countdowns are runtime RAM. A saved color or duration would be additional optional storage, not required by the request.

An initial global preset could use red for four simulation frames, then be tuned in-game. That is a proposed default, not the existing Lab behavior or a measured visibility requirement.

**8. Phase 2: yellow on the last frame before inputs become actionable**

Yellow already exists in the overlay palette. What is new is the condition, not the RGB value. Define it as: **this displayed simulation frame is still non-actionable, and the next simulated frame becomes actionable for the defined input category**.

This is more difficult than identifying the first actionable frame. The current `OVERLAY_ACTIONABLE` and `CheckIASA` helpers evaluate present state; seeing a transition on the next frame cannot retroactively flash the previous frame. End-of-animation is not a universal answer because attacks can become interruptible early, landing/shield/hitstun have their own timers, specials have character-specific cancel paths, and hitlag/animation rates affect the boundary. Some states permit a subset of actions before others, so “prevents inputs” needs a defined scope.

Start with a small verified set: normal landing, L-canceled and missed aerial landing, special landing, shieldstun, and selected ordinary attacks with understood IASA boundaries. Research each transition and account for callback order and displayed-frame indexing. Expand to hitstun, dodges, rolls, jumpsquat and specials only as their boundaries are proven. Do not advertise universal coverage while relying on a generic animation-length heuristic.

Use one yellow flash per actual simulation frame, with intentional behavior during pause/frame advance and hitlag. Validate by frame stepping and trying inputs on the flash frame and the next frame. Existing comments document a stale IASA flag and a spotdodge exception; those are concrete reasons to test individual states.

Reserve the global enable bit and eighteenth packed Lab overlay condition now, but expose the global feature after the detector's supported scope is validated. As a global fixed-yellow feature it costs **one bit**; as a selectable Lab overlay it uses the already budgeted extra pair of nibbles. Extra detector tables, timers and tracking are code/RAM, not save capacity.

**9. Order of implementation and verification**

| Step | Work | Completion criterion |
| --- | --- | --- |
| 0 | Give Tyro a stable separate game/save identity, update identity-dependent hooks, and verify isolation. | Upstream and Tyro saves coexist on the same card; switching games preserves each other's settings and records. |
| 1 | Add shared settings accessors, explicit packing, format validation and migration. | Old saves preserve settings; new/no-card saves initialize safely; round-trip succeeds; four reserved bytes remain. |
| 2 | Unify trail collection/rendering and clocks; migrate Lab/Eggsercize to shared service. Add player colors, translucency tuning and Very Very Fast. | One callback and one draw path; frame advance and restore are correct; native hardware/Dolphin rendering remains usable. |
| 3 | Add global trail rows, missed-L-cancel flash, and Infinite Shields through the common match service. | C/assembly events and ordinary matches use the preferences; shield handling includes all fighters/subfighters and recording overrides. |
| 4 | Canonicalize OSD settings identities, title/result metadata and timing helper. Update C/assembly callers. | All nineteen selectable categories have correct titles; displayed timing frames 1/2/3/4+ have Cyan/Green/White/Red. |
| 5 | Update both OSD editors and add TURN OSDS OFF. | Preferences persist; currently visible OSDs suppress immediately and re-enable correctly; no unsafe NULL message return. |
| 6 | Prototype the phase-2 last-frame detector with documented state coverage, then expose it globally. | Frame-step evidence verifies the last blocked/first actionable boundary for every supported state. |

Use small, reviewable changes; build shared C code, affected event modules and the assembly codeset together after API changes. `build.sh` already centralizes these modules, so a new shared source file must be included in the `eventMenu.dat` build and exports added consistently to `tmFunction.txt`/assembly definitions. Preserve existing ABI offsets or append fields/exports deliberately.

Meaningful automated checks are warranted for save packing/migration: all eleven overlay values, all eight OSD choices, sparse-ID mapping, old-format conversion, invalid input handling, unrelated-byte preservation, and binary boundary checks. Compile-time assertions should verify offsets and sizes on PowerPC. Build checks should cover changed C and assembly interfaces.

The runtime matrix should cover Lab, Eggsercize, another C event, at least one legacy assembly event, ordinary matches, all four player colors/CPU, teams, Nana, transformation, fighter/projectile hitboxes, reflected/neutral items, hitlag, slow motion, frame advance, pause, restart and save/reload. For the master switch, include messages whose callers recolor text after creation, miscellaneous/alternate IDs and direct assembly event text.

The work performed for this investigation was source tracing, save-manifest inspection and compiler layout verification. A full game build, in-game appearance check, live memory-card capacity inspection and phase-2 timing validation have **not** been performed. Compiled DAT sizes are not a measurement of live heap usage; exact added code/RAM costs beyond the concrete buffers and serialized format require the implementation and runtime measurements.

**10. What to remove if future storage becomes tight**

First pack existing fields: the overlay conversion alone recovers fourteen bytes without removing a feature. A fixed enum is much cheaper than a saved RGBA per OSD: nineteen raw four-byte title colors would cost 76 bytes, whereas the proposed choices cost eight.

If actual removal is still needed, the current saved Lab overlays are the largest custom allocation (32 bytes total). One HMN or CPU saved pair is two bytes; one slot from both lists is four. Giving up all saved overlay preferences recovers 32 bytes, but there is no reason to do that for this request. The old recommended-OSD behavior can recover one byte only after its remaining assembly reader/writer and initialization are removed. Input display and each packed controls byte recover one byte apiece, with a greater convenience cost relative to their small size. The enable mask is already reasonably compact; turning off individual OSDs does not reclaim its allocation.

Additional future choices can use the four reserved bytes, the remaining packed palette capacity, or deliberately audited unused score positions. The last spare global flag bit is now assigned to Infinite Shields. A separately versioned configuration file is another architectural option if the feature set eventually outgrows the current area; it requires its own card lifecycle/error handling and would consume card file/block capacity. Expanding the existing save format or repurposing more native records should be later choices, not prerequisites for the requested features.

**11. Ledgedash: current colors and the requested changes**

These additional requests are feasible. The attack/dash classification is a small change. A brighter protection overlay is straightforward once its visual priority and frame boundary are defined. Eggs require more integration because surface selection, native item state, callbacks, and the event's reset rules all matter.

The current event is implemented in [ledgedash.c](src/ledgedash.c), with action IDs and runtime data in [ledgedash.h](src/ledgedash.h). Its main callback runs at priority 15. `Ledgedash_HUDThink` records a 30-entry action timeline while ledge intangibility remains. `Event_Think` uses that timeline to color the fighter when **Color Overlays** is enabled. The same fixed palette supplies the HUD legend and timeline:

| Category | Current RGBA |
| --- | --- |
| None | 40, 40, 40, 180 |
| Cliffwait | 120, 120, 120, 180 |
| Fall | 128, 255, 128, 180 |
| Fastfall | 80, 160, 80, 180 |
| Jump | 52, 202, 228, 180 |
| Airdodge | 230, 22, 198, 180 |
| Attack | 255, 255, 255, 180 |
| Landing | 255, 128, 128, 180 |
| GALINT | 128, 128, 255, 180 |

The Attack branch currently checks `atk_kind != 1`, standing grab, and dash grab. It does **not** explicitly check ordinary dash; dash normally falls through to the default GALINT category. Add `state_id == ASID_DASH` to that existing branch and rename its enum/legend to **Attack / Dash**, preserving the existing numeric slot and grab coverage. Use initial dash as the first scope; including sustained Run would be a separate behavior choice. This affects both the action timeline and the corresponding body color. Check that the longer legend label fits the HUD.

There is currently no independent brighter overlay for protection. The GALINT fallback checks the remaining ledge timer, but it is not a general invincibility indicator. **Keep Ledge Invincibility** is a gameplay assistance option: it replenishes the timer while hanging on the ledge. It should remain separate from the new visual indicator.

Recommended new menu row: **Invincibility Highlight**, Off/On. When enabled, brighten the current action tint while the fighter actually has invulnerable or intangible hurt status. This preserves recognition of Attack / Dash during protected frames. As a starting visual proposal, blend RGB roughly 40% toward white and raise alpha from 180 to about 230; white Attack would gain strength through alpha. For protected frames without a classified action, use a bright blue/cyan tint derived from GALINT. Keep the original HUD palette so its legend stays consistent. These values need an in-game appearance check.

Read the effective hurt status, using the script/game hurt kinds and the actual ledge/respawn protection state, rather than assuming a nonzero ledge counter covers every source. The SDK exposes `hurt.kind_script`, `hurt.kind_game`, and `hurt.intang_frames`; its `FtHurtKind` distinguishes normal, invincible, and intangible. Verify which values are authoritative at the chosen callback point, including the final protected frame, hitlag, respawn, and character-script intangibility. If the desired scope is strictly ledge protection, the detector can be limited accordingly and the label should say so.

Calculate this cue from current fighter state independently of the 30-entry action log, so it still works outside the timeline window. The existing renderer clears native overlay slots 0 and 1 while enabled; integrate it with the common visual service described above so missed-L-cancel flashes, the future yellow cue, and native effects have deliberate priority. Clear any overlay owned by this feature when it turns off or protection ends. Changing the title or fixed palette uses no persistent storage.

**12. How Eggsercize works today**

The event-specific logic is in [eggs.c](src/eggs.c), especially `Egg_Spawn` at line 77, the damage callback at 139, and the main update at 315. Defaults and menu options are in [eggs.h](src/eggs.h).

1. **First spawn:** initialization creates the HUD and camera subject. The first egg is created once the human fighter's `input_enable` flag is true. There is no explicit multi-frame spawn delay in `Egg_Spawn`.
2. **Pick a surface:** choose random X and starting Y inside the stage camera bounds, then raycast vertically downward for 1,000 world units. The first floor intersection becomes the spawn position. Depending on the starting height, that can select the main floor or an elevated platform. There is no explicit Ground/Platform choice or requirement to be near the fighter.
3. **Reject unsuitable samples:** retry if there is no floor or the point is less than 25 world units from the previous spawn. Additional downward rays at X minus 8 and X plus 8 must both find a floor. These side rays do not verify the same connected surface or similar height, so they are only an approximate edge check. The search uses an unbounded `while (1)` loop; a constrained Ledgedash target must use bounded candidate selection instead.
4. **Create and launch:** construct `SpawnItem` with native `ITEM_EGG`, both positions set to the collision point, and velocity `(0, 2 + random[0,1), 0)` when **Egg Spawn Velocity** is enabled. That option is enabled by default. Thus the egg starts at a real floor intersection and pops upward; apparent waiting for it to settle is largely physics, not an intentional spawn timer. Free Practice unlocks the menu option that turns this initial upward velocity off.
5. **Maintain the target:** every event update disables pickup and nudging, zeros horizontal self-velocity, applies the selected model scale, updates model/hurtbox visibility, reinstalls the damage callback, and positions an egg camera subject at the egg plus 15 units vertically. The event does not continuously zero vertical velocity or pin the egg to a surface.
6. **Count damage and break:** each damage callback adds `dmg.recent` to the event's accumulated damage. The default break threshold is 12; Free Practice permits changing it. Reaching the threshold triggers effect 1232 and sound 244, calls `Egg_Destroy`, immediately creates the next randomly placed egg, and increments the counter. There is no respawn cooldown here.
7. **Other event behavior:** it clears fighter stale-move tables every frame and ends after one minute outside Free Practice, saving the egg count through the native event score system. Changing egg size destroys the current item and spawns a fresh random target. None of these supporting event behaviors is required just to create a Ledgedash egg.

The native item details are relevant. `Item_CreateItem2` maps to `0x80268B5C`, the native factory that initially marks an item grounded; this is confirmed by the primary [item implementation](https://github.com/doldecomp/melee/blob/master/src/melee/it/item.c). The egg's own spawn callback then enters its falling motion state, whose physics applies gravity. Local disassembly of `build/ISOStart.dol` shows the egg's resting-state initializer at `0x80288E6C` zeros velocity and enters motion 0, while its floor collision path can transition between resting and falling. A prototype can use this verified egg-specific entry after setting valid floor/collision data, with an explicit SDK/link symbol and version checks. Simply setting a generic grounded flag does not prove the target is already in its resting state.

Also, **`Egg_Destroy` is not immediate object deletion**. In this checkout's native binary, `0x80289158` hides the model, stops X/Y movement, enters motion 6 and initializes a 40-update countdown. The animation callback returns the destruction signal when that countdown expires. The active target is replaced immediately while the old hidden object finishes cleanup. This distinction matters for repeated instant resets and target lifetime tracking; use ordinary `Item_Destroy` for deliberate reset cleanup where appropriate.

The callback pointer is shared by item kind: the native initializer at `0x80267978` assigns `it_func = 0x803F14C4 + kind * 60` for common items. This was checked against the local DOL, not inferred solely from the header. Eggsercize writes through that pointer each frame and its damage handler reads the event's single global `egg_gobj`, rather than obtaining all state from the callback argument. General reuse should use a callback that identifies the actual target and event context, and isolate or safely dispatch/restore callback changes. A copied 60-byte callback table is one possible per-target approach, but native state changes must be checked for rebinding it to the shared table. Preserve native lifecycle callbacks.

There is a second SDK trap: `Item_PlaceOnGroundBelow` is declared as taking only a `GOBJ *`, but the native routine at `0x80276174` dereferences an incoming position pointer in register r4. Audit and correct its prototype before using it. The field labelled `is_spin` at `SpawnItem + 0x48` is also used by the native factory as ground/air selection. For this work, trust verified call semantics instead of those names. Existing Eggsercize does not call the mismatched ground-placement helper.

**13. Recommended Ledgedash egg design**

Start with **one stationary egg per attempt** and an event-local **Egg Target** menu with Off, Ground, and Platform. Default Off preserves the existing event until the player opts into target practice. Add an inward distance setting if tuning shows it is useful; a small set of presets would also work.

Use the active ledge as the anchor, rather than the fighter's current hanging/falling position. `Fighter_PlaceOnLedge` already finds the ledge collision line and its inward direction. Its Stage starting-position branch already raycasts at `ledge_x + 12 * inward_direction` to place the fighter on nearby ground. Reuse the ledge discovery, then choose a target point inward from that ledge near the expected landing/attack area. The final distance should be tuned across characters and attack ranges.

| Mode | Proposed placement rule |
| --- | --- |
| Ground | Follow the main floor connected to the selected ledge; choose an inward point with sufficient support for the egg. Do not let a high downward ray accidentally pick an overhead platform instead. |
| Platform | Enumerate currently enabled platform floor lines, choose the nearest suitable platform to the selected ledge/target region, and project/clamp the target onto that real surface with an edge margin. |
| No suitable platform | On Final Destination, show that Platform is unavailable and use an explicitly indicated Ground fallback. Never invent a platform height or create the target at an unchecked air coordinate. |

The collision SDK exposes runtime floor/enabled flags, a descriptor `is_drop` flag for pass-through surfaces, current/previous vertex positions, `GrColl_CrawlGround`, and `GrColl_GetPosDifference`. Those provide building blocks for surface selection and moving-platform support. Pass-through platforms are the first useful scope; solid elevated platforms and unusual stage geometry need an explicit classification policy. Verify support across connected floor segments, slopes, and the egg's actual collision footprint. The old three independent rays are insufficient for exact surface selection.

Create the target during initial ledge setup and after each reset/side change, after old target and particle cleanup. Give it zero initial velocity and enter a verified resting state with correct position, collision line, and model update. Aim for the egg to be present and hittable before the player can leave the ledge. Confirm this by frame stepping; do not promise exact zero-frame readiness before the item/fighter callback ordering has been tested. For moving platforms, let correctly initialized native collision carry it where possible; otherwise track platform displacement and revalidate the supporting line. On a disappearing surface, deliberately relocate or remove it.

Keep the egg neutral, unpickable, and unnudgeable. Record first damage to the target as the basic success signal; a configurable multi-hit damage threshold can be a later option. One egg per attempt avoids immediate replacement under a still-active multi-hit hitbox. Track whether the player was still protected when the collision occurred as optional runtime feedback; validate how hitlag affects the displayed GALINT and remaining-protection values.

**Reset behavior needs an egg-aware path.** Current successful-attempt delays are 120/60/30/1 updates for Slow/Normal/Fast/Instant, and the timer starts after the ledgedash becomes actionable. Instant can reset before an attack's startup reaches the egg. Recommended target-practice behavior: after a successful ledgedash, wait for the first egg hit or a bounded timeout, then apply the chosen reset delay. Preserve normal failure resets. With automatic Reset set to None, leave manual control available and provide target replacement through a reset/reposition action. Decide the timeout through playtesting rather than silently making an unreachable target wait forever.

Store active target/context in Ledgedash runtime data, validate creation failure, and explicitly clean up active and retiring items on retry, reposition, option changes, and event exit. Restore any shared callbacks before the event module is unloaded. Savestate restore needs special handling: the common system copies the event's 512-byte data block but does not serialize arbitrary neutral item objects, so a restored raw egg pointer is unsafe. Reconcile or recreate the target after restore; do not interpret a saved pointer as proof an item still exists. Preserve the Ledgedash camera, action HUD, success calculation, and existing hitbox log when adding target logic.

**14. Added memory cost and implementation order for Ledgedash**

The existing Ledgedash and Eggsercize event menu choices are not serialized into the custom settings area. Following that convention, all these additional Ledgedash changes cost **zero persistent bytes**, leaving the four-byte reserve in the proposed global format intact.

| Addition | Required persistent cost | Optional cost if made persistent |
| --- | ---: | ---: |
| Rename Attack and include Dash | 0 | 0 |
| Fixed brighter protection palette | 0 | 0 |
| Invincibility Highlight toggle | 0 | 1 bit |
| Egg Target: Off/Ground/Platform | 0 | 2 bits |
| Wait-for-hit reset preference, if exposed | 0 | 1 bit |
| Egg distance, if saved as an integer/preset | 0 | 1 byte, or a few bits for presets |
| Optional damage threshold, integer 0-199 | 0 | 1 byte |

The toggle, target mode, and optional reset preference fit into one new preference byte. Saving distance and threshold as well would total three bytes, using three of the four reserved bytes and leaving one. This is an optional extension to the original design, not a claim that existing event settings already persist. It needs a versioned field/default definition and should be chosen before shipping the new format.

PowerPC compiler layout checks produced these exact sizes: `LedgedashData` **72 bytes**, fixed event allocation **512 bytes**, SDK `ItemData` **4,044 bytes**, `SpawnItem` **76 bytes**, and the existing `LdshHitlogData` **3,844 bytes**. There are 440 unused bytes inside the already allocated event block; small target bookkeeping and a 60-byte callback copy can fit there without increasing that allocation. Add a compile-time boundary assertion. One live egg also requires its native item object, GOBJ, model joints and other engine allocations; 4,044 bytes is the SDK data structure size, not the total live heap cost. Measure the full target cost and hidden broken-object overlap in the running game. `SpawnItem` is a temporary construction description, not another long-lived per-egg save record.

Recommended sequence:

1. Update Attack / Dash classification and legend; verify dash, grab, attack, and existing landing/GALINT categories.
2. Add the independent protection highlight, integrate overlay priority, and frame-step the last protected/first vulnerable frames.
3. Prototype a single stationary Ground egg on Final Destination at initial setup and both ledge resets. Verify it is hittable immediately after setup and that callback cleanup is reliable.
4. Add egg-hit/timeout reset handling; test all delay settings, side swaps, retries, event exit, failed creation, and repeated hits.
5. Add Platform placement using actual enabled collision surfaces. Test Battlefield's side/top platforms, a moving platform, slopes, absent platforms, and stage transformations as applicable.
6. Add optional distance/threshold controls and saved preferences only if useful after playtesting. Integrate global trails and flashes through the shared services instead of introducing another copy of their implementation.

The additional investigation included source tracing, PowerPC layout checks, and disassembly of the native egg and item routines in the local DOL. No Ledgedash/egg gameplay code was changed, and no in-game readiness, placement, brightness, heap usage, or timing result has yet been validated.

**15. Global Infinite Shields**

Add an **Infinite Shields** Off/On row to the global settings menu, default Off. Its persistent cost is **one bit**, assigned to the sixth flag bit in `0x1F2E`. The proposed record remains 44 bytes, with four bytes reserved. Fixed full-shield health needs no saved numeric field.

Existing support is in [lab.c](src/lab.c), around lines 6707-6735. Despite living in the CPU settings menu, this code updates all six player slots and both main/subfighter indices. The local choices are Off, Until Hit, and On, with a separate health setting from 0 to 60. Until Hit checks the event's CPU shield-hit state. Wavedash, L-cancel and Powershield also have event-specific assignments of shield health to 60.

For the global toggle, start with **all active players, CPUs, and subfighters maintained at full health**, matching the straightforward meaning of the requested mode. Register the maintenance through the shared match service, rather than relying on Lab's callback. Menus have no fighters. Verify the callback order relative to native shield deterioration, collision damage and shield-size updates so the shield does not visibly shrink between maintenance calls.

Use an explicit effective policy: global On requests full shields; global Off lets event-local behavior continue. Make Lab's menu indicate when its local shield choices are overridden, and ensure its later callback cannot overwrite global full health with a lower local setting. An OSD-text master switch should not turn off Infinite Shields. The saved preference is independent of temporary runtime policy.

There is already a concrete recording exception: `Lab_Init` around line 6243 turns local Infinite Shields Off when importing an rwing savestate, with a comment explaining that it would cause desyncs. Extend this protection to the effective global mode during playback that must reproduce the imported state. Preserve the user's saved global preference and expose an intentional way to resume modified shield practice. Test the recording paths before claiming imported playback remains accurate.

Continuously refilling health is easy, but it is not by itself proof that a single unusually large shield-damage hit cannot trigger break logic before the refill callback. For a literal never-break mode, inspect the native damage/break boundary and prevent health from crossing it while enabled, keeping normal shieldstun and pushback. Begin with the existing full-health behavior, then verify ordinary attacks, sustained shielding, multiple hits in one frame, Marth's Shield Breaker, lightshield and powershield. Add a narrowly scoped break/damage hook if required. This is a timing/behavior question, not additional persistent storage.

**16. Separate Tyro's identity from upstream before changing saves**

The collision is real at the identity level. This checkout's [build.sh](build.sh), line 143, writes `GTME01`. Reading the actual `TM-CE.iso` header also produced `GTME01`, with title Training Mode Community Edition. The configured upstream is AlexanderHarrison/TrainingMode-CommunityEdition; its current [build script](https://github.com/AlexanderHarrison/TrainingMode-CommunityEdition/blob/master/build.sh) writes the same ID. Existing settings use the same custom offsets and native main-save machinery. A different ISO filename, menu version string, or banner does not separate their card settings.

GameCube save identity comprises the four-character game code, two-character maker code, and internal filename. Dolphin's [save-identity implementation](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/GCMemcard/GCMemcardUtils.cpp) checks these fields. The native main filename identified earlier is `SuperSmashBros0110290334`. Keeping that filename is compatible with separate saves when their game codes differ; changing the displayed card caption alone does not achieve isolation.

Dolphin's [GCI-folder implementation](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/GCMemcard/GCMemcardDirectory.cpp) also uses the first four characters for current-game filtering. Choose a new stable Tyro six-character disc ID with a **different game code**, not just a different maker suffix or revision byte. Keep an appropriate region code and maker code, and check the proposed ID/prefix against existing games/mods before finalizing it. Keep this ID stable across normal Tyro releases; use save-format versions and visible release names for subsequent updates.

Recommended approach: give Tyro its own disc/save identity and a separate output filename such as **TM-Tyro.iso**, using the normal card machinery to create a separate main save. Both versions can use the same physical or virtual memory card; they need distinct files, not necessarily distinct card images. This adds another native save file and its block allocation on the card, not another allocation inside the 44-byte settings record. Its full on-card size/free capacity still needs measurement.

This is a small coordinated repository change, not a Dolphin emulator modification. The existing build already changes vanilla `GALE01` to `GTME01`, demonstrating the mechanism. The work includes:

| Location | Required treatment |
| --- | --- |
| `build.sh` header/output/release packaging | Set the new stable ID, Tyro title, distinct output filename and corresponding release names. |
| `ASM/training-mode/Misc/Save File Renaming.asm` | Update the hardcoded `GTME` ISO check and visible save caption/description. This hook changes displayed text, not the internal main-save filename. |
| `ASM/Additional Codes/Use Save Banner #1.asm` | Update the `GTME` ISO check so the new ID still selects the intended ISO-save banner. |
| `ASM/training-mode/Misc/Do Not Save Nametag Region to Memcard for GALE01.asm` | Preserve its intentional vanilla/memcard-launcher distinction; audit rather than replace every `GALE` occurrence. |
| Native card creation/open/update | Verify generated card entries have the new game code and no fallback opens/writes the legacy `GTME` main save. Keep the native internal filename or deliberately give Tyro another one if desired. |
| Lab recording import/export filters | Handle the new identity and explicit legacy-recording imports. |

Centralize the four-character assembly comparison and build identity to avoid mismatches. A new disc ID with unchanged assembly checks would route Tyro through the wrong caption/banner branch. The input ISO check in `build.sh` must continue accepting the supported vanilla source IDs; that check is different from the output identity.

For existing users, the safest first release creates a **fresh Tyro save by default**, leaving `GTME` untouched. If preserving old settings, records, names and unlocks is desired, provide an optional copy/conversion process: read the legacy source, create a separately identified destination, validate the payload, convert the custom settings only in the destination, and regenerate native checksums through a verified path. Do not rename or retag the user's only original save in place, and do not automatically migrate the shared file on boot. A format signature does not stop old upstream code from reading the old namespace.

Recordings deserve an explicit compatibility path. `lab_css.c` around line 105 and `lab.c` around 4551/5815 match card game/maker identity against `os_info` and use the `TMREC` filename prefix. New-ID Tyro therefore will not automatically list old-ID recordings. Validate their export-header version and structure before copying/importing them into the new identity. Broadening a picker alone may not make native `CARDOpen` able to open foreign-game files; audit the card-access path and use a supported importer/converter. Never promise that renaming a host `.gci` filename changes its embedded identity.

Dolphin playtime is separate from Melee's saved game-time counters. Modern Dolphin stores playtime by game ID in `TimePlayed.ini`; a new ID starts an independent counter while leaving historical `GTME01` time intact. See [TimePlayed.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/TimePlayed.cpp). Native counters will instead be independent because the main save is separate, although an optional one-time copy could seed them with old totals.

Game-specific Dolphin INIs use the full ID and also load broader first-letter/three-character defaults. See [GameConfigLoader.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/ConfigLoaders/GameConfigLoader.cpp). Audit required upstream emulator settings and provide equivalent Tyro defaults where needed. Users' per-game controller profiles, cheats, texture folders, and savestate paths may need deliberate copying/reconfiguration. Do not treat Dolphin savestates as portable between different gameplay builds.

The banner-refresh issue is separate from card identity. Dolphin reads the GameCube game-list banner from the disc's `opening.bnr`, as shown in [VolumeGC.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DiscIO/VolumeGC.cpp), and caches game-list objects by ISO path, as shown in [GameFileCache.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/UICommon/GameFileCache.cpp). A distinct Tyro filename helps it discover a new entry, but changing an ID inside a repeatedly overwritten old path does not guarantee a cache refresh. Keep the Tyro `opening.bnr` and native memory-card banner/caption changes in the release workflow, and verify both displays. Refreshing metadata must not require deleting the upstream save.

Before shipping the new format, test a card containing only the legacy save, only Tyro's save, and both; run upstream, Tyro, upstream again, and confirm settings/scores/counters stay separate. Cover Dolphin GCI-folder and raw-card modes, supported Dolphin forks, and hardware if distributed for console use. Exercise insufficient card space, interrupted save, no-card boot, explicit legacy copy, and recording import. On controlled test cards, compare the legacy file before and after Tyro boots/saves. The identity changes and runtime isolation have been planned and source-checked here, but have not been implemented or tested in Dolphin.

**17. Budget if persistent event high scores are removed**

**For the event scores discussed here, the reclaimable target is 204 bytes. None of it is needed for the currently proposed global features.** This is a bounded region already included in the native main save payload. It is not additional free RAM, and it is not free for settings in the current executable: existing score access and records-reset code still owns it.

| Region, relative to the native game-data base | Size | Current role |
| --- | ---: | --- |
| `0x1A70–0x1ADF` | 112 bytes | Four-byte score positions for the current 28-event catalogue. Some events do not produce meaningful saved scores. |
| `0x1AE0–0x1B3B` | 92 bytes | Remaining 23 positions in the native 51-position score span, currently unassigned by this catalogue. |
| **Entire numeric event-score span** | **204 bytes** | **51 × 4 bytes; reclaim all of this if persistent event high scores are retired.** |
| `0x1A68–0x1A6F` | 8 bytes | Separate event-played flags; excluded from the 204-byte recommendation. |

The score span begins `0x208` bytes into the main logical payload. These are not raw `.gci` offsets. The native field at `0x1B3C` and the later progress/trophy fields are outside the score budget. Do not extend the allocation into them. The eight played-flag bytes might also be repurposed if completion tracking is deliberately retired and all its uses are handled, but that is a separate decision, not a consequence of stopping numeric high scores. This estimate does not include unrelated vanilla character/mode records elsewhere in the save.

Source evidence: the project's [load-score hook](ASM/training-mode/Custom%20Events/Event%20Select%20Screen/Event%20High%20Scores/Load%20Event%20High%20Scores.asm) and [save-score hook](ASM/training-mode/Custom%20Events/Event%20Select%20Screen/Event%20High%20Scores/Save%20Event%20High%20Scores.asm) calculate `(page_offset + event_id) * 4` and access `base + 0x1A70`. `GetPageEventOffset` in [events.c](src/events.c), around line 2409, supplies the cumulative page offset. Local DOL disassembly confirms the native getter/setter and the separate write at `0x1B3C`. The primary [native access/reset routines](https://github.com/doldecomp/melee/blob/master/src/melee/gm/gmmain_lib.c) corroborate the indexed score access and adjacent played flags.

An important reset path is **`0x8015EEC8`**. Its final clear, at `0x8015EF08–0x8015EF14`, zeroes **216 bytes starting at `0x1A68`**, reaching just before `0x1B40`. It includes the played flags, all 204 score bytes, and four following bytes. This was verified in the local unpatched DOL; scratch disassembly is `build/investigation/event-score-native.dis`. Simply removing Eggsercize's score writes would not protect a settings extension from this routine.

The resulting budget is:

| Budget | Bytes |
| --- | ---: |
| Existing custom settings allocation | 44 |
| Repurposed numeric event scores | +204 |
| **Combined settings capacity across the two regions** | **248** |
| Compact baseline proposed in section 3, including signature/version and all six global flags | −40 |
| **Unallocated after that baseline** | **208** |
| Optional persisted Ledgedash toggle/mode, distance and threshold | −3 |
| **Remaining after that optional extension** | **205** |

The 208-byte remainder consists of the existing four-byte reserve plus the reclaimed 204 bytes. Reserve additional bytes if the extension gets its own header. The two physical ranges are **not contiguous**: do not declare one 248-byte structure starting at either range and overwrite the native fields between them. Define bounded records and explicit accessors for each region.

For the features already requested, fixed trail decay rates, fixed player colors, softer historical-hit opacity, fixed timing colors, and the Ledgedash attack/dash classification need **zero persistent bytes**. Their code/constants and runtime objects have separate budgets. The saved global preferences and OSD color enum fit in the 40-byte compact baseline; event-local Ledgedash menu choices can remain unsaved. Even saving the three-byte optional Ledgedash preferences fits in the original 44-byte allocation. Therefore we do not need to sacrifice high scores to deliver the current plan.

What 204 bytes can hold depends on the representation. Before extension metadata, it can represent 1,632 Boolean flags, 204 one-byte values, 102 two-byte values, or 51 four-byte values. More practical examples, each considered separately:

| Possible use | Saved size |
| --- | ---: |
| Nineteen one-byte OSD color-enum choices | 19 bytes, versus eight when tightly packed |
| Nineteen custom RGB title colors | 57 bytes, plus enabled state already held in the existing mask |
| Nineteen custom RGBA title colors | 76 bytes, plus the existing enable mask |
| Two user-adjustable trail alpha values | 2 bytes |
| Hold and fade values for three saved trail presets, each value one byte | 6 bytes |
| Four one-byte preferences for each of the current 28 events | 112 bytes |
| Three complete copies of the 40-byte compact baseline | 120 bytes, plus preset-management metadata |

The fixed named-color enum originally requested does not require RGB/RGBA storage. Those larger examples show the room available if customization expands. Numeric fields need explicit ranges, defaults and validation; an ordinary C enum/int is not automatically a one-byte saved value.

An illustrative combined budget is **214 bytes used out of 248**, leaving **34 bytes**: baseline 40; optional Ledgedash preferences 3; two alpha values 2; replace the eight-byte compact OSD-color table with 57 bytes of RGB values, adding 49; four one-byte preferences for each of 28 events, adding 112; and reserve eight bytes for an extension header. This is a planning example, not a list of features currently implemented or a promise that four preferences cover every event's whole menu.

The extra space can also make the serialized format easier to maintain. Byte-per-choice enums, a larger version/header, and explicitly named event preferences become affordable without squeezing every new toggle into the first flag byte. Compactness is useful, but it is not a reason to keep a difficult format once the score region is intentionally available.

Recommended implementation if Tyro drops persistent scores:

1. **Separate the Tyro save identity first**, following section 16. Work only in its new save or an explicit copied migration destination.
2. Replace the centralized score getter/setter behavior so legacy callers cannot read configuration as a score or overwrite it. Keep compatible exported function signatures where required, but use neutral results/no writes or a separate runtime-only score store.
3. Update the C paths in `eggs.c` and `slalom.c`, plus Ledgestall and `Event_ExitFunction` in `Custom Event Code - Rewrite.asm`. Remove saved-best text and persistent-record comparisons/celebrations deliberately. Returning zero alone would otherwise make every positive attempt look like a new record. Preserve the current attempt's egg count, pole count, timer, success feedback and other useful training HUDs.
4. Audit the event-selection score display and remaining native calls, including ordinary event-mode paths where reachable. Stop them from interpreting the extension as numeric scores. Preserve event-played tracking unless separately retired.
5. Handle `0x8015EEC8` and any other clear/copy paths: a records-only reset should preserve settings, while creating/deleting a whole save should initialize them intentionally. Keep adjacent played/progress fields' native behavior. Initialize on genuinely fresh/no-card saves and recognize the extension by version/signature rather than guessing from score-like bytes.
6. Add bounded, versioned extension records through the existing main-save/checksum pipeline. Validate round trips, retries/end-of-event writes, records clearing, no-card boots, migration and upstream/Tyro switching. Verify both physical bounds so no write reaches adjacent native data.

For the current feature set, keep the conservative 44-byte design as a valid option. If Tyro is definitely never going to save event high scores, reclaiming this region once, early in the separate-save format work, is a reasonable choice: it provides ample growth room and permits simpler preference storage. Choose that before shipping the first new format to avoid an unnecessary second migration.

Reusing these bytes leaves the native file's declared payload/allocation unchanged unless the card serializer is separately redesigned. It does not automatically recover memory-card blocks. It also does not increase trail history, live egg allocations, executable size limits or savestate capacity. Those require runtime/code analysis even if their saved preference is just one byte. This investigation has identified the region and concrete access/reset paths; no score removal, extension, save migration or in-game persistence test has been performed.
