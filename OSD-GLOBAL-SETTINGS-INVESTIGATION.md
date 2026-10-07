# Global OSDs, hitbox trails, and Ledgedash: investigation and implementation plan

**Running to-do list: audited October 6, 2026**

The eleven-item correction request is implemented in section 36; the subsequent nine-item round is implemented in section 37, with its follow-up Wavedash/layout/shine corrections in section 38, native text-pointer fixes in sections 39-40, and final menu grouping in section 41. The user now reports that things look stable. That is useful gameplay feedback, although the exhaustive frame/card/performance checks below are not yet documented as complete. Sections 42-44 record the audit and requested next steps; **section 45 implements the first reserved-bit event preferences**. Historical sections describe the checkout at investigation time, not necessarily current behavior or the current storage budget.

- [ ] Protect accurate imported-recording playback from the effective global Infinite Shields override while preserving its saved preference (section 15). Lab's local shield controls **already show the global override**; that part is complete.
- [ ] Decide whether Infinite Shields means full-health refill or literal never-break. The current implementation refills health; a single sufficiently damaging hit can still need a native break-boundary hook (section 15).
- [ ] Add verified shieldstun boundaries to Actionable Yellow>Green (the initial phase-2 proposal included them). Hitstun, rolls/dodges, jumpsquat and character specials remain later expansion; current coverage is ordinary attacks and landing recovery (section 8).
- [ ] Extend Act OoWait source/opportunity tracking beyond the six native history slots. Normal/autocancel/aerial/special landing, tech/getup, throw and supported special labels now work for the supported short chains; long movement chains and character-specific recovery transitions still need explicit context (section 22).
- [ ] Add elapsed **own-fighter** hitstun/shieldstun context (`hs`/`ss`) where useful, separate from the implemented `hl` prefix. Do not label an opponent's stun as the acting player's delay (section 21).
- [ ] Complete live Dolphin frame-step validation: contact-frame/last-hitlag boundaries, shine on shield/body, fastfall, L-cancel, perfect protected ledgedashes, two yellow/two green frames, overlay priority, all start/reset/criterion modes, randomized eggs, FD fallback and moving platforms. Compiled tests do not replace those gameplay checks.
- [ ] Complete real save/reload and upstream/Tyro switching on controlled GCI-folder/raw cards, including no-card/failed-save paths; measure actual allocated save blocks and free card capacity. Audit any required Dolphin per-game defaults under TYRE01 (sections 1, 16).
- [ ] Measure live heap and rendering cost in dense multi-fighter/projectile matches, including Nana, teams, reflections, slow motion and recording restores. Static buffers and DAT sizes are measured; total engine allocations/performance are not (sections 6, 14).
- [ ] **Optional:** provide an explicit copied import/conversion path for old GTME recordings/settings. New identity isolation is implemented; foreign recording import is not automatic (section 16).
- [x] **Step 7, first subset:** persist core Ledgedash choices including Tips, and Eggs-ercise choices including infinite mode, using only reserved bits. Add **Reset Event Settings** to both main menus (section 45).
- [ ] **Step 7, later expansion:** inventory/persist Training Lab and additional Ledgedash choices; add Lab's reset button. These need a separate storage decision beyond the four bits now free (section 43).
- [ ] **New requested step 8:** implement an optional **Fixed** OSD grid: selected categories reserve cells from top left across, then down; results update in place without reordering or disappearing (section 44).
- [ ] **Proposed step 9:** implement a **Practice Panel** alternative: stable compact category rows, latest results retained, optional bounded recent history and a reserved detail area (section 44).
- [ ] **Optional:** provide a Lab editor for the eighteenth packed overlay condition. Its storage slot exists, but the condition is not exposed as an eighteenth editable row (section 3).
- [ ] **Storage decision, not implemented:** use audited spare score positions, retire persistent high scores for the 204-byte extension, or create a separate Tyro preferences file if the new event inventory requires it. No score bytes have been reclaimed (sections 17, 43).

Superseded requests: Very Very Fast decay was deliberately removed, and the Falling-start reset redesign is now reverted by request. They are not pending tasks. Global hitlag prefixes, landing labels, trails, flashes, protection, menu grouping and the current Ledgedash controls have implementations.

Investigated October 4, 2026, initially against checkout `b3d6700`, with the additional filename, success-criteria, reset, and OSD investigations against `08640e8`. Steps 0-6 now have implementations: centralized V1.4.1T2 metadata, versioned outputs, the separate `TYRE01` identity, packed/migrated settings, global trails, both OSD editors, master suppression, shared flashes/shields, and the staged recovery cue. Sections 24-31 record implementation and validation. Step 6 still requires live Dolphin frame-step validation; Ledgedash is implemented in sections 35-36, and hitlag prefixes plus initial recovery-source labels are implemented in section 36. The running to-do list above records remaining work. The README contains the running Tyro-specific changelog.

The global features and the first saved event preferences fit inside the existing **44-byte format-3 settings record**. Global additions originally left 29 bits; section 45 assigns **25** to event preferences and initialization, leaving **four bits reserved**. No high-score storage has been reclaimed. Broader event persistence may need the audited **204-byte score region** or a separate preferences file; section 17 explains that conditional budget. Tyro's stable **TYRE01** identity is implemented. Actionable cues cover the documented ordinary attack/landing states; accurate coverage of further states remains separate research.

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

| Current allocation (format 3) | Bytes |
| --- | ---: |
| Existing enable mask | 4 |
| Existing position, page, legacy behavior, advance/decrement, D-pad U/D, D-pad L/R, input display | 7 |
| Version and global flags in formerly unused `0x1F2E` | 1 |
| Packed overlays, including phase-2 slot | 18 |
| Packed OSD choices | 8 |
| Format signature | 2 |
| Extra flags byte (three global flags, initialization, reset delay, Tips; bit 7 reserved) | 1 |
| Packed event preference bytes (Ledgedash/Eggs-ercise; three high bits of the last byte reserved) | 3 |
| **Total** | **44** |

Current byte placement (format 3):

| Offsets | Current purpose |
| --- | --- |
| `0x1F24–0x1F2D` and `0x1F2F` | Preserve current fields and their offsets. |
| `0x1F2E` | Two version bits and six flag bits. |
| `0x1F30–0x1F41` | Eighteen packed overlay bytes. |
| `0x1F42–0x1F49` | Eight packed OSD-choice bytes. |
| `0x1F4A–0x1F4B` | Format signature. |
| `0x1F4C` | Run Turnaround bit 0, protection bit 1, CPU OSDs Off bit 2; event initialization bit 3, Ledgedash delay bits 4-5, Tips bit 6; bit 7 reserved. |
| `0x1F4D` | Ledgedash Starting Position bits 0-2, Reset bits 3-5, Success Criteria bits 6-7. |
| `0x1F4E` | Eggs-ercise damage threshold, 0-199. |
| `0x1F4F` | Eggs-ercise scale bits 0-1, velocity bit 2, collision bit 3, infinite mode bit 4; bits 5-7 reserved. |

Six flag bits cover TURN OSDS OFF, Very Fast trails, Instant trails, missed-L-cancel flash, two-frame yellow/green recovery cue, and Infinite Shields. No flag bits remain free in that first flags byte. After the additional globals and section 45's event preferences, **byte 40 bit 7 plus byte 43 bits 5-7 remain: four reserved bits total**. The flags plus version occupy one byte as a group. Use explicit masks/byte packing; an ordinary C enum can occupy four bytes, and C bitfield layout should not define a serialized format. Very Very Fast was removed and needs no allocation.

A more direct palette indexed by all 32 bitfield IDs costs 12 bytes instead of eight. That alternative required the original four-byte reserve; after allocating Run Turnaround, it would overlap the extra flag and require a revised layout or extension. A three-bit palette allows eight values total; expanding beyond that would require a format change or additional storage.

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

**8. Phase 2: yellow on the final two blocked frames, green on two recovered frames**

Yellow already exists in the overlay palette. What is new is the condition, not the RGB value. The accepted step-6 behavior is **yellow on the final two unfrozen simulated frames before supported recovery, followed by a two-frame green completion pulse**. The pulse can confirm recovery even if the player immediately starts the next action; it does not label that new action's startup as actionable.

This is more difficult than identifying the first actionable frame. The current `OVERLAY_ACTIONABLE` and `CheckIASA` helpers evaluate present state; seeing a transition on the next frame cannot retroactively flash the previous frame. End-of-animation is not a universal answer because attacks can become interruptible early, landing/shield/hitstun have their own timers, specials have character-specific cancel paths, and hitlag/animation rates affect the boundary. Some states permit a subset of actions before others, so “prevents inputs” needs a defined scope.

Start with a small verified set: normal landing, L-canceled and missed aerial landing, special landing, shieldstun, and selected ordinary attacks with understood IASA boundaries. Research each transition and account for callback order and displayed-frame indexing. Expand to hitstun, dodges, rolls, jumpsquat and specials only as their boundaries are proven. Do not advertise universal coverage while relying on a generic animation-length heuristic.

Count two yellow frames and two green frames in simulation time, with intentional pause/frame-advance and hitlag behavior. Validate by frame stepping and trying inputs on the flash frame and the next frame. Existing comments document a stale IASA flag and a spotdodge exception; those are concrete reasons to test individual states.

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
| 6 | Implement the staged two-frame yellow/two-frame green recovery detector, with documented state coverage and global controls; add Run Turnaround flash. | PowerPC tests pass; live frame stepping must confirm the final two blocked/first two recovered frames for each supported state and priority over missed-L-cancel red. |

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

**Reset behavior needs a criterion-aware path.** Current completed-attempt delays are 120/60/30/1 updates for Slow/Normal/Fast/Instant, and the timer starts after the ledgedash becomes actionable, even if it has no GALINT. Instant can reset before an attack's startup reaches the egg. Resolve success according to the new Success Criteria menu before starting the reset delay: immediate landing/GALINT verdicts can resolve promptly, while protected-action and egg modes need their own opportunity window. An egg's first damage is not necessarily a pop when a damage threshold is enabled. With automatic Reset set to None, leave manual control available and provide target replacement through a reset/reposition action. Separate approach-to-ledge behavior from an actual attempt, and choose bounded target/recovery timeouts through playtesting. Sections 19 and 20 describe this redesign.

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
| Success Criteria: four modes | 0 | 2 bits |
| Egg distance, if saved as an integer/preset | 0 | 1 byte, or a few bits for presets |
| Optional damage threshold, integer 0-199 | 0 | 1 byte |

The toggle, target mode, optional reset preference, and new four-mode success criterion use six bits and fit into one new preference byte. Saving distance and threshold as well still totals three bytes, using three of the four reserved bytes and leaving one. This is an optional extension to the original design, not a claim that existing event settings already persist. It needs a versioned field/default definition and should be chosen before shipping the new format. A fixed timeout costs no saved bytes; a saved timeout preset could use the final byte, while a raw two-byte frame count would exceed that remaining byte.

PowerPC compiler layout checks produced these exact sizes: `LedgedashData` **72 bytes**, fixed event allocation **512 bytes**, SDK `ItemData` **4,044 bytes**, `SpawnItem` **76 bytes**, and the existing `LdshHitlogData` **3,844 bytes**. There are 440 unused bytes inside the already allocated event block; small target bookkeeping and a 60-byte callback copy can fit there without increasing that allocation. Add a compile-time boundary assertion. One live egg also requires its native item object, GOBJ, model joints and other engine allocations; 4,044 bytes is the SDK data structure size, not the total live heap cost. Measure the full target cost and hidden broken-object overlap in the running game. `SpawnItem` is a temporary construction description, not another long-lived per-egg save record.

Recommended sequence:

1. Update Attack / Dash classification and legend; verify dash, grab, attack, and existing landing/GALINT categories.
2. Add the independent protection highlight, integrate overlay priority, and frame-step the last protected/first vulnerable frames.
3. Prototype a single stationary Ground egg on Final Destination at initial setup and both ledge resets. Verify it is hittable immediately after setup and that callback cleanup is reliable.
4. Add the Success Criteria menu and shared attempt-resolution/reset logic, including the separate approach phase and egg-pop/timeout handling; test every criterion, delay, start position, side swap, retry, event exit, failed creation, and repeated hit.
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

The original collision was real at the identity level. Before step 0, this checkout's [build.sh](build.sh) wrote `GTME01`. Reading the legacy `TM-CE.iso` header also produced `GTME01`, with title Training Mode Community Edition. The configured upstream is AlexanderHarrison/TrainingMode-CommunityEdition; its [build script](https://github.com/AlexanderHarrison/TrainingMode-CommunityEdition/blob/master/build.sh) uses the same ID. Settings use the same custom offsets and native main-save machinery. A different ISO filename, menu version string, or banner does not separate their card settings. Step 0 now writes `TYRE01` from root-level `version.h`, without rewriting the legacy ISO/save.

GameCube save identity comprises the four-character game code, two-character maker code, and internal filename. Dolphin's [save-identity implementation](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/GCMemcard/GCMemcardUtils.cpp) checks these fields. The native main filename identified earlier is `SuperSmashBros0110290334`. Keeping that filename is compatible with separate saves when their game codes differ; changing the displayed card caption alone does not achieve isolation.

Dolphin's [GCI-folder implementation](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/GCMemcard/GCMemcardDirectory.cpp) also uses the first four characters for current-game filtering. Choose a new stable Tyro six-character disc ID with a **different game code**, not just a different maker suffix or revision byte. Keep an appropriate region code and maker code, and check the proposed ID/prefix against existing games/mods before finalizing it. Keep this ID stable across normal Tyro releases; use save-format versions and visible release names for subsequent updates.

Recommended approach: give Tyro its own disc/save identity and a versioned output filename following **`TM-Tyro-V{base-version}T{Tyro-release}.iso`**, for example **`TM-Tyro-V1.4.1T1.iso`**, using the normal card machinery to create a separate main save. The filename changes with releases; the Tyro disc/save ID stays stable across them. Both versions can use the same physical or virtual memory card; they need distinct files, not necessarily distinct card images. This adds another native save file and its block allocation on the card, not another allocation inside the 44-byte settings record. Its full on-card size/free capacity still needs measurement. Section 18 describes the filename/build plan.

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

Before shipping the new format, test a card containing only the legacy save, only Tyro's save, and both; run upstream, Tyro, upstream again, and confirm settings/scores/counters stay separate. Cover Dolphin GCI-folder and raw-card modes, supported Dolphin forks, and hardware if distributed for console use. Exercise insufficient card space, interrupted save, no-card boot, explicit legacy copy, and recording import. On controlled test cards, compare the legacy file before and after Tyro boots/saves. The identity/build changes are now implemented and artifact-checked in step 0; runtime save switching and any future migration still need Dolphin/hardware validation.

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

**18. Versioned output filename and release bookkeeping**

Use the requested pattern **`TM-Tyro-V{base-version}T{Tyro-release}.iso`**. The current T1 version becomes **`TM-Tyro-V1.4.1T1.iso`**; T2 on the same base becomes **`TM-Tyro-V1.4.1T2.iso`**. Do not insert the space used in the in-game display string into the filename. Retain the exact uppercase `V` and `T` convention.

At the initial investigation, the version definitions in `src/events.h` were `TM-Tyro v1.4.1 T1` and `TrMo Tyro Edition v1.4.1 T1`. Step 0 has moved them to root-level [version.h](version.h), with the single editable value `TM_VERSION` set to `V1.4.1T2`. The C labels derive from that header; the shared build helper reads its version/name/ID and derives quoted ISO/release paths, titles, assembly identity constants and the save caption. All ISO operations and packaged patchers use those derived values. `./build.sh --version` displays the current version, game ID and output filename without building. A metadata change forces a full rebuild even when a partial build was requested.

Keep the new six-character Tyro disc ID stable between T1/T2/etc. A versioned host filename identifies the build and gives Dolphin a distinct path/cache entry; the stable embedded ID identifies the Tyro game/save namespace. Creating a new save ID for every filename/version would needlessly fragment settings and playtime. Test two versioned Tyro ISOs sharing the same Tyro save and an upstream ISO retaining its separate save. Save-format versions must still handle Tyro upgrades/downgrades intentionally.

The [README](README.md) has a Tyro-only changelog, newest version first. T1 entries were checked against the Tyro commits: Eggsercize trails (`9c40643`), Very Fast/Instant presets in both events (`ab38d33`), Tyro version labels, banners and corrected banner colors, and the development-policy change. T2 is explicitly unreleased and now records the implemented version/identity/build work as well as research/documentation. Add a bullet for each implemented Tyro change under its matching version in the same change that introduces it; keep proposed gameplay behavior in this plan until implemented.

**19. Ledgedash Success Criteria menu and HUD**

Add a four-value **Success Criteria** menu, with **GALINT** as the default. These are event-local choices unless we elect to save Ledgedash preferences. All four values require two bits; they fit in the previously proposed preference byte without increasing its three-byte total when distance and threshold are also saved.

| Menu choice | Planned success condition | Suggested success-panel label |
| --- | --- | --- |
| GALINT | Finish the ledge-to-stage attempt with at least one grounded actionable frame of remaining ledge intangibility. Preserve the current default calculation while validating its frame boundary. | `Success: GALINT` |
| Waveland | After grabbing/releasing the ledge and airdodging, land on a valid stage floor/platform. No remaining GALINT is required. Define success on the confirmed landing rather than requiring an attack afterward. | `Success: Waveland` |
| Attack / Dash in GALINT | Complete the landing and begin a qualifying grounded Attack / Dash action while ledge intangibility remains. Use the shared Attack / Dash classifier introduced for the overlay, including its existing grab classifications. | `Success: Act GALINT` |
| Pop Egg | Complete the ledge-to-stage landing and actually pop the current target egg, with the pop-causing hit during GALINT under the planning assumption below. | `Success: Pop Egg` |

The planning assumption for the hardest Pop Egg mode is **pop during GALINT**, matching the increasing challenge of the other modes. A user-selected alternative can permit a later pop within a short post-landing opportunity window; if chosen, name/describe that window and do not claim the hit was invincible. Determine eligibility at the damage collision that crosses the pop threshold, not when the hidden broken egg's retirement animation eventually finishes.

The current implementation in `Ledgedash_HUDThink`, around lines 349-402, marks an attempt finished after release when landing becomes actionable, a landing state appears in recent history, or Wait is entered. Success is then simply `hurt.intang_frames.ledge > 0`. It does **not** require `is_airdodge` for the default verdict, so normal landing/NIL paths can qualify today. Preserve that behavior for the existing GALINT mode unless intentionally tightening it; the new Waveland mode should explicitly require airdodge plus valid ground contact. Distinguish remaining GALINT from general invincibility: a bright protection overlay alone is not proof a ledgedash earned GALINT.

Implement one attempt lifecycle: **Approach → On Ledge → Released → Landed → Await Criterion, if needed → Resolved → Reset Delay**. Track landing completion and outcome resolution separately; the current single `is_finished` flag cannot represent a landed attempt still waiting for a protected action or egg pop. A result resolver should update numerator/denominator and play outcome audio exactly once, then apply reset/side-swap behavior using the selected criterion. `Swap on Success` must use this final verdict. Do not start Instant reset as soon as landing becomes actionable in the harder modes.

A protected-action mode can resolve immediately on a qualifying action and fail when its remaining GALINT opportunity expires. Egg mode waits for a verified pop or its relevant deadline. A damaged but unpopped egg is not success. If Pop Egg is selected while Egg Target is Off, enable Ground targeting and make that change visible in the menu; never leave the player with an impossible target criterion. Use the same verified Ground/Platform placement and Final Destination fallback described in section 13. Item creation/placement failure should display an unavailable-target explanation and avoid recording a player failure caused by that setup problem.

Keep the ordinary Angle and GALINT panels available. Replace the fixed `Success Rate` heading with the criterion-aware label and retain the count/percentage below, for example `Success: GALINT` with `12/20 (60.0%)`. If the header does not fit the current panel, add a short criterion row or adjust the panel deliberately rather than squeezing everything into the percentage string. `HUD_DrawInfoPanel` uses a fixed-width label area, and the current three result buffers are only 24 bytes each; use bounded formatting and verify large counts and all labels visually. With a pending criterion, show a pending marker rather than a premature failure percentage.

When changing Success Criteria, reset its session numerator/denominator and pending attempt, or maintain separate per-criterion session counters. Recommended first implementation: reset those session statistics with an explicit menu description so results from different criteria are never mixed under one label. Ledge refreshes during setup should not repeatedly count attempts. Count a committed attempt once after ledge release; approach/setup failures can reset without polluting the ledgedash success rate. Preserve manual reset and Reset=None behavior; manual cancellation should have a documented treatment rather than silently counting as a completed attempt.

Validate all four criteria with positive/negative GALINT, direct actions on the first actionable frame, grabs/dashes, ordinary landing versus air-dodge landing, NIL, egg damage below threshold, multiple-hit pops, the last protected frame, hitlag, all reset delays and side modes, criteria changes, and savestate restore. Snapshot/reset pending verdict and target identity coherently on restores so stale item pointers or later callbacks cannot count a second result.

**20. Falling-start reset investigation and proposed fix**

The code supports the reported possibility of a premature reset, but it does not contain a special faster countdown simply because Starting Position is Falling. The important difference is which path declares the attempt failed or complete.

Current behavior in `src/ledgedash.c`:

| Path | Current condition/timing |
| --- | --- |
| Falling placement, around line 925 | Spawn five units outward from the chosen ledge and 20 units above it; zero self velocity, then enter Fall. Initialize attempt flags and clear the reset timer. |
| Ledge placement | Enter CliffWait and apply initial ledge intangibility; ordinary ledge/drop attempts become eligible for completion tracking. |
| Completed attempt | `is_finished` starts a 120/60/30/1-update delay for Slow/Normal/Fast/Instant, whether the completed landing had GALINT or not. |
| Failure | Death, airdodge at state frame 9 or later, a normal ledge option, or a particular grounded-after-release condition starts 60/20/1/1 updates. |
| Reset=None | Returns before all automatic-reset logic. |

In particular, **`missed_airdodge` is not gated on having grabbed or released the ledge**. A player using an airdodge while approaching the ledge from the Falling start can enter the failure path at state frame 9, even while still trying to get into position. Fast and Instant allow only one further reset-countdown update; Normal allows 20 rather than the 60 used after a completed attempt. The native airdodge has not necessarily finished at frame 9. This is a concrete plausible cause of the reported interruption, not an in-game reproduction of the specific attempt.

A second ordering detail matters: `Event_Think` calls **ResetThink before HUDThink**. Failure checks can run before HUDThink observes a ledge grab or landing completion in that update. For example, an airdodge already at frame 9 on the update it reaches the floor can schedule failure before completion classification gets to handle it. Later HUD work does not explicitly cancel that pending countdown. Fix the ordering/lifecycle rather than adding an arbitrary sleep only to Falling mode.

There is **no automatic reset condition for plain Fall alone** in this function. If a hands-off Falling start resets while the fighter is still alive in Fall, the inspected conditions do not fully explain that observation. Reproduce it and record the actual reset reason/state before deciding a fix. The initial 20-unit drop and character-specific motion may also feel different from a player already placed on the ledge.

Recommended changes:

1. Store the resolved start position, including Random's actual result, and enter an explicit Approach phase for Falling/Stage/Respawn Platform starts. Repositioning clears every pending outcome/timer and target context.
2. Observe native state/contact transitions before classifying outcome or applying reset checks. Enter the actual attempt phase only after the ledge has been grabbed and released; setup/recovery inputs should not trigger the ledgedash-only airdodge failure predicate.
3. During Approach, permit falling, drifting and recovery/airdodge as appropriate. Reset on genuine death or a tested, bounded recovery timeout; do not declare an airdodge failed solely on frame 9. Choose recovery limits across slow/floaty characters and stages, rather than treating the short failure delay as time allowed to reach the ledge.
4. During a committed attempt, first accept actual stage landing and resolve/wait according to Success Criteria. Only declare a missed airdodge when landing/recovery is genuinely impossible or a tested attempt deadline expires. Preserve deliberate failure handling for unwanted normal ledge options.
5. Start the selected reset delay **after** outcome resolution. Keep setup timeout, criterion opportunity and post-result delay as separate timers. Specify pause/frame-advance/hitlag handling for each clock.

Before implementation, capture a short runtime trace containing resolved start mode, current/previous state, state frame, air/ground status, position relative to the ledge, ledgegrab/release/finished flags, pending reset timer, and the exact failure reason. Frame-step a plain Falling start and approaches using fastfall, drift, jump and airdodge under every delay. Compare with Ledge, Stage, Respawn Platform and Random. Verify that actual failed attempts still reset and that success counters/audio/side swaps happen only once. This investigation has traced the relevant source conditions; no Dolphin reproduction or reset fix has been performed.

**21. Show hitlag/blocked context before an OSD's actionable frame**

This is feasible, but the label must describe the right fighter and type of delay. When your shine hits an opponent or their shield, **your fighter experiences hitlag**; the opponent may then experience hitstun or shieldstun. Those opponent timers are not the reason your own shine/fastfall clock pauses. Recommended formatting is **`3hl -> Frame 1`** for three hitlag updates, and **`3hs -> Frame 1`** only when three actual hitstun updates on the acting fighter are relevant. Use `ss` for shieldstun if supported. The user's desired compact context-plus-frame presentation is retained; the abbreviations distinguish what actually delayed the action.

Current source paths explain why the pause is absent:

- `StatesAndTimers/IncFrameCounts.asm`, hook `0x8006AB78`, deliberately skips the TM state-frame increment when the fighter's freeze bit is set. This removes hitlag from the state-time count.
- Fox/Falco's Act OoShine in `Generic Per Frame.asm`, around lines 201-253, displays `TM.state_frame` from shine loop states `0x169`/`0x16E` when its jump-input test passes. It does not retain the hitlag duration before that loop. It observes the qualifying input; validate the actual native jump-cancel transition when adapting it so a press that cannot execute is not presented as an executed action.
- Fastfall's counter increments at `0x8007D550` and displays at `0x8007D57C`. Native DOL disassembly confirms this sits in the fastfall test, after rejecting an already-fastfalling fighter or non-descending vertical velocity. It is a count of eligible descending checks, not total elapsed time since jumping or shining. The native physics/input callbacks are suppressed by the freeze path during hitlag.
- The six-slot previous-state history stores state IDs and these non-frozen durations. It cannot recover discarded hitlag afterward just by summing its existing frame fields.

The native [fighter update/freeze paths](https://github.com/doldecomp/melee/blob/master/src/melee/ft/fighter.c) corroborate the suppression of normal callbacks during freeze. The local SDK distinguishes `flags.freeze`, `flags.hitlag`, `flags.hitlag_victim`, and the current floating-point `dmg.hitlag_frames` timer. Do not assume freeze always means attack hitlag; separate environment/debug freeze, and do not use the current remaining timer as the total elapsed duration once it has reached zero.

Add a small **runtime timing context per fighter/subfighter**, owned by the shared service/native hooks. Capture state transitions and relevant contact/freeze episodes, count each simulation update once, and retain the completed hitlag duration through the shine startup→loop or airborne sequence that owns the OSD. Accumulate multiple relevant hitlag runs if needed. The L-cancel event already logs hitlag into a 32-bit history (`lcancel.c`, around lines 339-343), providing a local example; that event-specific history is not a global solution and has a fixed horizon.

Keep three meanings separate: native action-state time, relevant hitlag/hitstun/shieldstun duration, and elapsed **actionable opportunities**. Continue using Frame 1/2/3/etc. for the applicable opportunity counter. For shine, its state-specific jump-cancel availability matters; for fastfall, descending eligibility matters. A universal IASA flag cannot replace those different rules. Do not call unavoidable startup or a hitlag freeze a player mistake. The earlier cyan/green/white/red timing palette should color the actionable frame number; color the blocked-context prefix independently.

Associate the prefix with a defined episode rather than every earlier freeze in the match. For Act OoShine, retain contact hitlag from that shine's startup/loop through its actual exit. For Fastfall, retain relevant interruption from the current airborne timing episode through the fastfall action, including a shine transition where appropriate; exclude unrelated old contacts after landing, death, reset or a new episode. Decide how to present multiple reasons: `3hl+8hs -> Frame 1` is possible within the message format, while zero context can retain the short `Frame 1`. A total-elapsed number could be a later display option, but these two displayed numbers should not be added together unless their interval definitions actually coincide.

No new persistent bytes are required for always-on context formatting. Runtime counters must be cleared or restored with savestates and recording/frame-decrement paths. Appending fields to FighterData requires matching C/assembly size/allocation changes and a recording-format audit; a bounded side table avoids silently changing existing fighter layouts but still needs lifetime and restore handling. Measure/compile the chosen context structure before claiming an exact RAM cost.

Test Fox and Falco shine against shield and an unshielded opponent, no-contact shine, multiple hits, electric hitlag, zero-hitlag hits, immediate/delayed jump cancels, fastfall before/after contact, true victim hitstun, shieldstun, Nana, reset/stock loss and frame advance. Sample at a consistent native phase: a later event callback can miss a one-update flag transition. Verify contact-frame and last-freeze-frame boundaries against frame stepping, and prevent queued messages from picking up a prefix from a later action.

**22. Act OoWait: include landing paths and label the recovery source**

Ordinary Landing is **already considered** in both `Generic Per Frame.asm` and the standalone Act OoWait routine at `0x8000551C`. The generic search accepts Wait or Landing, skipping intermediate WalkSlow/WalkMiddle/WalkFast/Turn/Squat states. It then suppresses display while the current state remains in its non-action list. That does not mean all forms of landing trigger correctly.

The standalone routine searches only six previous states. A found Wait uses a base of +1; a found Landing is called **Act OoAutocancel** and uses a hardcoded base of **−3**. It adds recorded state durations, subtracts pseudo-Wait and unavoidable Turn frames, suppresses results over 13 frames, then whitelists what came before the found state. Its whitelist includes aerial-landing states, throws, tech/getup ranges, LandingFallSpecial, selected specials, and aerial attacks. A normal jump→Landing with no aerial can be rejected because the state before Landing is not whitelisted. LandingAir*/LandingFallSpecial are not themselves generic-search anchors, so direct exits without the expected intermediate Wait can also be missed. These conditions explain the incomplete coverage in the source; reproduce each actual native transition before promising which case caused the user's observation.

Replace the backwards-search-only decision with a recorded **recovery context** captured when the fighter enters and leaves relevant states. Track the originating recovery kind, effective unavoidable lag, first actionable opportunity, intervening eligible movement, and whether an action has already emitted the message. Preserve that context through same-update Wait transitions and the short walk/turn/crouch sequence, then retire it on a new move, unrelated damage, ledge/death/reset or a deliberate maximum-age policy.

| Recovery source | Planned coverage/label |
| --- | --- |
| Normal Landing | `Landing`; use the fighter's actual normal-landing rules/attribute instead of assuming the hardcoded four-frame baseline. |
| Aerial landing | `L-cancel` when a verified successful cancel shortened the lag; otherwise `Aerial landing` or `Missed L-cancel`. Snapshot the result on landing, before later trigger input overwrites the relevant timer. |
| LandingFallSpecial | `Waveland` after an air dodge, or `Special landing` for other sources; do not call every special landing a wavedash. |
| Tech/getup | Distinguish `Tech`, `Tech roll`, `Getup`, `Getup attack`, and relevant missed-tech recoveries where their actual actionable boundary is verified. |
| Throw/supported special ending | `Throw`, `Laser`, `Needles`, etc., retaining the useful existing coverage. |
| Idle without known recovery context | `Wait` or the existing generic label; do not invent a preceding lag state. |

For aerial landing, use the actual engine/cancel-result path where available, not merely the state name or a late read of the trigger timer. Successful/missed L-cancel labels should share the landing-entry snapshot used by the planned global missed-L-cancel flash. Include direct attacks/jumps/dashes/shield actions on the first legal recovery frame in the coverage audit, and define whether each counts as acting for this OSD. Buffered movement that the existing OSD deliberately treats as intermediate should retain coherent opportunity accounting rather than generating duplicates.

Suggested output is `Act OoWait` with **`Landing -> Frame 1`**, **`L-cancel -> Frame 2`**, or **`Tech roll -> Frame 1`**. When blocked context also applies, use at most three lines: title, source, then `3hl -> Frame 1`. Store source/timing metadata separately from rendered strings, so the chosen title color and actionable-frame color can be applied independently. Respect the existing three-line message limit and verify the longest labels at each OSD position.

The current 13-frame cutoff is a display policy, not a storage limit. Retain it only if it is desired; otherwise remove/extend it deliberately and keep counters bounded. A six-state history or a current state duration cannot by itself identify the precise actionable boundary for every special move. Start with normal/aerial/special landing and tech/getup coverage, using native transitions and attributes, then extend verified special cases. This same recovery context can support the phase-2 last-blocked-frame overlay, but observing an exit after the fact still does not predict its previous final frame in advance.

Fixed source labels and timing formatting need **zero persistent bytes**. If a separate global display toggle is later desired, it costs one bit; the main six-flag byte is already full, so place it in reserved/extension storage. Audit deduplication IDs: Act OoWait currently uses message kind 5, which overlaps other OSD callers, so associate the canonical OSD setting and source separately from the replacement-queue kind. Validate title colors, master suppression, simultaneous messages, actionable frame colors, direct/buffered transitions, >13-frame delays, different characters, hitlag and restore.

**23. Updated implementation sequence for these additions**

1. Centralize Tyro version metadata and implement the versioned filename; establish the separate stable save identity and migration policy before serialized changes. Maintain the README version bullets as changes land.
2. Add the Ledgedash Attack / Dash classifier and independent protection highlight. Instrument/reproduce Falling-start resets, then introduce approach/attempt/resolution phases and verify existing GALINT behavior.
3. Add Success Criteria and the criterion-aware success panel. Implement Waveland and protected-action modes against verified landing/action boundaries before integrating egg-pop mode.
4. Add the stationary Ground egg, criterion-aware target lifetime/deadline, damage/pop callback and reset behavior; then add actual-platform support and optional tuning. Test all start/reset/criterion combinations.
5. Add shared per-fighter timing/recovery context. First verify shine/fastfall hitlag prefixes and normal/aerial landing labels; expand tech/getup/special cases, then use this metadata in the planned OSD palette/title/master work.
6. Add the remaining shared global trails/flashes/shields and staged last-blocked-frame predictor, applying the callback/restore/priority policies already described. Validate save round trips separately from runtime timing behavior.

The new two-bit success criterion does not increase the three-byte optional Ledgedash preference budget: compact globals plus those preferences still use **43 of 44 bytes**, leaving one byte. Always-on hitlag/source labels and runtime approach/outcome tracking consume no memory-card settings bytes. Additional saved display toggles or timeout controls use that remaining reserve or the proposed score extension. The versioned filename/identity work is now implemented in step 0; new gameplay criteria, reset fixes, OSD tracking and serialized settings changes remain planned.

**24. Step 0 implementation: V1.4.1T2 and TYRE01**

Root [version.h](version.h) is the obvious editable version location. `TM_VERSION` is `V1.4.1T2`; `TM_GAME_ID` is the stable `TYRE01`; `TM_GAME_NAME` is Training Mode Tyro Edition. Only the version changes for ordinary Tyro releases. `TYRE` is a Tyro mnemonic with the USA `E` region character, distinct from upstream `GTME`. This is a custom ID, not an assigned retail title. Dolphin's [GameCube reader](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DiscIO/VolumeGC.cpp) supports GameCube identities that do not start with `G` and identifies the platform from the disc format.

Implemented propagation:

| Consumer | V1.4.1T2 result |
| --- | --- |
| In-game short label | `TM-Tyro V1.4.1T2` |
| Long label/crash-report version | `Training Mode Tyro Edition V1.4.1T2` |
| ISO output | `TM-Tyro-V1.4.1T2.iso` |
| Disc header | ID `TYRE01`, title `TM-Tyro V1.4.1T2`; the shorter title respects the ISO tool's length limit. |
| Banner titles | Short versioned Tyro title and full long title; source RGB5A3 artwork, maker credits and description are preserved. |
| Native memory-card caption | A 32-byte padded Tyro title followed by `Game Data V1.4.1T2`, within the existing caption bounds. |
| Caption/banner assembly checks | Both compare the generated `TYRE` constant, keeping the ISO-save banner branch under the new ID. |
| Release ZIP/folder and patchers | `TM-Tyro-V1.4.1T2`, with generated Windows/shell output-filename configuration. |
| Symbol map | `build/TYRE01.map`, also included in the release archive; the original source map is retained. |

The native main-save internal filename and serialized settings/score layouts are unchanged. The disc game code supplies the separate native card namespace. No automatic migration, retagging of a legacy save, or broadening of recording filters was added. The intentional GALE-only launcher/name-region condition was retained. No Dolphin user files or memory cards were edited.

The build stops before ISO mutation if a C module fails; version/identity changes invalidate partial-build selection so old C/assembly version labels cannot be accidentally packaged under a new filename. `clean.sh` follows current metadata and preserves legacy `TM-CE.iso`/other Tyro versions. Release staging uses `build/releases`; Windows can package through built-in PowerShell if `zip` is unavailable. The README changelog and development instructions were updated with the implemented changes.

Validation includes the full optimized C/assembly release build, emitted disc header/magic, compiled short/long labels, both generated assembly identity comparisons, caption bounds, banner artwork/credit preservation, archive integrity, and a packaged-patcher ISO round trip. Baseline SHA256 checks cover the unchanged vanilla ISO, legacy TM-CE ISO and source banner. Generated verification records/logs are in ignored `build/investigation`/build scratch. Runtime first-save creation and upstream→Tyro→upstream switching on GCI-folder/raw cards have not yet been tested in Dolphin; those remain the release-validation tasks listed in section 16.


**25. Step 1 implementation: packed settings, validation and migration**

Implemented October 5, 2026. The format in section 3 is now the actual 44-byte record: unchanged prefix offsets, six global flag bits/two version bits, 18 HMN/CPU nibble pairs, nineteen stable three-bit OSD choice slots, the `TY` signature, and four preserved reserved bytes. The sparse-ID map is explicit and separate from message kinds. No score storage or neighboring native fields were reclaimed.

[settings.c](src/settings.c) is the portable codec. [settings_game.c](src/settings_game.c) supplies one runtime service through `EventVars.settings` and two appended native function exports, keeping existing export indices unchanged. C event modules and native OSD/page/position/recommended-mask hooks use the accessors. Native call wrappers preserve general/volatile floating-point registers, LR, CR, CTR and XER; their register-index arguments are numeric. Compiler assertions verify the SDK record boundary. Lab persists all seventeen existing overlay conditions for both actors, instead of truncating at eight enabled conditions each, while retaining the eighteenth future slot.

On the first settings access after legacy Tyro data becomes available, migration snapshots both old lists before overwriting them. Valid controls and the complete enable mask are retained; invalid values/pairs are sanitized, duplicate valid overlay groups retain the last saved value, enabled OSD choices become White, and new global flags default Off. The signature is written last. Repeated loads are idempotent. Native dirty/save/checksum machinery handles persistence; the codec does not open, retag or copy upstream files. Fresh/no-card Tyro initialization clears exactly 44 bytes before lazy migration, so stale RAM cannot masquerade as saved overlays or a signature.

Ownership requires the exact running game/maker ID from `version.h`. Foreign identities and unsupported signed versions leave the entire persisted settings record unchanged and use defaults in a separate runtime buffer; menu changes can work temporarily without rewriting that record. Reserved bytes and unused palette bits survive current-format validation. Existing Boolean menus synchronize mask/palette state without discarding already-enabled title colors or unrepresented mask bits. The new color/global-effect menus and their gameplay behavior are not exposed yet. Ledgedash's frame-advance lookup reads only its low control nibble.

Partial builds compare shared C/assembly/export ABI fingerprints; a changed ABI forces all modules to rebuild, avoiding a new Lab client combined with an old eventMenu service or old native function table. The version remains V1.4.1T2 while unreleased T2 changes accumulate. The README records the implemented work and DEVELOPMENT.md documents the format, API and tests.

Seven tests execute the actual compiled big-endian PowerPC codec, bridge and native wrappers under Unicorn. Coverage includes every overlay slot, byte-straddling colors, flags/control nibbles, unknown enable bits, invalid indices/choices, deterministic random legacy records, migration ordering/duplicates/idempotence, adjacent memory canaries, reserved storage, unsupported versions, complete six-byte identity checks, dirty behavior, and GPR/FPR/CR/CTR/XER preservation. Floating-point restoration is tested with a callee that deliberately clobbers its volatile FPRs. The optimized release build is checked separately. Real native card save/reload and switching remain runtime validation tasks; no user card was edited during implementation.

Compatibility limit: the earlier step-0 `TYRE01` build was unversioned and cannot read the new packed record. Do not use that older build against a migrated Tyro save. Upstream/T1 `GTME01` remains separately identified. Future version-aware Tyro builds should retain the `TY` marker and preserve unsupported settings; this guarantee applies to this settings record, not arbitrary future changes elsewhere in the main save or recording format.


**26. Step 2 implementation: shared/global hitbox trails**

Implemented October 5, 2026. [trails.c](src/trails.c) supplies one bounded model and palette; [trails_game.c](src/trails_game.c) supplies the shared capture/render service. Duplicate Lab/Eggsercize rings, capture callbacks and GX renderers were removed. The common native match-start hook initializes the effect for ordinary matches and legacy/C events. Local menus configure the common service through an appended EventVars API pointer.

The L-button OSD menu and Lab OSD menu now expose **Hitbox Trails Very Fast** and **Hitbox Trails Instant**. Native rows 2 and 4 have an explicit settings-row mapping to the previously allocated flag bits, rather than consuming OSD-mask bits. Both preferences persist independently. Very Fast overrides Instant's overlapping current geometry, drawing the union once; enabled global profiles override local presets. Local menus display the active global state and remain usable for other presets when both global modes are Off.

Both Lab and Eggs-ercise include **Very Very Fast** between Very Fast and Instant: hold one tick, fade 50 units per tick, nominal visible ages zero through four. Existing Normal/Fast/Very Fast/Instant/Slow/no-fade semantics are retained. Current geometry uses alpha 200; historical geometry is capped at alpha 72, with fade normalized against the original 200-alpha lifetime. This implements the accepted current-versus-history opacity interpretation; there is no invented damage-based sweetspot/sourspot classification.

Trail colors use bright Red/Blue/Yellow/Green equivalents of the player's accent identity, with CPU type explicitly Gray. Team accents are respected. Main/subfighters in all six player slots are considered. Owned items use a color only when their owner matches a live fighter object; neutral or unmatched owners use Gray, avoiding dereferencing stale ownership pointers. Native reflection/ownership behavior still needs runtime coverage. The former Lab body-overlay color override was removed from trail rendering.

The shared bank holds 128 forty-byte samples: **5,120 bytes**, plus twelve bytes of bank indices/clock state and small service/GOBJ overhead. Source keys permit stationary/hitlag geometry to be refreshed instead of stacking identical translucent copies. Capture is gated by the native match-frame key and age advances through a service simulation tick, avoiding dependence on C-event-only game_timer and skipped video frames at slow speeds. Local DOL inspection verified the native key at Match offset 0x24, its pause/frame-advance gates, the pre-event match-start hook, and the scene-preparation hook used for cleanup. Scene/match/retry boundaries, common successful savestate loads, Ledgedash repositioning and detected backward time clear history. Dense matches can overwrite samples before their nominal decay lifetime; the larger ring is not unlimited history.

These changes use **two already allocated save bits**, with no format-version change, score removal or consumption of the four reserved bytes. Shared header/ABI fingerprints force a full rebuild when the new service interface changes. The version stays V1.4.1T2. The README changelog and developer guide describe the implemented controls and runtime validation cases.

Twelve tests run the compiled PowerPC settings/trail model and native accessors. New coverage checks global-row/flag separation, independent toggles, indexed native reads, profile union/priority, all palette entries/CPU behavior, preserved nominal fade endpoints, historical alpha caps, duplicate-frame capture, stationary deduplication, timeline clearing and ring bounds. The optimized C/assembly release build passes without compiler warnings. The new ISO/archive and unchanged legacy inputs are checked separately. Visual brightness, L-menu layout/click behavior, stage/callback timing, slowed/frame-advanced gameplay, Ice Climbers, reflections and recording interactions still require an in-game Dolphin pass; model tests alone cannot confirm those visuals.


**27. Step 3: global trail-row completion**

The basic global rows were introduced with the shared renderer in step 2. This follow-up finishes the trail-row portion of step 3: labels and native row IDs are declared in settings.h, native label macros are generated from the same names used by Lab, and each Lab callback changes only its selected preference before refreshing both values from the service. Both On is shown explicitly in the local trail menus, with the Very Fast single-render union explained in their descriptions. These remain two independent preferences in the existing packed flag byte; there is no new serialized allocation or format-version change.

Fourteen compiled PowerPC tests now include a saved-record round trip for Off/Off, On/Off, Off/On and On/On into a fresh service instance. Editing one row preserves the other row, existing OSD mask/title colors, overlay choices and unrelated flags. Both native indexed reads and shared setters are exercised. The optimized release build passes without compiler warnings; generated label strings and packaged assets are checked separately. This is codec/service validation, not an actual memory-card or GUI-click test in Dolphin.

The wider original step 3 also lists missed-L-cancel flash and Infinite Shields. Those are still pending: this change completes the global trail rows requested here, without claiming those gameplay services are implemented. The version remains V1.4.1T2 and the README changelog is updated.


**28. Step 4 implementation: OSD identities, title palettes and timing**

Implemented October 5, 2026. All shared-message producers in the native OSD directory carry an explicit settings identity, independently of their original message kind. C OSD producers use the same tagged metadata; untagged event-owned feedback retains its existing style. The nineteen selectable categories use their saved palette choices. Lab exposes Off, White, Red, Green, Blue, Yellow, Cyan and Magenta and changes only the selected category. The native L-menu color selector and TURN OSDS OFF control were subsequently completed in step 5 (section 29).

[osd_style.c](src/osd_style.c) decodes identities and timing roles without identifying meaning from formatted text. Integer timing arguments are copied from the actual vararg list before formatting consumes it. Original message kinds, alternate IDs and the -1 always-new convention are retained, while replacement also checks settings identity. This stops Act OoWait/Act OoHitstun and combo/ledge collisions without changing counters or measurements. The combo-end native callback completes the canonical Combo Counter in the shared C queue.

[osd_style_game.c](src/osd_style_game.c) applies palette/timing style before text rendering, after caller-specific recoloring. Existing result subtexts keep their own colors. Category Off hides queued configurable text/background without returning a NULL GOBJ; event instructions and unconfigured feedback remain under their own styling.

The clarified timing policy is relative to the producer's original best valid frame: best is Cyan, one later Green, two later White, and other measured frames Red. Counters are not rewritten to one. Most actionable measurements already use best=1; Peach's instant double jump explicitly preserves best=5, so Frame 5/6/7/8 display Cyan/Green/White/Red. Already-normalized displayed arguments are used. Non-timing quality/window/diagnostic quantities retain their established interpretation: GALINT, shield advantage, SDI counts, dashback rates, angle/hop quality, shortening windows and lockout state are not indiscriminately recolored as lateness. L-cancel includes explicit Success/Missed feedback separately from its timing line.

Wavedash's first row contains separate positioned title and frame-value subtexts. Its angle and optional hop rows remain, respecting the three-line limit and their quality colors. Font placement must be inspected visually at every OSD position; the compiled model cannot establish final readability.

Twenty tests run the compiled PowerPC codec, metadata/helper and draw-time adapter. Added checks cover metadata versus queue identities, alternate/non-replacing behavior, native tag encoding and vararg snapshots, palette choices, unchanged frame-5 measurements, best-relative colors, overrides after caller recoloring, preservation of result colors, Off visibility and event-owned text. The optimized C/assembly release build and packaged assets are checked separately. No extra permanent bytes or score storage were consumed. Runtime GUI/save and OSD timing/layout checks in Dolphin remain necessary before distribution.


**29. Step 5: both OSD editors and TURN OSDS OFF**

Implemented October 5, 2026, retaining V1.4.1T2. Both editors now choose Off, White, Red, Green, Blue, Yellow, Cyan or Magenta. The L-button menu shows each color name beside its category and previews the chosen color on that row; B advances and Z goes backward, wrapping through all eight choices. Its global trail/master rows remain independent Boolean toggles. Stick/D-pad navigation, X/Y screen-position selection and A/Start exit retain their existing functions. Long row text is horizontally scaled to fit the existing columns; verify its visual readability in Dolphin.

The native editor's cursor order is mapped explicitly to sparse setting IDs in `src/osd_editor_game.c`. B/Z are intercepted before the original random-stage-selection Boolean toggle path. Palette input changes the shared record and displayed label immediately. The native selected Boolean is updated separately, allowing its checkbox animation to detect the change and then update its row snapshot. The exit pass saves that synchronized Boolean; enabled values preserve the actual selected nonzero color. Empty/reserved rows ignore editing input, preserving custom-event and unknown enable-mask bits. The new editor functions append export slots without changing older indices.

TURN OSDS OFF appears in both editors. On suppresses tagged configurable global OSD text and backgrounds at draw time, including existing messages while paused; Off restores normal display with the same individual choices. Untagged event feedback, legacy direct event text and shared hitbox trails retain their own behavior. Message objects remain valid for callers that immediately dereference or recolor them. Messages continue their normal lifetimes while hidden, so switching back does not resurrect expired results. This is a visibility switch, not a queue/history reset.

The master toggle uses the previously reserved `TM_FLAG_OSDS_OFF` bit 0. Native row ID 6 routes to that flag, independently of OSD enable bit 6. No additional save bytes are allocated: the settings record remains 44 bytes, format version 1, and the four reserved bytes remain free. Future/foreign formats continue to use private defaults without rewriting their records.

Twenty-six compiled PowerPC tests pass. Editor coverage exercises forward/backward wrapping, native snapshot saving, reserved-row/navigation isolation, independent master/trail toggles, existing-message suppression and restoration, event-owned visibility, future-format fallback, and a serialized round trip into a fresh service instance. The optimized C/assembly release build passes without compiler warnings and produces the updated ISO/ZIP. These checks do not substitute for Dolphin verification of menu layout/clicks, checkbox animation, pause visibility or actual card I/O. Test a color choice, both global trail flags and master suppression across exit/re-entry, match/event changes, pause, save/reload and an upstream/Tyro switch.


**30. Step 6: recovery cues, global flashes/shields, and original reserve ledger**

This section records format 2 at step 6. Section 31 is the current format-3 reserve ledger.

Implemented October 5, 2026, retaining release name **V1.4.1T2** and stable identity **TYRE01**. The packed settings format is now **version 2**. The yellow window is the final **two** simulated recovery frames, followed by a **two-frame** green completion pulse. Green still confirms completion when the player immediately starts the next ordinary action; it is not a claim that the new action's startup is interruptible. New yellow recovery takes priority. Pause does not consume frames; hitlag/freezes suppress the timing color and do not advance its countdown. Restores, Ledgedash repositioning, scene/match changes, death and backwards state/frame clocks clear transient tracking.

Both the L-button menu and Lab OSD menu expose **Actionable Yellow/Green**, **Flash Run Turnaround**, **Flash Missed L-Cancel**, and **Infinite Shields**. The last two complete the deferred shared flash/shield portion of step 3. New controls start Off. B/Z toggle the native Boolean rows; Lab edits only the selected shared flag. TURN OSDS OFF suppresses message text only, so it does not disable these body overlays or shields.

| Requested case | Implementation/boundary |
| --- | --- |
| Landing without a move | `Landing` (42): normal landing-lag attribute when native landing interruption is allowed, or animation completion if earlier. |
| Autocancel landing | Also `Landing` (42); it uses the same native boundary. |
| Wavelanding / wavedashing | `LandingFallSpecial` (43): native animation endpoint and actual playback rate; ordinary landing's shorter interrupt threshold is used only if its native allow-interrupt flag is set. Air-dodge landing normally has it clear. |
| Aerial performed without landing | `AttackAirN/F/B/Hi/Lw` (65?69): the earlier of the native IASA command and animation completion. Landing switches to the appropriate landing detector. |
| Missed aerial landing lag | `LandingAirN/F/B/Hi/Lw` (70?74): actual animation endpoint/rate, retaining full lag. |
| L-cancelled aerial landing lag | The same five states; native rate scaling already represents shortened, truncated L-cancel lag. The cue does not guess by halving a counter. |
| Ordinary grounded attack recovery | Jabs, rapid-jab end, dash attacks, tilts and smashes: native IASA or animation completion. Rapid-jab start/loop, looping animations and zero-rate charge holds are excluded until ordinary recovery resumes. |
| Character exceptions | Game & Watch's custom ordinary attack/aerial/landing IDs 341?352 normalize to their equivalent categories. His non-L-cancellable custom aerial landings are not reported as missed cancels. Kirby dash attack IDs 351?352 are included; its airborne continuation has no IASA callback and uses animation completion only. |

The staged scope means broad normal-action recovery, not every input available in every state. Earlier narrow cancels (jab followups, boost grabs, aerial drift/fastfall) are not the general recovery boundary. Character specials, hitstun, shieldstun, dodge/roll/tech/getup recovery and ledge actions are not predicted yet. Shieldstun and tech/getup would be useful next additions after their native timers and input boundaries are checked. Do not add them using the animation endpoint alone.

`src/action_cues.c` provides the bounded, read-only two-frame script lookahead and pulse model. It copies the full native command snapshot including its stack, handles native timers/loops/subroutines/jumps, skips command payloads using the verified DOL sizes, and watches opcode 0x58 (Enable IASA). It never executes hitbox, sound, background-flash or other gameplay side effects. Pointer, opcode, stack and instruction-budget checks prevent unbounded traversal; unknown scripts do not produce a guessed IASA boundary. `src/action_cues_game.c` samples all six player slots and both subfighters after their simulation/event callbacks. Live animation length is read through the verified getter at `0x8006F484`, without requesting/replacing animation data.

The service wraps each fighter's existing GX callback. Immediately before drawing, yellow/green replaces all competing body-color layers, including **Lab's red missed-L-cancel overlay** and the shared red flash. All three original `ColorOverlay` structures are restored byte-for-byte after the native draw. Native/event color-animation timers and scripts are not erased. Invisibility and collision-display choices keep their own behavior. Yellow/green is visible only in its short timing window; other local overlays resume afterward.

Run Turnaround flashes red for **four** unfrozen simulation frames on entry to `TurnRun` (19), not standing turn (18), dash, run or run-brake. Re-entry triggers another flash. The shared missed-cancel flash lasts four unfrozen frames, recording the trigger-window result at the real aerial-landing entry before the timer ages; it works even when L-cancel text is Off. Timing colors have priority over both flashes. Infinite Shields refills every live main/subfighter to native full shield health before fighter processing and after event-local writes; Lab's local shield rows show whether the global override is On. Off leaves local event policy alone. This is the existing refill-style training behavior, not new immunity to a single hit that exceeds a full shield.

**Current persistent reserve ledger (logical record offsets; not raw encoded card offsets):**

| Location | Assignment | Reserved capacity left there |
| --- | --- | ---: |
| Record byte 10 / RAM `0x1F2E`, bits 0?5 | Master OSD Off, Very Fast trails, Instant trails, missed L-cancel, recovery cue, Infinite Shields | 0 bits |
| Record byte 10, bits 6?7 | Format version = 2 | 0 bits |
| Record byte 40 / RAM `0x1F4C`, bit 0 (mask `0x01`) | `TM_FLAG_RUN_TURNAROUND` (logical flag index 6), native row ID 17 | Allocated: 1 bit |
| Record byte 40, bits 1?7 (mask `0xFE`) | Unassigned; preserved by setters and validation | **7 bits** |
| Record bytes 41?43 / RAM `0x1F4D?0x1F4F` | Unassigned; preserved | **24 bits / 3 bytes** |
| **Total remaining in the original reserve** | No score reclamation or record growth | **31 bits (3 bytes + 7 bits)** |

The settings record remains **44 bytes**. Version-1 signed records migrate to version 2 by initializing only the newly allocated Run Turnaround bit Off and changing the existing version bits; palettes, controls, overlays, original six flags and the other **31 reserve bits** are retained. Unversioned Tyro records still go through the prior packed migration. Migration is idempotent and marks native card data dirty. Earlier version-aware Tyro builds reject version 2 and use private defaults without rewriting it; their settings will not be shown until returning to the new build. This does not make the old unversioned TYRE prototype safe to reuse. GTME upstream saves remain separate.

The PowerPC build reports **624 bytes** for twelve fighter tracking/draw records, plus **16 bytes** of manager/clock/live state (640 static bytes), one GOBJ/two process records and ordinary stack/code overhead. The render wrapper temporarily copies 384 bytes of native color structures on the stack; script lookahead copies 36 bytes. These are runtime costs, not card space.

Forty compiled PowerPC tests pass, including state/rate boundaries, all five cancelled/uncancelled aerial landing categories, IASA before animation end, two-yellow/two-green sequences, immediate next actions, freeze/pause/restore, subfighters, four-frame flashes, source-specific flash suppression, actual draw-callback composition/restoration, full shields in all twelve slots, native editor rows, and reserve-preserving version migration. Native DOL disassembly and the [landing implementation](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_Landing.c), [aerial landing/rate scaling](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_LandingAir.c), [attack callbacks](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_AttackAir.c), [command dispatcher](https://github.com/doldecomp/melee/blob/master/src/melee/ft/ftaction.c) and [animation completion](https://github.com/doldecomp/melee/blob/master/src/sysdolphin/baselib/aobj.c) support the staged boundaries. **Live Dolphin frame-step/input verification remains required**; synthetic adapter tests are not evidence that every fighter/animation/camera combination has been observed in-game. Validate all requested cases on Fox/Falco and another character, L-cancel red priority in Lab, G&W/Kirby exceptions, Nana, slow motion/frame advance, pause, recordings/restores, save/reload and upstream switching before calling runtime validation complete.


**31. Global invincibility/protection overlay and current reserve ledger**

Implemented October 5, 2026, retaining **V1.4.1T2** / **TYRE01**. Both OSD editors now expose **Invincibility Overlay**. Native row ID **25** is a Boolean flag row, independent of OSD enable-mask bit 25. It starts Off and applies globally to humans, CPUs and active subfighters.

The drawing service checks native effective whole-body protection at each draw, through the existing `Fighter_GetIntangibleFrames` SDK symbol (`0x8007B868`). Despite that SDK name, the native function returns a **hurt status**, not a remaining-frame count: normal, invincible or intangible. Its implementation combines script and engine protection, which covers ledge/respawn protection, dodge windows and move-granted protection without guessing from an action name. The [native collision/protection code](https://github.com/doldecomp/melee/blob/master/src/melee/ft/ftcoll.c) confirms those status fields and the engine's ledge/respawn setters. Partial limb-only protection is not presented as whole-character invincibility.

Yoshi's double jump is **armor**, not invincibility. The user's requested exception is included whenever Yoshi's native active jump-armor value (`FighterData.dmg.armor`, `0x18B4`) is positive; it is not keyed to the entire animation or inferred from an arbitrary frame table. The [native double-jump entry](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_JumpAerial.c) installs that armor. This overlay is a protection cue; it does not alter damage, knockback, armor strength or invincibility.

Colors use exactly the shared trail RGB mapping: bright Red/Blue/Yellow/Green from the player's accent (including teams), with CPU/unmatched accents Gray. Alpha is **112/255** for a translucent body tint. Actionable Yellow/Green remains highest priority; protection then takes priority over the short red diagnostic flashes and local body colors. The original native/event color structures are restored after each fighter GX call. Vulnerability removes the tint immediately; pause, master-text suppression and history clearing do not prevent the draw-time protection check. Dead fighters remain untinted, and deliberate native/event invisibility remains respected.

**Current format-3 extension ledger:**

| Logical record / RAM location | Assignment | Remaining reserve |
| --- | --- | ---: |
| Byte 10 / `0x1F2E`, bits 0-5 | Existing six global flags | 0 bits |
| Byte 10, bits 6-7 | Packed format version **3** | 0 bits |
| Byte 40 / `0x1F4C`, bit 0 (`0x01`) | Run Turnaround, logical flag 6 | Allocated |
| Byte 40 / `0x1F4C`, bit 1 (`0x02`) | Invincibility Overlay, logical flag **7**, native row **25** | Newly allocated: **1 bit** |
| Byte 40, bits 2-7 (`0xFC`) | Preserved, unassigned | **6 bits** |
| Bytes 41-43 / `0x1F4D-0x1F4F` | Preserved, unassigned | **24 bits / 3 bytes** |
| **Total available from the original reserve** | No score reclamation or record growth | **30 bits (3 bytes + 6 bits)** |

The record remains **44 bytes**. Version-2 records retain Run Turnaround and every other existing preference; only the new protection bit is initialized Off, alongside the version-tag change. Version-1 records initialize both newly assigned extension bits Off. The other **30 reserved bits** survive both migrations. Unversioned records still use the existing migration. Older version-aware builds reject format 3 and use private defaults without rewriting it; settings reappear when returning to this build. Signed version 0 remains unsupported; any later format must deliberately define its migration/tag instead of blindly incrementing the two-bit field.

Forty-four compiled PowerPC tests pass, adding palette/CPU/subfighter coverage across engine and script statuses, live pause/toggle/protection-end behavior, timing priority, unchanged native color data, move protection, Yoshi's active armor, native row save/reload and version-2 migration preserving Run Turnaround and the 30 reserve bits. The optimized release build is checked separately. Actual Dolphin validation still needs ledge/respawn/air-dodge/spot-dodge and move windows, Yoshi armor, teams/Nana, paused toggles, restore and native card save/reload; tests model these inputs rather than executing every native move.


**32. Save-creation invalid-read fix and relocated-DAT validation**

The user reported repeated Dolphin warnings after creating a save: invalid read `0x0000992C`, PC `0x807012A0`. A read-only capture of the running emulated GameCube MEM1 mapped that exact instruction to the shared settings service's game-ID comparison, not native card I/O. Live code performed an indexed byte read after GCC had combined the low-memory identity pointer and a module string into a symbol-minus-`0x80000000` address expression. The hmex/native MEX relocation path did not retain that addend; adding the pointers wrapped the read into `0x0000992C`.

The byte comparison helper is now explicitly non-inlined, keeping both addresses as runtime arguments and their subtraction outside relocatable symbol constants. The six-byte identity check, validation/migration and fallback behavior are retained. No save format, reserved-bit allocation, identity or release-version changes were needed. The current record remains format 3 / 44 bytes with **30 reserved bits**. User card files were neither opened nor edited during diagnosis; the emulator inspection was read-only.

The prior ELF tests did not exercise this failure mode. The test runner now compiles a fresh optimized production DAT and applies the actual native relocation rules before emulating its exported settings accessors. Regressions cover a fresh/no-signature save, repeated reads, settings writes, format-2 migration and foreign-identity preservation. Forty-six PowerPC tests pass, including those DAT regressions. The rebuilt release passes without warnings/errors. The user's running session has not been replaced or patched; restart emulation with the rebuilt ISO to validate the full save-creation flow. Keep the existing save rather than deleting it to work around a code-address bug.


**33. Grouped global menu, sustained red pulses and damage-weighted trails**

Implemented October 5, 2026, retaining V1.4.1T2, TYRE01, packed format **3**, the **44-byte** record and **30 reserved bits**. These changes allocate no save bytes and reclaim no event scores.

The native title and Lab entry/title now read **Global Settings - OSDs and Overlays**. The exact master label is **OVERRIDE: OSDS OFF**. Read the native grid down the left column, then the right; it has 29 physical rows, 27 named controls and two gaps:

| Physical cursor rows (zero-based) | Content |
| --- | --- |
| Left 0-14 | First fifteen OSDs, in the canonical Lab/settings order |
| Right 15-18 | Combo Counter, Grab Breakout, Ledgedash Info, Act OoHitstun |
| Right 19 | Blank gap |
| Right 20 | **OVERRIDE: OSDS OFF** |
| Right 21 | Blank gap |
| Right 22-28 | Very Fast trails, Instant trails, Pulse Missed L-Cancel, Pulse Run Turnaround, Actionable Yellow/Green, Infinite Shields, Invincibility Overlay |

`TMSettings_EditorIDs` is a display-order map over stable OSD/flag identities. New accessor field **17**, `TM_SETTING_EDITOR_ROW`, translates the native RSS physical row IDs into those bindings. L-menu initialization and exit saving use that field; the original logical-row accessor (16), color slots and global flag bits keep their meaning. Both gaps return Off, ignore saves and preserve unknown/custom-event enable bits. Their text and checkbox subtrees are hidden without hiding neighboring rows. B/Z on a gap makes no preference change. Lab places its master immediately after the nineteen OSDs and follows with the same shared-control order; its scrollable menu does not consume native grid gaps.

Missed L-cancel latches the actual landing-entry result and repeats red for the **entire uncancelled aerial landing state**; a successful cancel does not pulse. Run Turnaround repeats red only while **TurnRun** is active. Both stop on state exit, reset/restore/death or disabling the selected global preference. Opacity repeats a twelve-simulation-frame waveform: **180, 148, 116, 84, 52, 20, 20, 52, 84, 116, 148, 180**. Pause and hitlag retain the current phase. This replaces the earlier single four-frame flash; it is a Slippi-like pulse, not a claim of exact Slippi timing. Yellow/green recovery remains highest priority, followed by protection, then red diagnostics. Native/local colors are restored after drawing.

Investigation of the pre-unification trail implementation (`2ddff77`, `HitboxTrails_Color`) found **damage-dependent RGB**, with a fixed initial alpha of 200, rather than a dedicated hard/soft-hit flag. Fighter and item hitboxes expose base damage (`dmg`) and actual/staled damage (`dmg_f`); the latter is unsuitable for a stable move-strength cue. The new renderer keeps player RGB and stores damage-weighted opacity in the existing sample color's alpha byte. Damage at/below 3 uses alpha 64; damage 4-11 uses `20 * damage`; damage at/above 12 uses alpha 240. This is a bounded damage-strength visualization, not a universal sweetspot or knockback classifier.

For the user's Fox-nair example, **12 damage = current alpha 240 / starting history alpha 96**, versus **9 damage = current alpha 180 / starting history alpha 72**. Simultaneous hitboxes are sampled independently, and a damage/alpha change preserves the previous stronger sample rather than refreshing it as identical geometry. The preset's fade timing stays unchanged; history caps scale with captured strength. Fighter and projectile samples share this rule. No damage fields were added to the 40-byte sample: the bank remains 128 samples / **5,132 bytes including clock/index state**. Extremely strong hitboxes saturate the opacity cap, so different high-damage values can have the same alpha.

**Very Very Fast was removed** from Lab and Eggs-ercise. Remaining local decays are Normal, Fast, Very Fast, Instant, Slow and Off (no fading). Global Very Fast/Instant preferences retain their saved flags and priority. Local decay values are not serialized, so removing that enum entry requires rebuilding callers, but no memory-card migration. Header fingerprints force the full rebuild.

Forty-nine compiled PowerPC tests pass, including grouped native reads/writes through the actual hook macros, complete exit-save preservation, exact master label/gap visibility, pulse duration/repeat/freeze/exit behavior, strong/weak opacity and historical samples, removed-preset indexing, and relocated production DAT settings access. The optimized release is rebuilt and checked separately. Dolphin validation is still needed for title/row fit, gap navigation, the pulse's visual feel and Fox-nair/other hitbox contrast under Very Fast/Instant/slow motion/frame advance.


**35. October 6 fixes, resumed trail tuning and Ledgedash implementation**

The interrupted opacity edit was resumed and expanded to cover the user's additional runtime reports. Release name remains **V1.4.1T2**, identity **TYRE01**, packed format **3**, record size **44 bytes**, reserve **30 bits**. No new persistent field or high-score reclamation was introduced. Ledgedash options follow the existing event-local convention and reset on event reload.

**Renderer lifecycle and missing overlays.** The same native fighter GOBJ is reused across a changed spawn number. The old shared wrapper reset its tracking record, including the saved original GX callback, then saw its own wrapper still installed and failed to recover that callback. It could therefore return without calling the real renderer. The fix retains the original renderer for the same GOBJ, clears only transient state on generation changes, finds draw ownership by object rather than a changing slot value, and restores the native callback when all applicable global/local overlays are Off. A local-overlay API lets Ledgedash use the same render-time composition rather than clearing native color-animation structures. This directly addresses the reported death/spyglass/invisible-respawn failure mechanism; full Dolphin reproduction/verification remains pending.

**Missed L Cancel.** Read-only captured GameCube RAM confirms the common input-window field at offset `0xE4` contains integer **7** (`00000007`), while the old SDK declared it float. That declaration and the detector are corrected. The actual landing hook still captures the result, and a late landing-entry fallback independently latches the correct result when the hook did not populate it. Valid cancels below the window do not pulse; missing/late inputs do. The result lasts only through the corresponding aerial landing state.

The labels are now exactly **Missed L Cancel** and **Run Turnaround**. Pulse alpha repeats over **eight** unfrozen simulation frames: `180,140,100,60,20,60,100,140`; pause/hitlag hold the phase. Actionable Yellow/Green remains higher priority, then protection, then red diagnostics/local body colors. These are fixed waveform/code changes, not new saved preferences.

**Protection coverage.** The shared helper combines engine/script aggregate status, the native effective-status getter, Yoshi's positive jump-armor value, and whole-body protection represented by all current hurtboxes being protected. The [native SetAllHurtCapsules path](https://github.com/doldecomp/melee/blob/master/src/melee/ft/ftcoll.c) changes the capsules without assigning the two aggregate fields, so checking those fields alone can miss roll/move windows. Roll, spot-dodge and air-dodge coverage follows live protection rather than a whole-animation state whitelist. A lone protected limb does not label the whole character invincible. Global and local Ledgedash highlights use this same helper.

**Trail tuning.** Current alpha is now `96 + round(damage^2 * 159 / 144)`, clamped by checking damage before squaring and saturating at 255 for 12+ damage. History starts at 48 for the low end and scales to 128 for the strongest samples. Examples:

| Base damage | Current alpha | Starting history alpha |
| ---: | ---: | ---: |
| 0/1 | 96/97 | 48/49 |
| 2 | **100** | **50** |
| 3 | **106** | **53** |
| 9 | **185** | **93** |
| 12+ | **255** | **128** |

This brightens low-damage moves and the overall curve while retaining stronger-hit contrast, player RGB, 40-byte samples, the 5,132-byte bank and preset fade endpoints. Very Very Fast stays removed.

**Global title.** Native text alignment value 2 is right alignment (verified in the DOL text parser). The short title **Global Settings** now uses the same X anchor, 655, as the right column, at the existing title Y. The green position text keeps its left-side anchor, avoiding the previous long-title overlap. Lab's entry/title are shortened too.

**Ledgedash completion.** The earlier Ledgedash requests remained planning work during steps 0-6. They are now implemented:

- **Attack/Dash** uses the previous attack/grab classifier plus initial Dash, keeping its numeric color/timeline slot. Sustained Run is not automatically classified as Dash.
- **Ledge** selects Left/Right; D-pad selection still works, and reset-side swaps update the menu value.
- **Protection Highlight** brightens RGB 40% toward white and raises alpha to 230 while protected. It works independently of Color Overlays, preserves native color state through the common draw wrapper, and leaves the action-log legend palette unchanged.
- **Success Criteria** selects GALINT (default), Waveland, Attack/Dash in GALINT, or Pop Egg in GALINT. A separate **Criterion** row in the success HUD identifies the selected rule; `*` means the landed attempt is awaiting a harder criterion. Changing criterion resets session statistics and cancels/repositions the current attempt.
- **Egg Target** selects Off/Ground/Platform, with **Egg Distance** (5-80 stage units, default 20) and **Egg Pop Damage** (1-50, default 10). Pop Egg criteria visibly enables Ground targeting if it was Off.

The explicit attempt model tracks Approach, On Ledge, Released, Landed/Await Criterion and Resolved. Waveland accepts confirmed ground contact after airdodge without requiring GALINT. Default GALINT waits for landing to become actionable and retains ordinary/NIL landing eligibility. Attack/Dash waits for a qualifying grounded action while ledge intangibility remains; egg mode waits for the target's threshold-crossing collision during grounded GALINT. A resolved result updates counts/audio once. The reset countdown starts afterward; Reset=None still records results/audio without repositioning. An unresolved committed attempt has a 180-unfrozen-update limit. During Approach, airdodge/recovery inputs do not trigger the old frame-9 failure. Setup deaths reset without adding failures. Manual reposition or a regrab cancels a pending attempt without adding a new result; already resolved statistics are retained. Regrabs also refresh target health/context. Reset delays continue to use the existing success/failure delay selections.

Egg placement enumerates enabled floor segments and current world vertices, distinguishes solid ground from droppable platforms, clamps targets at least three units from segment endpoints and rejects unsafe/narrow candidates. A Platform request on Final Destination deliberately selects solid Ground fallback, shown in the HUD. Creation uses zero velocity and the verified native resting initializer `0x80288E6C`, with supporting line, collision positions and model updates initialized. It does not launch an egg and wait for it to fall. Moving support vertices update the egg position; disappeared support is reselected. Unavailable setup is reported and excluded from the player's rate.

The target is neutral, unpickable and unnudgeable, one per attempt. Partial damage is not a pop. Pop eligibility is captured at the damage collision, including an immediate action after landing or the last GALINT frame; hitlag/retirement delay cannot retroactively change it. Threshold accumulation is floating point. Damage callbacks are restored before destruction, and a scene-cleanup GOBJ removes active/retiring targets before event code unloads. Target ownership is outside the copied 512-byte event block, and an appended restore serial in EventVars forces reconciliation/recreation after savestate loads instead of trusting a saved item pointer. Ledge searches also correct the right-edge next-line test. Native item/collision timing still requires Dolphin testing.

Sixty compiled PowerPC tests pass: reused-GOBJ/death/respawn forwarding, all-Off unwrapping, local-layer restoration, L-cancel fallback/integer window, live protection/rolls/all-capsule detection, low-damage opacity, all four criteria and pending/timeout/freeze behavior, and safe ground/platform selection including Final Destination fallback. The compiled Ledgedash DAT has seventeen menu rows with all six new controls and the Attack/Dash legend. The release build and packaged assets are checked separately. Model/adapter tests are not a live gameplay reproduction; verify death/respawn in ordinary matches and Lab with globals Off, missed cancels with timing/protection toggles, actual roll/ledge/respawn windows, title placement, all criteria/side/reset modes, egg visibility/collision and native save/reload in Dolphin.


**36. October 6: all eleven requested corrections**

Version remains **V1.4.1T2**, identity **TYRE01**, record **44 bytes / format 3**. No settings bits, score bytes or new saved fields were allocated. Byte 40 bits 0/1 remain Run Turnaround/protection; **bits 2-7 plus bytes 41-43 remain free: 30 reserved bits**. New timing counters and local egg menu choices consume runtime RAM/code only.

| Request | Implemented behavior / location |
| --- | --- |
| 1. Restore Ledgedash reset behavior | `ResetThink` again runs before `HUDThink`; completed attempts use the original 120/60/30/1 delay regardless of verdict. Original death, frame-9 airdodge, ledge-action and grounded frame-12 failure checks use 60/20/1/1. Countdown no longer skips hitlag. Normal-landing completion again uses TM state time and both recent landing-history slots. Harder criteria keep their protected opportunity open; statistics/audio resolve once. This supersedes the section-20 Falling-start redesign, including its approach exemption. |
| 2. Egg submenu and randomness | Main menu has **Egg Targets**. Inside: Enable Eggs, Egg Target (Ground/Platform/Random), fixed Egg Distance, Randomize Distance, inclusive Min/Max Distance, and Egg Pop Damage. Each reset samples target/distance once; fallback uses that same distance. Reversed distance bounds are normalized. Pop Egg criterion requires/enables eggs. Safe ground/platform selection and FD fallback remain. |
| 3. Global/local invincibility | Native function `0x800C0658` selects color slot 0 when `color[0].colanim` at fighter offset `0x430` is nonzero, otherwise slot 1. The old wrapper disabled slot 0 but wrote slot 1, leaving the selected slot colorless. It now applies the composed tint to the native selected slot and restores all 384 bytes of native color state after drawing. `color_overlay_id` is not this chooser and is left untouched. Global protection and Ledgedash local highlight share the correction. |
| 4. Perfect ledgedash yellow/green | The same selected-slot fix allows timing colors to replace protection during perfect invincible wavelands. Tests execute the actual native chooser and verify two yellow/two green frames, followed by the protection tint, with the original native colanim ID preserved. |
| 5. Regular landing in Act OoWait | Shared C implementation accepts normal Landing regardless of whether an aerial preceded it. It subtracts the fighter's actual normal lag instead of hardcoded four frames, includes direct LandingAir*/LandingFallSpecial exits, retains intermediate movement and the 13-opportunity policy, and emits on the first update of the performed action. Source line identifies Landing, Autocancel, L-cancel/Missed L-cancel, Waveland/Special landing, Tech/Tech roll/Getup, Throw, Laser, Needles or Float aerial where supported. L-cancel labels use the landing-entry snapshot. |
| 6. Legend | The action legend reads **Act**, preserving attack, grab and initial-dash classification and the existing numeric/timeline slot. |
| 7. Wavedash font | First-row title/result runs use 70% scale; a hitlag-prefixed row uses compact positioned prefix/result runs at 55%, preserving angle/hop rows and the three-line limit. |
| 8. Master label | Both editors use **OVERRIDE OSDS OFF** through the shared label constant. |
| 9. Hitlag prefixes | Native hook `0x8006AB78` samples actual fighter freezes before the old frame counter discards them and before IASA/physics producers. All tagged timing results snapshot relevant own-fighter hitlag; fastfall/wavedash/L-cancel use their airborne episode, shine preserves startup/loop contact, OoS/OoHitstun use their shield/victim episode, OoWait its recovery, and other timed categories the current state. Multiple contacts accumulate, duplicate updates do not. Display is e.g. **3hl -> Frame 1**; prefix stays white, actionable number keeps its original measurement/best-frame color. L-cancel retains `/7` and separate Success/Missed. Counters clear on restore/reposition/scene/death/rewind and reset with fighter generation. Untimed outcome/angle/advantage messages retain their meanings. |
| 10. Trail strength through hue | Fixed alpha again: current **200/255**, history **72/255** with the existing fade/lifetimes. Base damage blends player RGB toward the matching alternate over **3-15 damage**: red -> magenta, yellow -> orange, blue -> cyan, green -> neon green, gray -> white. 0-3 stays base, 15+ reaches the alternate. 9- and 12-damage phases remain distinct; each sample preserves its own hue, including simultaneous hitboxes. No damage-weighted opacity remains. |
| 11. Plan audit | The top-level running to-do list distinguishes unfinished earlier work, runtime validation, optional extensions and superseded requests. README T2 and the developer guide reflect the current implementation. |

`src/osd_context.c` owns bounded counters/opportunity arithmetic; `osd_context_game.c` owns twelve fighter-generation entries and native adapters. PowerPC layout is **64 bytes/context + 8 bytes identity = 864 bytes** of static runtime storage. `MsgData` grows from 48 to **56 bytes** per message for its snapshot and prefix index. No native FighterData or recording layout changes. Export slots **33/34** append context sampling and Act OoWait after the existing 33 exports; no old index moves. Build fingerprints include the new header and force consistent module/ASM rebuilding.

Validation: **70 compiled PowerPC regressions pass**, including actual native color selection, protected wavelanding, local highlights, freeze deduplication and episode retirement, native ordinary-landing/L-cancel labels, first-recovered-frame jump, original reset predicates, random distance bounds, compact Wavedash formatting, neutral prefixes, equal trail opacity with distinct hues, packing/migration and optimized MEX DAT relocation. The optimized C/assembly release build passes without compiler warnings and produces the updated versioned ISO/ZIP. Native frame stepping, visual fit and real card I/O remain the explicit checks above; user card files were not edited.


**37. October 6, second round: all nine requested adjustments**

This section supersedes the text/layout/opacity details in section 36. Release remains **V1.4.1T2 / TYRE01**. The CPU text override allocates **record byte 40 bit 2**, the first newly consumed reserve bit; the format remains the compatible **44-byte format 3**. All other saved fields and score storage remain unchanged.

| Request | Current implementation |
| --- | --- |
| 1. Shorthand numeric frames | Native OSD and C display strings use **Nf** instead of Frame N, including fastfall, shine, landing, item interrupts, jump cancel, lockouts, GALINT, frame advantage, custom Lab OSDs and legacy event frame counters. Ratios retain units, e.g. **2f/7f**. Titles such as Frame Advantage retain their meaning. Measurements and technique baselines are unchanged. |
| 2. Compact hitlag separator | **3hl->1f** uses the supported ASCII fallback with no spaces. The native 287-codepoint dictionary at `0x8040C8C0` has no right-arrow (`81A8` in Shift-JIS) or alternate arrow glyph. No unsupported UTF-8 is sent to the native converter. Prefix remains white; the result remains independently colored. Positions derive from native glyph tokens and kerning at `0x8040CB00`, so the runs meet without the old wide gap. |
| 3. Third timing color | Best/best+1/best+2/later is now **Cyan / Green / Yellow / Red**. A best-frame-1 result displays 3f yellow. Technique-specific best intervals stay intact, including Peach's original best frame 5. Saved title-palette White remains White. |
| 4. Act OoWait row order | **Title, recovery source, Nf**. Metadata marks a leading string pointer and second integer vararg explicitly (tag bit 22), retaining typed timing snapshots and correct third-row color/prefix. |
| 5. Reset-only egg randomization | A cached Ground/Platform choice and distance are stored in the copied Ledgedash event data. Actual reposition/reset chooses them once. Ledge regrabs may replenish the egg at the same choice, while savestate restoration and invalid-support recreation retain it; those paths consume no placement RNG. Option changes that reposition are resets. Egg objects/callbacks remain outside the copied event block. |
| 6. Wavedash hop line | Short Hop is **cyan at 1f**, **green at every other timing**. Full Hop is **red at every timing**. Missing-hop two-line displays stay two lines; angle colors retain their existing rules. The native producer uses appended shared export 35 (`OSD_WavedashHopColor`). |
| 7. CPU OSD override | **OVERRIDE CPU OSDS OFF**, default Off, appears beside the all-OSD override in both editors. It hides existing/new CPU-owned queued messages and their backgrounds at GX time, including untagged CPU feedback. CPU identity comes from `Playerblock.p_kind`, not port/slot number. Lab's CPU info panel is covered too. Human messages, general event feedback, trails/body overlays and each saved category choice are preserved. |
| 8. All-OSD label | Both editors use **OVERRIDE ALL OSDS OFF**. The original master flag and configurable-message scope are retained. |
| 9. Slight trail-opacity increase | Current hitboxes are **216/255** (previously 200); fresh historical samples **84/255** (previously 72). Strength still changes hue instead of alpha. Decay uses the original 200-based clock, preserving all endpoints and Instant's age-zero-only behavior. |

**Current reserve ledger:**

| Storage | Allocation / remaining capacity |
| --- | --- |
| Byte 40 / `0x1F4C`, bit 0 | Run Turnaround |
| Byte 40, bit 1 | Invincibility Overlay |
| Byte 40, bit 2 | **OVERRIDE CPU OSDS OFF** (new; global flag ID 8) |
| Byte 40, bits 3-7 (`0xF8`) | **5 bits free**, preserved by setters/validation |
| Bytes 41-43 / `0x1F4D-0x1F4F` | **24 bits / 3 bytes free** |
| **Total unassigned** | **29 bits**, down from 30 |

Fresh records and unversioned migration initialize CPU suppression Off. Versions 1/2 migrate with the newly owned CPU bit Off while preserving the other 29 reserve bits. Previously generated format-3 records have this formerly unused bit zero; existing format-3 CPU choices round-trip unchanged. Older format-3 builds ignore/preserve the added flag as an unknown reserved bit, so returning to this build restores it. No signature, existing flag index, OSD ID, control offset or score region moves.

The native 29-row editor now reads: nineteen OSDs, gap at row 19, ALL override at 20, CPU override at 21, then seven visual/gameplay controls at 22-28. Logical CPU row ID 29 maps to flag 8; native physical RSS row ID 18 maps to that logical row. Unknown enable-mask bit 29 is preserved. Lab's global rows are shifted coherently with all initialization/callback offsets. MsgData adds the player queue index, growing **56 -> 60 bytes** per message; Ledgedash adds **8 runtime event bytes** for cached placement. No additional save bytes are needed.

Validation: **76 compiled PowerPC tests pass**. New coverage checks exact shorthand/prefix text and native-kerning adjacency, source-first varargs/third-row metadata, cyan/green/red hop policy, live CPU suppression and human/general preservation, independent CPU native-menu saves and untouched mask/reserve bits, reset-only egg choices and restore recreation, third-frame yellow, and unchanged decay endpoints at the increased opacity. The optimized release compiles C and assembly together; shortened assembly strings have explicit alignment before following instructions. Native save/card switching, on-screen typography and actual gameplay remain live Dolphin checks in the running to-do list.


**38. Follow-up: hop measurement, normal Wavedash font, one-row timing and Jump Out Of Shine**

Release remains **V1.4.1T2 / TYRE01**, settings **44 bytes / format 3**, and **29 reserved bits** remain. This work consumes no persistent bits, changes no native FighterData layout, and moves no existing function-export index.

1. **Hop color uses the actual printed hop count.** The native Wavedash producer's r7 is wavedash timing, while r9 is the hop-duration value printed on row three. The earlier helper was incorrectly passed r7's source. The producer now saves r9 in nonvolatile r25 before Message_Display and passes that value to the shared color helper afterward. Short Hop at 1f is cyan; every other Short Hop is green; Full Hop is always red. Wavedash timing and hop duration are independent measurements.
2. **Normal top-row font.** Wavedash title/timing now form one centered first row at 100% subtext scale. Angle and hop stay on rows two/three with their original rules. The native message's aspect fitting can still accommodate unusually long text; there is no imposed 70% top-row scale.
3. **No separate centered hitlag fragments.** The earlier manual kerning/position estimates did not match the game's renderer and allowed the result to overlap the arrow. The new formatter creates a single centered row containing all timing components. ESC plus eight uppercase hexadecimal digits is recognized by the existing native ASCII conversion hook and emitted as a native 0x0C command with exactly three RGB bytes (corrected in section 39). No control spelling appears on screen. The hitlag prefix remains white, and timing/turn components retain their own colors; spacing and alignment come from one native glyph stream. Visible text remains e.g. `3hl->1f`, without surrounding arrow spaces. The combined Wavedash row begins with its title, so the original subtext header still supplies the live user-selected title color.
4. **Confirmed Jump Out Of Shine, with turn context.** Fox/Falco's old generic loop checks predicted a jump from input before the native IASA callback. A turn has priority in that callback, so such a prediction could misreport a turn as an action/jump. The old loop display is removed. A shared observer captures loop opportunities before IASA at the existing hook `0x8006B7F4`, then inspects the actual resulting state after the callback at `0x8006B80C`. The title is **Jump Out Of Shine**. JumpSquat/ground jump/air jump transitions produce the jump result; first turn entry records context and waits. B release, failed jump input, startup, reflection-hit recovery and turn recovery do not create a jump result.

**Invisible timing note: the normal shine turnaround consumes two native animation steps, including its entry update.** Turn IASA has no jump/turn checks. Wait for the engine to restore the loop, then resume the opportunity count at 1; do not add these unavoidable steps to the player's delay or print them as late inputs. The implementation follows native loop availability rather than blindly subtracting two from an elapsed state timer. This also handles pause, hitlag and nonstandard animation durations. The [shared Fox/Falco reflector handlers](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftFox/ftfoxspeciallw.c) corroborate loop turn-before-jump priority, the countdown on turn entry/animation, and empty turn IASA handlers.

The first turn's `Ntrn` is measured from initial loop input opportunities and colored Cyan/Green/Yellow/Red at 1/2/3/4+. After that turn, the jump's `Nf` is counted from restored jump availability. Airborne loops with no remaining jump do not inflate this jump counter before landing. Turning has its own opportunity count because it can remain available without an air jump. Hitlag stays a separate accumulated own-fighter context. Examples:

- Jump immediately, without a turn/contact: **1f**.
- Contact, first turn, then jump immediately after recovery: **3hl->1trn->1f** (hitlag white; first turn cyan; jump cyan).
- First turn on opportunity 3, delayed jump after recovery: **3trn->2f** (first turn yellow; jump green).
- A second actual turnaround completes the episode immediately: **1trn->1trn**, optionally with **Nhl->** before it. The **second turnaround is always red**, regardless of its count. Later jumps/turns in that completed episode do not issue another result; a fresh shine starts a new episode.

All counters and the final result are bounded/snapshotted, keyed to fighter object/generation and native simulation update. Duplicate callbacks, pause, death, scene/restore/reposition and rewind follow the existing transient-context policy. Ground/air loop transfers preserve an episode; a fresh startup resets it. Global OSD colors, ALL/CPU suppression, canonical category 8 and queue replacement continue to apply. Other fighter-specific techniques, including JC Shine, are preserved.

Runtime costs: the new shine episode is **52 bytes**, appended to each of twelve context entries. The context table grows **864 -> 1,488 bytes** (+624). MsgData appends three fields and grows **60 -> 72 bytes** (+12/message), preserving every previous offset. Export slots **36/37** append before/after IASA observation after the prior 36 exports. No persistent setting or recording fighter layout changes.

Validation: **84 compiled PowerPC regressions pass**. The tests execute the actual assembled Wavedash producer with mismatched wavedash/hop counts and confirm both printed values and hop color; they execute the actual patched native text converter and inspect its color/glyph stream rather than assume text-run geometry. Additional tests cover both Fox and Falco, actual turn/jump outcomes, two-turn termination/red formatting, two blocked turn updates, hitlag, duplicate updates, no-air-jump availability, release/failure suppression and reset/rewind. C and assembly release builds are warning-free. Final ISO/archive module verification is separate. Live Dolphin frame stepping and visual confirmation remain in the running to-do list; no user card was edited.


**39. Native text-pointer warning and corrupted hitlag/turn/title rows**

The reported warning is **Unknown Pointer 0x025187c0, PC 0x803A8368, LR 0x803A824C**. These addresses are in the native text-width parser. The newly added inline-color converter incorrectly emitted **0x0C + four RGBA bytes**. Local DOL disassembly shows the width parser skips only three payload bytes at 0x803A8314, and the renderer consumes RGB at 0x803A8D7C. The extra alpha byte (normally 0xFF) becomes a glyph prefix, causing an invalid SIS glyph/kerning lookup. It also throws off the subtext locator used by text editing, explaining missing Wavedash titles and corrupted/overlapping shine and hitlag rows.

The converter now emits **0x0C + exactly three RGB bytes**. Its ASCII escape syntax still accepts eight uppercase RGBA hex digits; alpha is consumed from the input and deliberately omitted from the native payload. One centered timing row and its separate inline colors remain. Shine state logic, hop-count measurements, normal Wavedash font, OSD preferences and save format are preserved.

The previous tests checked conversion output but did not execute the downstream native parser; that gap allowed the invalid payload through. The new regression runs the actual native width parser at **0x803A8134**, including the reported PC, and native subtext locator **0x803A6FEC**. It reproduces an unmapped-pointer failure using the old four-byte payload, then verifies corrected streams with zero/0xFF color bytes, Wavedash title/angle/hop rows, hitlag/landing-source rows and shine turnaround/jump rows. Text opcode-history and bounded SIS font fixtures are initialized to match the parser's requirements.

**85 compiled PowerPC tests pass**, and the optimized C/assembly release builds without warnings. The final versioned ISO/archive is rebuilt and its shared/event/ASM payloads checked separately. This addresses the demonstrated text-stream failure; live Dolphin confirmation remains required. Version **V1.4.1T2**, identity **TYRE01**, format **3 / 44 bytes** and **29 reserved bits** remain unchanged. User cards were not edited.


**40. Remaining hitlag pointer warning: repeated native text replacement**

The follow-up warning reports **Unknown Pointer 0x025345c0, PC 0x803A8438, LR 0x803A8FBC**, again in the text-width/render path. The user also showed duplicate hitlag fragments and nearly blank Jump Out Of Shine text, while non-hitlag cases worked. The RGB payload correction in section 39 was necessary but did not cover how native text setters replace a previously colored row.

Local DOL inspection identifies another limitation: the optional old-body-length scan used by **Text_SetText**, at **0x803A7068**, treats inline color opcode 0x0C as an end marker. It recognizes glyphs/spacing/tracking, but does not count the four-byte RGB command. A second replacement therefore fails to remove the old colored body. SetText writes the new closing command over the previous color opcode, leaving its RGB bytes and text behind; those orphaned bytes can be interpreted as glyphs, producing duplicated/blank text and invalid font pointers.

Shine results previously went through formatting twice when hitlag was present: once inside Message_Display using generic hitlag context, then after the caller attached the finalized shine/turn context. Without hitlag, the first pass left the row plain, explaining why ordinary/turn-only cases behaved differently.

Two coordinated corrections are implemented:

- A native C2 hook at **0x803A7068** counts **0x0C plus three RGB bytes as four body bytes**, preserving correct replacement bounds. This makes later growing/shrinking updates safe for colored text too.
- **TM_OSD_DEFER_FORMAT**, tag bit 28, makes shine results wait until finalized hitlag/turn/outcome metadata is attached before formatting. The completed row is formatted once; timing/turn colors and one-row alignment remain intact.

The regression now runs the actual **native SetText, Position, Scale, Color and subtext locator**, including repeated replacements of the same complete message buffer. Without the old-length hook it detects retained/corrupted data; with the hook it verifies exact title/body/detail boundaries, width parsing and memory canaries over multiple growing/shrinking hitlag, turnaround and plain-text updates. A bounded fixture supplies the already-formatted string to the native setter, as the production timing builder does. Previous tests exercised conversion and finished-buffer traversal but missed this mutation lifecycle.

**86 compiled PowerPC tests pass**; the optimized C/assembly release builds without warnings. The corrected versioned ISO/archive is rebuilt and payload-checked. Read-only Dolphin MEM1 inspection was used to inspect text structures; no process memory or user card was modified. Version **V1.4.1T2 / TYRE01**, packed format **3 / 44 bytes** and **29 free reserved bits** are unchanged. Live gameplay confirmation of the reported layouts remains outstanding.


**41. Global Settings naming and final grouping**

The cue label is now exactly **Actionable Yellow>Green**, shared by both editors. The logical flag and yellow/green behavior are unchanged.

Both Global Settings menus now use this order: **nineteen OSDs, OVERRIDE CPU OSDS OFF, OVERRIDE ALL OSDS OFF, one blank separator, then the seven overlays/shared controls**. Native display rows 19/20 are CPU/ALL; row 21 is the disabled separator; rows 22-28 are the visual/gameplay controls. Native physical RSS IDs 16/17 now map to CPU/ALL and ID 18 to the separator. Logical flag IDs and serialized bits remain unchanged. Lab includes the same disabled empty row, and its initialization/callback offsets are shifted consistently.

Existing compiled editor regressions cover the moved override rows, hidden separator checkbox, all native exit saves, preserved unknown OSD-mask bits and palette/flag round trips. The full native text-update regressions remain included. Format **3 / 44 bytes**, version **V1.4.1T2**, identity **TYRE01** and **29 free reserved bits** are unchanged. The README records these Tyro changes.


**42. Current audit: what is actually unfinished**

Audited October 6 against the current source, rather than treating the original proposal as an up-to-date implementation inventory. This is planning work; no gameplay code, save layout or release version changes in this review.

| Item | Current status and remaining work |
| --- | --- |
| Original global settings, trails and Ledgedash feature set | Implemented, including both editors, title palettes, best-relative cyan/green/yellow/red timing, ALL/CPU suppression, shared trails, missed-cancel/turnaround pulses, protection, Infinite Shields, two yellow/two green frames, Ledgedash criteria and randomized ground/platform eggs. Sections 36-41 supersede earlier text and behavior. |
| Imported-recording shield policy | **Unimplemented exception.** Lab turns its local shield option Off on an imported rwing recording, but the global refill service can still force full health. Suppress the effective global refill during accuracy-preserving playback without changing the saved toggle; provide an explicit return to modified practice. `Lab_RefreshShieldOverride` already supplies the local menu's status text. |
| Literal unbreakable shields | **Not implemented.** Current mode refills full health. Audit native damage/break ordering before promising that no single hit can break a shield. This is a behavior decision/hook, not a new saved setting requirement. |
| Expanded actionable cue | **Shieldstun still missing** from the initial proposed coverage. Hitstun, dodge/roll recovery, jumpsquat and specials are later candidates requiring individual actionable-boundary definitions. Landing, wavelanding/autocancel, aerial landing lag and ordinary ground/aerial attacks are already covered. |
| Extended Act OoWait recovery context | Regular landing and short-chain recovery labels work. Longer chains remain bounded by the six native history entries and the existing opportunity-window checks; retaining the recovery episode beyond those limits is still future work. |
| Own-fighter stun prefixes | `hl` and shine turnaround context are implemented. Additional actual victim `hs`/`ss` context is not. Contact that merely puts an opponent into hitstun/shieldstun must continue to be described as the acting fighter's hitlag, not its own stun. |
| Event preference persistence and resets | Some Lab controls already persist in the 44-byte shared record. General per-event preferences and the three requested event reset buttons do not have a centralized persistent implementation. See step 7 below. |
| Fixed grid / Practice Panel | Both are newly planned. The current renderer remains a recent-message queue. See steps 8-9 below. |
| Optional legacy imports / eighteenth Lab overlay row | Explicit copied GTME settings/recording conversion and an editor for the eighteenth packed condition are not implemented. Identity isolation and seventeen-condition Lab persistence are implemented. |
| Save capacity and score reclamation | The existing settings cost and score-region bounds are known. Actual card allocation/free blocks are not measured, and the 92/204-byte candidates remain owned by score/reset code. No extension has been allocated. |

**Validation still outstanding, distinct from missing features:** complete the targeted live frame-step matrix, real card/no-card/failure/reload and GTME/TYRE switching, TYRE-specific Dolphin default audit, and dense-match heap/rendering measurements. The current 86 compiled PowerPC regressions and warning-free release build are established evidence; the user's stability report is additional evidence. Neither means that every listed combination has been checked. The recent pointer/layout fixes should not be listed as absent merely because broad validation remains open.

Very Very Fast decay and the revised Falling-start reset lifecycle were deliberately removed/reverted. Do not re-add them as unfinished work. Keep score persistence until the storage decision explicitly changes it. Older tables in sections 2, 14, 17 and 23 contain historical budgets: **today the core is 44 bytes, format 3, with 29 reserved bits**; the current Ledgedash menu is larger than the early three-byte proposal.


**43. New step 7: remembered event settings and event-scoped defaults**

**Objective:** Training Lab, Ledgedash and Eggs-ercise should remember chosen preferences across leaving/re-entering an event and, with a valid Tyro memory card, across restarting the game. Each event gets **Reset Event Settings** in its main menu, before Exit. Begin with a shared session service to establish lifecycle/default behavior, then attach permanent storage; RAM-only remembering is an intermediate milestone, not completion of the requested memory-card behavior.

**Inventory and scope:**

| Event | Initial preference inventory | Exclusions / decisions |
| --- | --- | --- |
| Training Lab | General display/camera/HUD/game-speed/staling/local-trail choices; CPU behavior, DI/SDI/ASDI, counter actions/delay, shields/health/angle, tech/getup choices; info-panel presets/size/rows. Reuse existing saved input-display, control-binding and actor-overlay values rather than duplicating them. Audit advanced counter, dynamic character recovery and other submenus by stable field ID before promising a complete menu snapshot. | Frame Advance active state, live actor percent/position, recording/sample buffers, savestates, current playback phase, controllers and pointers are runtime state. Persist explicit target-percent/lock preferences only after defining entry semantics. Recording-file metadata remains part of the recording, not a copy of general preferences. |
| Ledgedash | Twelve main preferences: Starting Position, Reset, HUD, Tips, Camera Mode, Keep Ledge Invincibility, Game Speed, Color Overlays, Reset Delay, initial Ledge, Success Criteria, Protection Highlight. Seven Egg Targets preferences: Enable, Target, Distance, Randomize Distance, Min, Max, Pop Damage. | Nineteen values fit in **19 bytes with one byte per choice**, or at least **52 packed bits / 7 bytes**, before headers. Do not save the current randomized target/distance, automatic side swaps, target object, attempt timer, success counters or active Frame Advance. Save the user's initial ledge selection, not the side selected by an automatic reset. |
| Eggs-ercise | Damage threshold, scale preset, spawn velocity, fighter collision, local trail enable and decay: **six one-byte choices / 6 bytes** before headers. | Retry and Enable Free Practice are actions, not stored menu values. Initially retain normal challenge entry and remember tuning for when Free Practice is enabled; do not bypass its gating just by loading values. An optional remembered start mode would need a separate explicit design. Do not persist egg objects, spawned positions/RNG, timer or current score. |

These are preference records, **not serialized `EventOption` structures**: those structures contain runtime pointers and callbacks. Define immutable defaults next to a typed, named field schema; menus, reset logic and load validation must use the same definitions. Distinguish schema field identity from menu-row indices so moving a row cannot reinterpret a save. Use byte enums where bounded, wider explicitly encoded fields where necessary; validate ranges and dependent choices such as min/max and Pop Egg requiring eggs enabled.

**Storage feasibility and decision gate:**

- The **29 reserved bits** cannot hold the requested event inventory; Ledgedash alone needs at least 52 bits. This step must not claim that all settings fit in the existing reserve.
- The **92 bytes** at `0x1AE0-0x1B3B` are unused by the present event catalogue, but native indexed score/reset paths still own them. A small whitelist might fit after metadata; a full Lab inventory may not. Using them needs protected access/reset paths and a capacity policy for future events.
- Retiring numeric event high scores would make **204 bytes** at `0x1A70-0x1B3B` available after all score read/write/reset ownership is changed. Preserve played flags and unrelated records. The native 216-byte records clear starting at `0x1A68` must not erase a preference extension.
- An illustrative extension budget is **16 bytes of metadata + a provisional 96-byte Lab cap + 19 Ledgedash + 6 Eggs-ercise = 137 bytes**, leaving **67 of 204**. The Lab cap is a planning allowance, not a measured full-menu size. Finish the whitelist before selecting this layout; do not silently drop settings to meet the allowance. This example exceeds the 92-byte candidate.
- A separate Tyro preference card file is a fallback if the desired inventory grows beyond the bounded native region or scores are retained. It adds file/block allocation, card lifecycle and failure handling; measure those costs before choosing it.
- Keep the existing **44-byte format-3 core** intact. Its two-bit version tag is already 3, so blindly incrementing it to 4 is invalid. Give the extension its own magic/version/length, per-event schema and validation metadata. The score and core regions are noncontiguous; never cast them as a single continuous 248-byte structure. Foreign identities or unsupported future schemas must use private defaults without rewriting those bytes.
- A header cannot stop an **older Tyro executable** from writing scores over a repurposed score region. Upstream GTME remains isolated by the implemented identity, but same-ID Tyro downgrade behavior needs its own policy. If preserving preferences through older Tyro builds is required, prefer a separately named preference file or restrict switching to compatible builds/copy-based tests. Detecting corruption and returning defaults is recovery, not preservation. This decision belongs in the storage comparison.

**Recommended implementation sequence:**

1. Produce the exact named field/default/range/serialized-size inventory for the three events, including Lab's nested menus. Mark transient and already-shared fields explicitly. Choose the initial whitelist and storage route from the measured total.
2. Add event-preference accessors to the shared service so separate event DAT modules use one session copy. Load choices before initialization applies camera, HUD, actor setup and egg placement; reconcile dependent effects once after batch loading.
3. Add **Reset Event Settings** to all three main menus using those immutable defaults. Update both menu values and their previous/derived values, restore visibility/disable rules, and apply the effective settings in one batch. One button press must not execute every callback's save/reposition independently.
4. Attach the versioned card extension through native dirty/checksum/save machinery. Coalesce changes and save at a deliberate menu-close/exit checkpoint; no card writes on every frame, egg spawn, ledge touch, automatic reset or side swap. Preserve session choices if the card is unavailable or a save fails and show the existing appropriate save status.
5. Keep recording imports/playback and automatic event behavior as **temporary effective overrides**. They must not overwrite the user's stored preferences. In particular, resolve recording-safe Infinite Shields policy before restoring saved Lab defaults into accuracy-preserving playback.
6. Add migration/default validation and live card tests before considering the step complete. New fields get defaults; supported older event schemas migrate by named mapping. Future unsupported blocks remain untouched. Reenter all three events and reboot, testing card failure and an older compatible Tyro build as well as upstream isolation.

**Reset scope:** reset only that event's preferences. Leave the other two events, shared Global Settings/title colors/overrides, recorded inputs/files and persistent high scores alone. Lab-owned controls/input display/actor overlays already stored in the shared record may reset with Lab; document that these are Lab preferences, distinct from Global Settings. Stop active playback/recording safely without deleting captured data. Ledgedash reset cancels the pending attempt and regenerates its target once; criterion statistics follow the existing criterion-change policy. Eggs-ercise restores challenge/free-practice gating coherently. Default Frame Advance remains Off on fresh event entry.

**Checks:** validate each field and adjacent-byte canaries; reset and reload round trips; configuration changes without saving temporary RNG/side/recording state; all menu values/disabled rows after reset; one batch of side effects and one save checkpoint; corrupted/unsupported extension data; no-card/failed-card behavior; upstream/Tyro switching; records-reset preservation if a score region is chosen. Add a README gameplay changelog entry only when the feature is actually implemented.


**44. New steps 8-9: stable OSD layouts and readable repetition**

**Why the current display moves:** `Message_Add` removes a matching `(kind, settings_id)` result, shifts the queue and puts the newest message at index zero. `Message_Destroy` shifts surviving entries again. `Message_Manager` animates changed indices over **six updates**, animates deletion over six, and expires messages after **120 simulation updates** (about two seconds at normal unpaused 60 Hz). There are seven queues, eight messages each; configurable player OSDs are not one permanent category bank. Repeating several techniques therefore changes their location even when the set of enabled categories never changes. Disabling slide animation alone would make these position changes abrupt and would not solve the problem.

Retain **Recent** as a compatibility choice. Add **Fixed** as requested and prototype the recommended **Practice Panel**. Both new styles should retain the latest completed result until replaced or the practice context resets: expiry marks it stale instead of removing its geometry. Current best-relative colors, title palettes, hitlag/turn prefixes, technique-specific baselines and non-timing outcome rules remain measurement rules, independent of layout.

The conversation mockups compare these three styles with a user-triggered repetition sequence. They are schematic layout proposals, not native game screenshots or measured final font/cell dimensions. Browser checks confirm stable Fixed/Panel coordinates and deliberate Recent reordering at 320, 480 and 736 pixels; final game fit still needs the acceptance checks below.

**Step 8 — Fixed grid:**

- Assign enabled categories a canonical, stable order, then reserve cells from **top left across to the right, then down rows**. Empty cells show their title and `—` until a result arrives. New results replace only their own cell; no insertion, shifting, collapsing or expiry-based reflow.
- Keep the mapping stable throughout practice. Rebuild deliberately after closing the settings editor or changing the eligible player set. ALL/CPU overrides hide the affected content without reshuffling the remaining cells; selected category preferences remain intact.
- Key content by **player + canonical category**, retaining the exact technique/kind inside each result. Never allow P2/CPU results to overwrite P1's cell. For the common one-human/CPU-suppressed case, five enabled categories mean five reserved cells. Multiple players require labeled cells or deliberate per-player pages.
- Start with two or three columns using measured 640x480 safe-area bounds and readable text. Keep space for the stage HUD, timer and three-row contextual displays, including long shine prefixes and Wavedash. Do not shrink all text to force nineteen categories into one screen.
- Provide bounded, **manually selected pages** when the selection cannot fit; every selection has a stable logical slot/page. Never rotate pages automatically during repeated actions. Size page capacity from the prototype, rather than promising that all nineteen OSDs times all actors fit at once.
- On death/reset/retry/rewind/new match, clear stale result/history as appropriate while retaining the chosen layout and category mapping. Clearly identify a retained value as **last result**, not current state.

**Step 9 — recommended Practice Panel:**

A single compact panel near a screen edge, with **one stable row per selected category**: title at left, latest timing/outcome at right, brief recovery context underneath only where needed. This avoids repeated full boxes and uses much less screen area than a five-card grid. The row never slides, disappears or changes height when another category fires.

- Update the number in place when the measurement finishes. Keep the result until a newer attempt. Identical results refresh recency/repetition count without another flash. A small nonblinking new-result indicator is sufficient; avoid pulsing the whole box or repeatedly fading it out.
- Give stale results a readable subdued treatment or an age marker. Keep frame colors legible and meaning intact; do not dim a result to near invisibility. Use a stable width for numeric/prefix content.
- Optionally retain the **last three results** as small labeled history markers in the same reserved row width. This gives context for rapid repetition without a queue animation. Preserve full typed outcomes; do not assume frame advantage, angles, timing and counts share a universal average or success rule.
- Allow a chosen focus row to show full Wavedash/shine/landing detail in a **reserved detail area**. Other rows stay fixed. Selecting a focus is explicit; an automatic focus that follows every new result would reintroduce distraction.
- Coalesce duplicate notifications of the same actor/category/action episode, while retaining genuine separate attempts even when their numbers match. Apply the newest completed measurement immediately; do not delay feedback merely to keep it on screen. A bounded history handles bursts.

**Shared architecture and cost:**

1. Separate measurement/result capture from placement and lifetime. Keep a bounded per-player/category result bank and use the current message API as the compatibility entry point. Event/general feedback outside configurable categories can continue using its own recent queue and essential-event scope.
2. Preserve the native caller contract: several producers recolor/edit the returned text object. Do not return NULL or destroy it before their edits. Preserve title/body subtext indices and the tested inline-RGB replacement lifecycle. Store finalized hitlag/turn/source metadata without creating two independently centered timing fragments.
3. Category 8 has multiple character-specific techniques and alternate queue kinds. Fixed/Panel may show the latest technique in that category, but its title/detail/history must retain which technique produced it; never change the measurement producer's replacement semantics by conflating raw message kind with setting ID.
4. Bound history, text objects and rendered cells. Up to nineteen categories across six player queues is **114 logical keys**, not a promise to allocate 114 heavy text/background objects. Render only the visible page/rows and measure actual text/GOBJ/heap cost; runtime history consumes RAM, not save space. Clear generations safely on stock loss, scene changes and restores.
5. Expose the style in both editors. The native editor already fills 29 rows, so a useful route is expanding its existing X/Y display selector to the four Recent anchors plus Fixed and Practice Panel, rather than consuming the separator. Lab can expose a separate Display Style field. UI choices may be composite; stored layout mode and Recent anchor remain separate.
6. **Proposed reserve allocation, not yet made:** two bits for `Recent / Fixed / Practice Panel / reserved`. Section 45 now owns byte 40 bits 3-6, so the earlier candidate there is unavailable. Use, for example, the remaining **byte 43 bits 5-6**. Keep position byte 4 in its existing 0-3 range: current readers sanitize 4/5, so writing new modes there would lose the selection on older builds. Setters must preserve the other unknown bits. If allocated, reserve falls **4 -> 2 bits**; it is **four today**. Grid density, focus and history preferences would each need their own measured budget if persisted.

**Acceptance checks:** five selected categories keep identical coordinates over rapid alternating/repeated results and expiry; placeholders and stale values are unambiguous; title/result/source and mixed inline colors fit without clipping at standard aspect ratios; human/CPU/all overrides preserve the map; multiple humans and category subtypes remain distinct; no automatic page/focus movement; frame advance, pause, slow motion, rewind, stock loss and retries preserve correct lifetimes; heavy matches have bounded allocations and acceptable render time. Run existing native text lifecycle regressions alongside any new layout changes.

**Recommended order:** finish the exact step-7 preference inventory and storage choice; implement session defaults/reset buttons and permanent persistence; then build the shared stable result bank plus Fixed; finally implement Practice Panel using that same bank. If screen movement is the immediate priority, Fixed and its result bank can be developed before permanent event storage because they depend only on the small shared display-mode preference, not the score extension. Keep the remaining shield-policy/frame/card checks in the release checklist throughout.


**45. Step 7 first implementation: reserved-bit Ledgedash and Eggs-ercise preferences**

The user narrowed the first implementation to these two events and existing reserve only, excluded Eggs-ercise local trail settings, and specifically requested saved Ledgedash hints and Eggs-ercise infinite mode. This section supersedes section 43's initial mode assumption and its full-event inventory as the scope of this first pass. Version remains **V1.4.1T2 / TYRE01**, core **44 bytes / format 3**; no scores or native records are repurposed.

| Saved preference | Bits | Default |
| --- | ---: | --- |
| Ledgedash Starting Position | 3 | Ledge |
| Ledgedash Reset | 3 | Same Side |
| Ledgedash Success Criteria | 2 | GALINT |
| Ledgedash Reset Delay | 2 | Normal |
| Ledgedash Tips / hints | 1 | On |
| Eggs-ercise damage threshold | 8 | 12 |
| Eggs-ercise scale | 2 | Normal |
| Eggs-ercise spawn velocity | 1 | On |
| Eggs-ercise fighter collision display | 1 | Off |
| Eggs-ercise infinite / Free Practice mode | 1 | Off (timed challenge) |
| Initialization marker for both blocks | 1 | Clear on an older save; publish on first valid edit |
| **Newly allocated** | **25** | **Four of the previous 29 bits remain free** |

**Current reserve ledger (logical record bytes, not encoded card offsets):**

| Location | Assignment |
| --- | --- |
| Byte 40 bits 0-2 | Existing Run Turnaround / protection / CPU OSD override |
| Byte 40 bit 3 | Event preferences initialized (`0x08`) |
| Byte 40 bits 4-5 | Ledgedash reset delay (`0x30`) |
| Byte 40 bit 6 | Ledgedash Tips (`0x40`) |
| Byte 40 bit 7 | **One free bit** (`0x80`) |
| Byte 41 bits 0-2 / 3-5 / 6-7 | Ledgedash start / reset / criterion |
| Byte 42 | Eggs-ercise damage threshold |
| Byte 43 bits 0-1 / 2 / 3 / 4 | Eggs-ercise scale / velocity / collision / infinite mode |
| Byte 43 bits 5-7 | **Three free bits** (`0xE0`) |
| **Remaining reserve** | **Four bits; zero whole unused bytes** |

Settings fields **18/19** append stable preference IDs through the existing shared accessors. Field **20** resets one saved event block. Explicit masks preserve unrelated globals and the remaining free bits; no C struct or enum size defines the card layout. The current record's two-bit format tag remains 3. Previously generated format-3 saves have the new initialization bit clear, so getters use immutable defaults without dirtying or interpreting old reserve payload. The first valid explicit edit initializes both blocks and sets the marker last. Version-1/2 migrations also clear that marker. Invalid enum/range data is repaired individually once initialized; invalid writes do not initialize the block. Unsupported/foreign core records continue using private runtime defaults without writes to their saved data.

Ledgedash loads the five choices before initial placement; it sets current/previous menu values, updates the criterion label and enables eggs when Pop Egg requires them. Only explicit menu edits write preferences, so automatic resets, ledge side changes and egg RNG never alter saved choices. Other Ledgedash options, including egg target/distance controls, remain event-local in this first pass.

Eggs-ercise remembers four tuning values and its infinite-mode choice. Selecting Enable Free Practice saves infinite mode; reentry restores the count-up timer and enabled tuning controls. Timed mode retains challenge gameplay defaults while staged tuning values are shown in disabled menu rows. Local trail preferences consume **no save bits**; global trail choices remain the shared controls. Active egg objects, positions, RNG, score and elapsed time are runtime state.

Both main menus now have **Reset Event Settings**. Ledgedash restores all of its menu defaults in one batch, updates camera/HUD/tips and repositions once; criterion statistics follow the existing criterion-change policy. Eggs-ercise resets its saved values and restarts the event in timed mode, with a fresh clock and correct enabled/disabled rows. Neither reset changes another event, Global Settings or persistent high scores. A restart also returns unsaved local controls to their ordinary initial values.

Explicit preference edits mark native card data dirty. Shared menu close and event exit/retry call the existing `Memcard_SaveIfChanged` checkpoint; there is no frame-by-frame save polling. Native card serialization/checksums and save identity remain responsible for disk writes. An absent/failing card can still leave the session copy usable, but permanent save/reload must be checked on a test card.

Validation: **95 compiled PowerPC regressions pass**, including every legal saved value, rejected indices/ranges, exact packed bytes/canaries, all four remaining reserve bits, first-write initialization, individual repair/idempotence, sibling-event reset isolation, hints/infinite-mode reload, private foreign/future fallback and fresh optimized DAT relocation. Optimized event/ASM release builds and final ISO/archive payloads are checked separately. Live Dolphin card reload, no-card/failure paths and mode/reset/menu behavior remain the release checks.
