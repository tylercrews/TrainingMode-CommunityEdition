# Training Lab menu: T3 investigation and implementation plan

**Version: V1.4.1T3. Initial source audit: October 7, 2026. Branch: `t3-menu-rework`.**

This is the working research and decision log for the Training Lab menu rework. Like [the OSD investigation](OSD-GLOBAL-SETTINGS-INVESTIGATION.md), it separates observed behavior, proposed changes, implementation, and validation. **The first native implementation is now built; section 12 records its code, checks and remaining Dolphin validation.** Section 2 preserves the baseline inventory before the rework, and later design decisions supersede earlier proposals where stated.

## Running to-do list

- [x] Increment the central release version to V1.4.1T3; retain the stable TYRE01 identity.
- [x] Inventory the current pause menu, conditional submenus, dynamic rows, custom screens, and shortcuts.
- [x] Propose a complete semantic grouping with row counts and a depth limit.
- [x] Investigate alternating rows, contiguous color blocks, selection, and value styling.
- [x] Establish how submenu descriptions can show purpose and contents before entry.
- [x] Sketch an alternative interface, then rebuild it around controller navigation after user feedback.
- [x] Choose a single-cursor category selector/list direction in place of the mouse-oriented rail.
- [x] Implement the first hierarchy and native controller layout; refine after a Dolphin trial.
- [x] Implement combined alternating rows and semantic color blocks.
- [x] Implement a presentation layer that preserves setting identities and callback behavior.
- [x] Implement purpose/contents previews, generic unavailable reasons, and existing shield/trail override explanations.
- [ ] Validate all conditional menus and recording transitions in Dolphin.
- [ ] Validate native 4:3/CRT readability, menu fit, input conflicts, and rendering cost.

## 1. Scope, sources, and the three views

The request names three ways of listing options but explicitly describes two. This document supplies **(1) the exact existing order, (2) the proposed semantic tree, and (3) common-task routes through that tree**. The third view makes the navigation cost reviewable without inventing another competing taxonomy.

The inventory is of the Training Lab event's **in-match pause menu**, including screens opened by its actions. Import is documented separately because it belongs to the Training Lab character-select flow, not the pause menu. The native L-button OSD editor is also a separate interface; its shared settings must continue to agree with Lab.

Primary local sources:

| Source | What it establishes |
| --- | --- |
| [src/lab.h](src/lab.h), menu definitions beginning around line 896 | Static row order, submenu destinations, labels, value arrays, shortcuts. |
| [src/lab.c](src/lab.c), `Event_Init`, `Record_*`, `Lab_*`, `Export_*`, `CustomTDI_*` | Runtime availability, replaced labels, saved settings, custom screens. |
| [src/recovery.c](src/recovery.c), `RecOptions_*`, `RecoveryMenus` | Five character-specific recovery menus. |
| [src/menu.h](src/menu.h) and [src/menu.c](src/menu.c) | Nine visible rows, four description lines, navigation, text/model drawing. |
| [src/settings.h](src/settings.h) and [src/settings_game.c](src/settings_game.c) | Shared names, setting IDs, save service and override scope. |
| [src/lab_css.c](src/lab_css.c) | Recording import interface outside the pause menu. |
| [build.sh](build.sh), [scripts/project-config.sh](scripts/project-config.sh), [version.h](version.h) | Central version metadata and rebuild rules. |

This audit reads source, not a live Dolphin menu. Rows can be disabled or renamed at runtime. Disabled rows still occupy the list; navigation skips them. Blank disabled rows are counted where the current renderer reserves space for them. Order below is one-based display order, not C enum IDs. Values such as On/Off are choices within a row, not extra menu entries.

## 2. View one: existing menus in appearance order

### 2.1 Main Menu — 9 rows

1. General → General
2. CPU Options → CPU Options
3. Recording → Recording
4. HMN Info Display → HMN Info Display
5. CPU Info Display → CPU Info Display
6. Character RNG → dynamically assembled Character RNG; disabled without a supported fighter
7. Stage Options → stage-specific menu; disabled outside Fountain of Dreams/Pokemon Stadium
8. Controls → Controls
9. Exit → return to Event Select

### 2.2 Main Menu → General — 19 rows

1. Frame Advance
2. Player Percent
3. Lock Player Percent
4. Model Display
5. Fighter Collision
6. Item Grab Sizes
7. Environment Collision
8. Camera Mode
9. HMN Color Overlays → HMN Overlays
10. CPU Color Overlays → CPU Overlays
11. HUD
12. DI Display
13. Input Display
14. Game Speed
15. Move Staling
16. Custom OSDs → Custom OSDs
17. Action Log → Action Log
18. Hitbox Trails → Hitbox Trails
19. Global Settings → Global Settings

### 2.3 General → HMN Color Overlays / CPU Color Overlays — 17 rows each

Both menus use the same order; the settings belong to different actors.

1. Actionable
2. Hitstun
3. Invincible
4. Ledge Actionable
5. Missed L-Cancel
6. Can Fastfall
7. Autocancel
8. Crouch
9. Wait
10. Walk
11. Dash
12. Run
13. Jumps
14. Full Hop
15. Short Hop
16. IASA
17. Shield Stun

Choices include colors and effects, not just enable/disable. Preserve all existing choices. These actor-specific overlay preferences use the shared settings service; moving them into a section called Visual Feedback must not imply they are automatically session-only. There are 17 editable rows; a reserved eighteenth storage condition is not an existing menu row.

### 2.4 General → Custom OSDs — 9 reserved rows

1. Add Custom OSD
2. Remove OSD: `<state name>` — first added state, otherwise blank/disabled
3. Remove OSD: `<state name>` — second added state, otherwise blank/disabled
4. Remove OSD: `<state name>` — third added state, otherwise blank/disabled
5. Remove OSD: `<state name>` — fourth added state, otherwise blank/disabled
6. Remove OSD: `<state name>` — fifth added state, otherwise blank/disabled
7. Remove OSD: `<state name>` — sixth added state, otherwise blank/disabled
8. Remove OSD: `<state name>` — seventh added state, otherwise blank/disabled
9. Remove OSD: `<state name>` — eighth added state, otherwise blank/disabled

Added states appear in insertion order. Removing one compacts subsequent rows. At capacity, Add remains present but refuses the addition with an error sound. A proposed improvement is a visible `8/8` explanation rather than sound alone.

### 2.5 General → Action Log — 8 rows per selected action

1. Action Number
2. Action Behaviour
3. Set State — becomes **Remove State** with the captured state name as its value
4. State Frame
5. Min Stick X
6. Min Stick Y
7. Fastfall
8. IASA

Action Number selects one of ten configurations; it does not open ten nested menus. Keep that bounded editor pattern. Action Behaviour is the existing display/color behavior choice; do not relabel it as CPU behavior.

### 2.6 General → Hitbox Trails — 3 rows

1. Enable (local)
2. Decay
3. Global: Off — informational; can become **Global: Very Fast**, **Global: Instant**, or **Global: Both On**

Decay choices are Normal, Fast, Very Fast, Instant, Slow, Off. **Decay Off means no fading**, not disabled trails. The two global trail flags override local controls; when both flags are On, the existing service uses the Very Fast union once.

### 2.7 General → Global Settings — 30 rows including a spacer

1. Wavedash
2. L-Cancel
3. Act OoS Frame
4. Dashback
5. Fighter-specific Tech
6. Powershield Frame
7. SDI Inputs
8. Lockout Timers
9. Item Throw Interrupts
10. Boost Grab
11. Act OoLag
12. Act OoAirborne
13. Jump Cancel Timing
14. Fastfall Timing
15. Frame Advantage
16. Combo Counter
17. Grab Breakout
18. Ledgedash Info
19. Act OoHitstun
20. OVERRIDE CPU OSDS OFF
21. OVERRIDE ALL OSDS OFF
22. **Blank disabled spacer**
23. Hitbox Trails Very Fast
24. Hitbox Trails Instant
25. Missed L Cancel
26. Run Turnaround
27. Actionable Yellow>Green
28. Infinite Shields
29. Invincibility Overlay
30. OSD Display → OSD Display

Rows 1–19 select Off, White, Red, Green, Blue, Yellow, Cyan, Magenta. Rows 20–21 and 23–29 are global toggles. **Infinite Shields here is a shared global preference; the identically named CPU row is a different local policy.** Text suppression does not turn off the visual cues/trails. This distinction needs to remain visible in the new menu.

### 2.8 Global Settings → OSD Display — 3 rows

1. Display Style — Recent / Fixed Grid / Practice Panel
2. Recent Position — HUD / Sides / Top Left / Top Right
3. L/R: OSD Page — informational instructions, not an editable setting

Recent Position affects the Recent layout; stable layouts use the top left. Manual OSD paging already uses paused L/R input. A new menu tab/page binding must account for that existing behavior.

### 2.9 Main Menu → CPU Options — 24 rows

1. CPU Percent
2. Lock CPU Percent
3. Tech Options → Tech Options
4. Trajectory DI
5. Custom TDI → custom stick editor
6. Smash DI Amount
7. Smash DI Direction
8. ASDI
9. Behavior
10. Counter Action (Ground)
11. Counter Action (Air)
12. Counter Action (Shield)
13. Counter Delay
14. Advanced Counter Options → Advanced Counter Options
15. Infinite Shields
16. Infinite Shields Health
17. Shield Angle
18. Intangibility
19. Grab Escape
20. Grab Release
21. Move CPU — becomes **Finish Moving CPU** while repositioning
22. Controlled By
23. Freeze CPU
24. Recovery Options → supported CPU character's recovery menu, otherwise disabled

Freeze CPU is currently an action, not a normal On/Off row. Do not assume it can be converted into a Boolean without checking its callback/state behavior.

### 2.10 CPU Options → Tech Options — 16 rows

1. Tech Option
2. Get Up Option
3. Tech Invisibility
4. Tech Invisibility Delay
5. Tech Sound
6. Simulate Tech Trap
7. Tech Lockout
8. Tech in Place Chance
9. Tech Away Chance
10. Tech Toward Chance
11. Miss Tech Chance
12. Miss Tech Wait Chance
13. Stand Chance
14. Roll Away Chance
15. Roll Toward Chance
16. Getup Attack Chance

Tech and getup chance controls have runtime availability tied to their corresponding Random selections. Probability callbacks rebalance related values. Miss Tech Wait Chance is separate from the four Stand/Roll/Attack chances; preserve that distinction.

### 2.11 CPU Options → Advanced Counter Options — 8 rows per hit number

1. Hit Number
2. Counter Logic
3. Counter Action (Ground)
4. Counter Action (Air)
5. Counter Action (Shield)
6. Delay (Ground)
7. Delay (Air)
8. Delay (Shield)

Hit Number selects one of ten records. Counter Logic gates the six custom action/delay rows. Show the reason for disabled rows rather than hiding the existence of custom controls.

### 2.12 CPU Options → Recovery Options — conditional menus

| CPU / menu title | Rows, in current order |
| --- | --- |
| Fox / Fox Recovery | 1. Firefox Low; 2. Firefox Mid; 3. Firefox High; 4. Double Jump; 5. Illusion; 6. Fast Fall |
| Falco / Falco Recovery | 1. Firebird Low; 2. Firebird Mid; 3. Firebird High; 4. Double Jump; 5. Phantasm; 6. Fast Fall |
| Sheik / Sheik Recovery | 1. Vanish to Ledge; 2. Vanish to Stage; 3. Vanish High; 4. Double Jump; 5. Fair; 6. Amsah Tech |
| Captain Falcon / Falcon Recovery | 1. Falcon Dive; 2. Drift Back; 3. Double Jump; 4. Fast Fall; 5. Falcon Kick |
| Marth / Marth Recovery | 1. Dolphin Slash Early; 2. Double Jump; 3. Dancing Blade; 4. Fair |

All are Boolean permissions. `Event_Init` enables every option in the selected recovery menu, overriding individual static initializers. Other CPU characters have no recovery-options submenu in this implementation.

### 2.13 Main Menu → Recording — 18 rows

1. Save Positions — becomes **Restore Positions** once the initial state exists
2. HMN Mode
3. HMN Record Slot
4. CPU Mode
5. CPU Record Slot
6. Mirrored Playback
7. CPU Counter
8. Loop Input Playback
9. Auto Restore
10. Start Paused
11. Playback Takeover
12. Re-Save Positions
13. Prune Positions
14. Delete Positions
15. Slot Management → Slot Management
16. Set HMN Chances → HMN Playback Slot Chances
17. Set CPU Chances → CPU Playback Slot Chances
18. Export → custom memory-card export flow

On a fresh session, all rows except Save Positions are disabled by `Record_Init`. Saving, deleting, importing, selecting mirrored playback, and record-mode changes update availability. Some playback features depend on compatible state/mirroring conditions. The new UI must represent those dependencies and keep saved recordings compatible.

### 2.14 Recording → Slot Management — 6 rows

1. Player
2. Slot
3. Modify Inputs → Alter Inputs
4. Delete Slot
5. Copy Slot To
6. Copy Slot

### 2.15 Slot Management → Modify Inputs (title: Alter Inputs) — 13 rows

1. Frame
2. Stick X
3. Stick Y
4. C-Stick X
5. C-Stick Y
6. Analog Trigger
7. A
8. B
9. X
10. Y
11. Z
12. L
13. R

Frame is an editor selector, not navigation depth. This screen currently scrolls. A two-page editor with Frame repeated as a reference control is a better candidate than another chain of submenus.

### 2.16 Recording → Set HMN Chances / Set CPU Chances — 7 rows each

Both menus use the same order but operate on separate recording banks.

1. Slot 1
2. Slot 2
3. Slot 3
4. Slot 4
5. Slot 5
6. Slot 6
7. Random Percent

Slot chance rows begin disabled and become usable when recordings exist. Empty slots must not become selectable simply because a new presentation list contains them. Probability rebalancing and separate human/CPU Random Percent values must remain intact.

### 2.17 Main Menu → HMN Info Display / CPU Info Display — 10 rows each

1. Display Preset
2. Size
3. Row 1
4. Row 2
5. Row 3
6. Row 4
7. Row 5
8. Row 6
9. Row 7
10. Row 8

Preset choices are None, State, Ledge, Damage; sizes are Small, Medium, Large. Each Row has these choices in existing value order:

> None; Position; State Name; State Frame; Velocity - Self; Velocity - KB; Velocity - Total; Engine LStick; System LStick; Engine CStick; System CStick; Engine Trigger; System Trigger; Ledgegrab Timer; Intangibility Timer; Hitlag; Hitstun; Shield Health; Shield Stun; Grip Strength; ECB Lock; ECB Bottom; Jumps; Walljumps; Can Walljump; Jab Counter; Line Info; Blastzone Left/Right; Blastzone Up/Down.

These are field choices within the eight rows, not 29 additional submenus. Preserve their identities when changing labels.

### 2.18 Main Menu → Character RNG — dynamic order, 8 reserved entries

The menu appends the human fighter's supported rows first, then the CPU fighter's rows if its option-array pointer differs. A same-character pair shares the rows instead of duplicating them. This is **not** a fixed five-row menu.

| Fighter | Its rows, in order |
| --- | --- |
| Peach | 1. Peach Turnip; 2. Peach Forward Smash |
| Luigi | 1. Luigi Misfire |
| Game & Watch | 1. GnW Hammer |
| Ice Climbers | 1. Nana Throw |

Example: human Luigi / CPU Peach displays Luigi Misfire, Peach Turnip, Peach Forward Smash. Human Peach / CPU Peach displays only Peach Turnip and Peach Forward Smash. No supported fighter means the root row is disabled. `OPTCHARRNG_MAXCOUNT` reserves eight entries; the runtime assembly can produce up to three with the current fighter table.

### 2.19 Main Menu → Stage Options — conditional

| Stage | Rows, in current order | Choice order |
| --- | --- | --- |
| Pokemon Stadium | 1. Transformation | Normal, Fire, Grass, Rock, Water |
| Fountain of Dreams | 1. Left Platform Height; 2. Right Platform Height | Random, Hidden, Lowest, Left Default, Average, Right Default, Highest |
| Other stages | No active submenu | Stage Options remains disabled. |

Stadium's current description says it requires Stage Hazards and is experimental with issues. Keep that context in any restyle. A stage-hazards toggle is not an existing row in this Lab pause menu.

### 2.20 Main Menu → Controls — 6 rows

1. Frame Advance Button — L, Z, X, Y, R
2. Frame Decrement Button — None, Z, L, R, X, Y
3. DPad Up — Taunt, None
4. DPad Down — Place CPU, None, Frame Advance
5. DPad Left — Load State, None
6. DPad Right — Save State, None

These value orders are serialized. Keep them unchanged or supply an explicit migration.

### 2.21 Custom screens and related interfaces

These are part of the experience but are not ordinary `EventOption` lists:

| Entry point | Current controls / sequence |
| --- | --- |
| CPU Options → Custom TDI | Live main-stick/C-stick input; A: Save Input; X: Delete Input; B: Return; Z: Reversible. Saved inputs are displayed in sequence. |
| Recording → Export, step 1 | Select a Memory Card: Slot A, Slot B. |
| Export, step 2 | Enter File Name: character keyboard; A: Select; B: Backspace; Y: Caps; X: Space; Start: Confirm. Filename and stage/HMN/CPU/card details appear. |
| Export, step 3 | Save File to Slot A/B? Yes, No; asynchronous success/error status follows. |
| Lab character-select Import, outside pause menu | Select Memory Card A/B → Select Recording → confirmation. File list provides A: Select; B: Return; X: Delete; Y: Swap Sheik/Zelda; load/delete confirmations offer Yes/No, while incompatible versions or no recordings show OK. |

Do not add an in-match Import row to the current inventory; there is none. Moving Import into the pause menu would be a separate behavioral feature requiring investigation.

### 2.22 Existing quick access

Holding Y invokes the Lab shortcut list, including from descendant menus; the renderer propagates the shortcut pointer when entering a child. For a submenu shortcut it walks back to the root before entering the target. The controls and targets are:

| While holding Y | Existing target |
| --- | --- |
| A | Frame Advance toggle |
| X | Model Display |
| D-pad Left | Tech Options |
| D-pad Right | Slot Management |
| D-pad Up | Global Settings |
| D-pad Down | Recording |
| Z | Re-Save Positions |

Unpaused configurable D-pad assignments in Controls are a different input path. Preserve these shortcuts, or record an intentional remapping. Avoid treating Y as a free new tab-modifier without resolving the shortcut conflict.

## 3. What currently makes the menu difficult to scan

| Screen | Existing rows | Problem |
| --- | ---: | --- |
| General | 19 | Mixes simulation, human setup, visual debugging, OSD tools, and shared preferences. |
| CPU Options | 24 | Mixes damage, DI, tech, counters, shield, positioning, and recovery. |
| Global Settings | 30, including spacer | Nineteen OSD colors and unrelated global flags form one long list. |
| Recording | 18 | Mixes capture, playback, initial state, slot editing, and export. |
| Tech Options | 16 | Probabilities and diagnostic/trap controls compete with the basic mode selectors. |
| Each actor's Overlays | 17 | Movement, recovery, combat, and protection conditions have little visible structure. |
| Alter Inputs | 13 | Buttons and analog controls make one tall editor. |
| Each Info Display | 10 | A nearly fitting editor exceeds the nine-row viewport by one. |

The root is already compact. The main improvement is splitting its long descendants according to the job the player is doing. Smaller text or a taller list would retain the underlying scanning problem.

## 4. View two: recommended semantic tree

**Target: 5–9 selectable rows per ordinary screen, fewer where a feature is small.** Count the root as depth 0: normal controls should take one or two submenu entries; specialist editors may take three. Do not add a fourth nested level. Tabs/pages within one editor do not add tree depth, but still need visible page names and input hints.

Root order: **Session; CPU; Recording; Visual Feedback; Global Settings; Stage & RNG; Controls; Exit.** Eight rows. Global Settings is promoted so cross-event preferences are easy to find. Visual Feedback is a functional category, not a promise that all of its settings are unsaved.

All row names in this section refer to existing controls unless marked proposed. A slash describes a runtime alternate label or actor-specific pair, not a new combined setting.

### 4.1 Session — 5 rows, depth 1

1. Frame Advance
2. Game Speed
3. Player Percent
4. Lock Player Percent
5. Move Staling

Use two labeled blocks: **Simulation** (Frame Advance, Game Speed), **Fighter Setup** (percent/lock/staling). These frequently used controls do not need separate two-row submenus. Keep controller bindings in Controls.

### 4.2 CPU — 9 rows, depth 1

1. CPU Percent
2. Lock CPU Percent
3. Behavior
4. DI & Survival → 7 rows
5. Tech & Getup → 5 rows
6. Counter Actions → 5 rows
7. Shield & Protection → 4 rows
8. Position & Control → 3 rows
9. Recovery Options → existing character menu, 4–6 rows

| Child screen, depth 2 | Proposed row order | Count / block treatment |
| --- | --- | --- |
| DI & Survival | Trajectory DI; Custom TDI; Smash DI Amount; Smash DI Direction; ASDI; Grab Escape; Grab Release | 7. DI / Grab blocks. |
| Tech & Getup | Tech Option; Get Up Option; Tech Chances →; Getup Chances →; Tech Feedback & Traps → | **5** actual rows; the hub deliberately leaves room rather than filling all nine. |
| Counter Actions | Counter Action (Ground); Counter Action (Air); Counter Action (Shield); Counter Delay; Advanced Counter Options → | 5. Defaults followed by per-hit customization. |
| Shield & Protection | Infinite Shields (local); Infinite Shields Health; Shield Angle; Intangibility | 4. Shield / Protection blocks; display global override context. |
| Position & Control | Move CPU / Finish Moving CPU; Controlled By; Freeze CPU | 3. No further submenu. |
| Recovery Options | Existing character-specific list from section 2.12 | 4–6. Keep character name in title. |

Intangibility has one canonical home under Shield & Protection. The CPU row for Tech & Getup should preview its five child entries.

At depth 3, Tech & Getup has these terminal editors:

- **Tech Chances — 4 rows:** Tech in Place Chance; Tech Away Chance; Tech Toward Chance; Miss Tech Chance.
- **Getup Chances — 5 rows:** Miss Tech Wait Chance; Stand Chance; Roll Away Chance; Roll Toward Chance; Getup Attack Chance.
- **Tech Feedback & Traps — 5 rows:** Tech Invisibility; Tech Invisibility Delay; Tech Sound; Simulate Tech Trap; Tech Lockout. Use Feedback and Trap blocks.

Counter Actions → Advanced Counter Options retains all eight rows from section 2.11 at depth 3. Custom TDI remains its custom editor, also at depth 3. No further grouping inside either editor.

### 4.3 Recording — 9 rows, depth 1

1. Save Positions / Restore Positions
2. HMN Mode
3. HMN Record Slot
4. CPU Mode
5. CPU Record Slot
6. Playback Rules → 6 rows
7. Positions & Files → 4 rows
8. Slot Management → existing 6 rows
9. Playback Chances → 2 rows

Use **Initial State**, **Human Capture**, **CPU Capture**, **Tools** blocks. Keep mode and slot side by side in the navigation sequence for each actor. Replace HMN with Human in presentation labels if we approve that naming change.

| Child screen, depth 2 | Proposed row order | Count |
| --- | --- | ---: |
| Playback Rules | Mirrored Playback; CPU Counter; Loop Input Playback; Auto Restore; Start Paused; Playback Takeover | 6 |
| Positions & Files | Re-Save Positions; Prune Positions; Delete Positions; Export | 4 |
| Playback Chances | Set HMN Chances →; Set CPU Chances → | 2 |

Keep Delete Positions visually separate from routine resaving. Export still launches its existing modal flow. Slot Management is directly on the Recording root so Modify Inputs stays at depth 3. Adding a Slots & Chances hub above Slot Management would create an unnecessary fourth level. Playback Chances has two actor entries leading to their seven-row chance editors at depth 3.

Alter Inputs should become one editor with **Analog** and **Buttons** pages:

- Analog: Frame; Stick X; Stick Y; C-Stick X; C-Stick Y; Analog Trigger — 6 rows.
- Buttons: Frame (same selector, repeated for context); A; B; X; Y; Z; L; R — 8 rows.

The repeated Frame is a view of the same option, not a second frame setting. Keep the selected recording, slot, and frame visible in the editor title.

### 4.4 Visual Feedback — 9 rows, depth 1

1. Model Display
2. Collision & Bounds → 3 rows
3. Camera Mode
4. HUD
5. DI Display
6. Input Display
7. Fighter Displays → 4 rows
8. Hitbox Trails → existing 3 rows
9. OSD Tools → 2 rows

| Child screen | Proposed row order | Depth / count |
| --- | --- | --- |
| Collision & Bounds | Fighter Collision; Item Grab Sizes; Environment Collision | Depth 2, 3 rows. |
| Fighter Displays | HMN Info Display →; CPU Info Display →; HMN Color Overlays →; CPU Color Overlays → | Depth 2, 4 rows. Two clearly labeled actor blocks. |
| OSD Tools | Custom OSDs →; Action Log → | Depth 2, 2 rows. |

At depth 3, retain the actor-specific Info Display and Overlay editors. To avoid scrolling or additional depth:

- **Info Display pages:** Setup (Display Preset, Size); Rows (Row 1 through Row 8). Pages are peers inside the same editor. Alternatively show preset/size as a fixed header and eight editable rows, which requires layout work.
- **Overlay pages:** Movement (Crouch, Wait, Walk, Dash, Run, Jumps, Full Hop, Short Hop — 8); Timing (Actionable, Ledge Actionable, Missed L-Cancel, Can Fastfall, Autocancel, IASA — 6); Combat (Hitstun, Invincible, Shield Stun — 3).
- **Custom OSDs:** retain Add and up to eight Remove rows; compact unused empty capacity in the view if implemented safely.
- **Action Log:** retain its eight-row per-action editor with labeled State, Input Conditions, Display blocks.

This is the tradeoff for keeping Visual Feedback at nine rows: actor-specific editors require three entries from the root. The category-rail alternative can make those routes more direct without increasing the tree depth.

### 4.5 Global Settings — 7 rows, depth 1

1. OVERRIDE ALL OSDS OFF
2. OVERRIDE CPU OSDS OFF
3. Movement & Landing OSDs → 6 rows
4. Combat & Defense OSDs → 8 rows
5. Action Timing OSDs → 5 rows
6. OSD Display → existing 3 rows
7. Global Visuals & Shields → 7 rows

| Child screen, depth 2 | Exact members in proposed order |
| --- | --- |
| Movement & Landing OSDs | Wavedash; L-Cancel; Dashback; Jump Cancel Timing; Fastfall Timing; Ledgedash Info |
| Combat & Defense OSDs | Act OoS Frame; Powershield Frame; SDI Inputs; Boost Grab; Frame Advantage; Combo Counter; Grab Breakout; Act OoHitstun |
| Action Timing OSDs | Act OoLag; Act OoAirborne; Lockout Timers; Item Throw Interrupts; Fighter-specific Tech |
| Global Visuals & Shields | Hitbox Trails Very Fast; Hitbox Trails Instant; Missed L Cancel; Run Turnaround; Actionable Yellow>Green; Invincibility Overlay; Infinite Shields (global) |

All nineteen OSD categories have exactly one home. The last group is a broad action/interrupt bucket; test whether **Action & Interrupt OSDs** is a clearer name. Global Visuals & Shields uses **Trails** (2), **Fighter Cues** (4), **Shield Rule** (1) color blocks. The old blank spacer becomes spacing/block styling rather than an option.

These global rows retain their existing IDs, choice orders, defaults and save behavior. Scope should be visible in the title/detail: **Global — applies across events**. A local shield row should say **Local — overridden while Global Infinite Shields is On**, with the effective behavior shown.

### 4.6 Stage & RNG — 2 rows, depth 1

1. Stage Options → existing stage-specific 1–2 rows
2. Character RNG → existing runtime-assembled 1–3 rows

Keep unavailable entries visible with reasons such as `No stage controls on Final Destination` or `No RNG controls for these fighters`. A stable two-row hub is easier to locate than a shifting root. For a relevant fighter/stage, show the actual available option names in the preview before entry.

### 4.7 Controls — existing 6 rows, depth 1; Exit — root action

Keep the Controls order from section 2.20. Use **Frame Stepping** and **D-pad Shortcuts** blocks. Exit remains immediately visible at the end of the root list or category rail; modal export/custom editors must still close safely before returning to Event Select.

### 4.8 Coverage and depth checks

| Current origin | Recommended destination | Coverage |
| --- | --- | --- |
| General's simulation/player rows | Session | All 5. |
| General's display/collision rows | Visual Feedback + Collision & Bounds | Model, all 3 collision controls, camera, HUD, DI and input. |
| General's actor overlays | Visual Feedback → Fighter Displays | Both actors, all 17 conditions each. |
| Main's info displays | Visual Feedback → Fighter Displays | Both actors, preset/size/all 8 rows each. |
| General's OSD tools/trails | Visual Feedback → OSD Tools / Hitbox Trails | Add/remove custom states, all Action Log rows, all 3 trail rows. |
| General's Global Settings | Root Global Settings | All 19 palette rows, 9 toggles, OSD Display; spacer removed from navigation. |
| CPU Options and descendants | CPU and its child screens | All 24 hub rows, 16 tech rows, 8 advanced-counter rows, all 5 recovery variants, Custom TDI. |
| Recording and descendants | Recording and its child screens | All 18 existing rows; all 6 slot-management, 13 input-editor, and 7+7 probability rows; export flow retained. |
| Character RNG / Stage / Controls / Exit | Stage & RNG / Controls / Exit | All conditional variants and actions retained. |

Longest ordinary paths are **3 submenu entries**, for example Recording → Slot Management → Modify Inputs, CPU → Counter Actions → Advanced Counter Options, and Visual Feedback → Fighter Displays → HMN Color Overlays. All ordinary leaf pages fit in nine rows. Export's card/name/confirmation steps are a task flow, not additional generic settings-tree depth.

## 5. View three: options by common task

Depth here counts entering a top-level category from the root, then any child screens. The controller prototype's category selector avoids opening a separate root list when switching categories; the category contents appear as soon as its selector changes.

| Player intent | Proposed route and relevant controls | Entries |
| --- | --- | ---: |
| Frame-step or slow down | Session: Frame Advance, Game Speed; Controls for bindings | 1 |
| Set starting damage | Session: Player Percent/Lock; CPU: CPU Percent/Lock | 1 per actor |
| Practice DI | CPU → DI & Survival: TDI, SDI, ASDI; Custom TDI opens the stick editor | 2; 3 for editor |
| Practice tech chases | CPU → Tech & Getup: Tech/Get Up; chance and trap editors one step further | 2–3 |
| Practice punish windows | CPU → Counter Actions: ground/air/shield action and delay; Advanced for per-hit rules | 2–3 |
| Practice shield pressure | CPU → Shield & Protection: local policy/health/angle; Global Settings → Global Visuals & Shields for shared Infinite Shields | 2 |
| Practice edgeguards | CPU → Recovery Options; CPU → Position & Control to move/freeze/assign a port | 2 |
| Record and replay a sequence | Recording: positions, actor modes and slots; Playback Rules for looping/restore/takeover | 1–2 |
| Edit one recorded frame | Recording → Slot Management → Modify Inputs: Analog/Buttons pages | 3 |
| Randomize playback | Recording → Playback Chances → human/CPU chance editor | 3 |
| Inspect collision or inputs | Visual Feedback: Model/DI/Input; Collision & Bounds for collision detail | 1–2 |
| Configure actor diagnostics | Visual Feedback → Fighter Displays → actor info/overlay editor | 3 |
| Show timing feedback | Global Settings → relevant OSD group; OSD Display for layout | 2 |
| Add custom state feedback | Visual Feedback → OSD Tools → Custom OSDs / Action Log | 3 |
| Control platforms or fighter RNG | Stage & RNG → applicable child | 2 |
| Save recording to card | Recording → Positions & Files → Export, then card/name/confirmation flow | 3 + flow |

This view also defines a usability review: a person should be able to predict the category for each task without memorizing the old General menu.

## 6. Styling research: zebra rows and semantic color blocks

### 6.1 What the current renderer actually supports

`MENU_MAXOPTION` is 9 and `MENU_DESCLINEMAX` is 4. `EventMenu_CreateModel` creates a popup-model rowbox for each visible slot. Those rowboxes are **behind the value column**, not across the full option label. The code changes joint corner positions and writes `mobj->mat->diffuse` plus alpha. The selected row already has a separate full-width yellow highlight, at 0.4 alpha. Ordinary value boxes use a dark fill at 0.6 alpha; enabled toggles use green.

`EventMenu_UpdateText` resets rowbox diffuse every refresh and hides the box for ordinary submenu rows and actions without a value string. **Changing only `ROWBOX_COLOR` will not create full-width alternating rows**, and color set only at model creation will be overwritten on update. Full-row backgrounds require widening/repositioning these boxes with changed visibility rules, or adding a separate full-row background layer while preserving value boxes.

Recommendation: separate **row surface**, **value presentation**, and **selection**. A full-row surface supplies zebra/group shading. Value text always carries On/Off/current selection. The selected row retains an unmistakable outline/marker or high-contrast band. No user should have to infer selection or On status from a background color.

### 6.2 Three treatments to compare

| Treatment | Implementation concept | Best use / limitation |
| --- | --- | --- |
| Zebra | Alternate two low-contrast row fills by absolute displayed row index, including scroll offset. | Helps track label-to-value alignment; does not explain relationships by itself. |
| Semantic blocks | Give contiguous related controls a shared muted fill plus a visible group label or thin labeled boundary. | Makes Simulation, DI, Human Capture, etc. identifiable; labels must carry meaning independently of hue. |
| Combined — recommended | Group hue establishes the block; slight luminance alternation separates its rows. Selection stays a separate layer. | Supports both scanning and semantics; keep saturation/contrast restrained so selection wins. |

Example native starting palette for an opaque menu surface: neutral rows `#191D29` / `#222838`; DI block `#17303B` / `#1E3A47`; tech block `#29243B` / `#332C49`; shield block `#32301E` / `#3E3A25`. These are **proposed trial RGBs**, not validated contrast or final artwork. Color pairs should be tested over the actual DAT background and effective alpha. Use bright readable labels and an independent selection treatment.

Semantic examples that should remain on one screen:

- Session: Simulation (2 rows), Fighter Setup (3).
- DI & Survival: Trajectory DI/Custom TDI (2), SDI/ASDI (3), Grab (2).
- Recording root: Initial State (1), Human Capture (2), CPU Capture (2), Tools (4).
- Advanced Counters: Context (Hit Number/Counter Logic), Ground, Air, Shield pairs. A three-pair layout or reordered view can visually pair action and delay without changing backing indices.
- Global Visuals & Shields: Trails (2), Fighter Cues (4), Shield Rule (1).

Avoid spending selectable rows on empty spacer options or headings. Render headings in gaps or a small fixed section caption. If gaps reduce the available viewport, remeasure the row budget rather than claiming nine rows still fit.

### 6.3 Required visual and interaction rules

- Use explicit submenu chevrons, values, and action labels; do not make a blank value column the only indication of a submenu.
- Keep consistent value alignment; long values should receive a deliberate abbreviated display plus the full meaning in details.
- Preserve selected-row contrast across all fills; don't use the green On fill as the selected state.
- Distinguish disabled controls with readable dim text and a reason; the present disabled color uses alpha 0, so verify native text alpha behavior before copying it into a redesign. Current navigation skips disabled rows: either render their reason beside the label, or allow inspection focus while continuing to block editing/activation.
- Base zebra parity on the visible view's absolute row index, not just screen slot, to prevent flips when scrolling. Apply group membership by stable option identity.
- Label Local / Global / Saved scope where known. Do not claim all Lab options persist: current settings persistence varies by feature.
- Prefer paging the dense specialist editors to reducing their text size.
- Keep native input glyph/font constraints in mind; the existing ASCII table has project modifications. Check glyph support before adding Unicode arrows or bullets to game labels.

## 7. Submenu descriptions: purpose plus included options

### 7.1 Available now without a renderer redesign

`EventOption.desc` already stores four strings. `EventMenu_CreateText` creates four subtexts and `EventMenu_UpdateText` fills them from the highlighted option. The current system can therefore show **one line of purpose and three lines listing contents** before A enters a submenu. No additional settings storage is needed.

Example for the proposed Counter Actions entry:

```text
Choose how the CPU responds after a hit.
Includes: Ground, Air, Shield actions;
Counter Delay; Advanced Counter Options.
Advanced: per-hit actions and delays.
```

Example for Stage Options on Fountain of Dreams:

```text
Set the platform heights on this stage.
Includes: Left Platform Height;
Right Platform Height.
Applies to Fountain of Dreams.
```

Those examples need native text-fit testing. The aspect setting is not a verified wrapping/overflow policy, and the renderer simply supplies four strings; do not assume unlimited text wraps safely.

### 7.2 Recommended behavior

Give each submenu a purpose plus its **immediate child names**, in displayed order. Do not dump the entire descendant tree. Terminal screens should preview every option before entry; broad hubs can list their immediate child categories. For long names, use approved short aliases and preserve full labels in the child menu.

For the modest tree here, authored four-line previews are the smallest first implementation. A more robust presentation layer could generate contents from the child view, refresh when dynamic availability changes, and use separate purpose/contents text regions. Keep runtime strings in persistent owned buffers; never leave text pointing at stack storage or pass generated percent signs as formatting instructions.

| Case | Preview behavior |
| --- | --- |
| Ordinary submenu | Purpose + all immediate child names. |
| Stage Options | Actual current-stage labels; unavailable reason if no submenu. |
| Character RNG | Actual assembled labels in HMN-then-CPU order; no generic promise of all five controls. |
| Recovery Options | Current character and all applicable recovery toggles. |
| Disabled child | Include it with its reason when relevant; don't hide important dependencies. |
| Saved/global settings | Add scope where it helps the player predict impact. |
| Global override | Explain the effective value and how to change the source preference. |
| Contents exceed three lines | Use a deliberate expanded preview layout or bounded page mechanism; do not silently cut off entries. |

A layout redesign should reserve a fixed detail area: purpose, **Includes:** list, context/availability. Selection changes update the preview; opening the child should preserve the same names/order. Entering and returning should retain the selected row and page, so exploration does not reset the player to the top.

## 8. Controller-first UI: visible category tabs + selections and description panels

**October 7 follow-ups:** the user requested a fresh direction optimized for a joystick that moves one direction at a time, with A to enter and B to go back, followed by persistent category tabs and a split menu. The earlier mouse-oriented rail/tree comparison is superseded. Below the tabs, the new demo has two equal-width panels: selections/current values on the left, and purpose/contents/context on the right. Each submenu option occupies its own preview row; paged editors separate those rows under page headings. There is one controller cursor for tabs and the left list; the right panel is read-only and updates with the selection. On narrow browser screens the panels stack to preserve readability; native two-panel fit still needs measurement.

The accompanying interactive sketch is [training-lab-menu-demo.html](docs/training-lab-menu-demo.html). Its entire menu is reachable through four directions and A/B. Keyboard arrows simulate the stick; A/Enter enters or confirms; B/Escape backs out. Six on-screen buttons simulate those same inputs for touch/mouse review. A standard browser-mapped gamepad can use its stick/D-pad and south/east A/B buttons; nonstandard adapters require external keyboard mapping. Actual hardware support remains to be tested. Illustrative values change locally; the sketch does not control the game or prove native console fit.

The demo retains section 4's semantic destinations and the sample Peach/Fox match on Fountain of Dreams. Exit is the eighth category-selector position, containing Return to Event Select and Resume Practice. Starting category is Session. Global scope, actor-specific settings, and the catalog of conditional variants remain unchanged. Combined zebra/block styling is the default; optional design controls compare Combined, Zebra, and Blocks. Custom TDI and Export remain destination sketches rather than native editor/card-flow emulations. Values/ranges, availability, probability balancing, and per-hit/per-action data remain illustrative.

### 8.1 Controller rules

| Focus / situation | Up / Down | Left / Right | A | B |
| --- | --- | --- | --- | --- |
| Category selector | Enter first/last row | Previous/next category, one at a time | Enter the remembered row | Resume practice |
| Category's option list | Move one row; boundaries wrap through selector | Change a focused setting by one value; submenu rows have no horizontal action | Open submenu, value picker, or action | Focus category selector |
| Nested submenu | Move one row; wrap within list | Change a focused setting by one value | Open/activate focused row | Return to the exact parent row/page |
| Paged editor's selector | Enter first/last row | Previous/next page | Enter list | Return to parent |
| Value picker | Move one choice; wrap at ends | No action | Apply highlighted choice | Cancel without changing value |

The top tab bar is part of the same controller focus sequence as the list. **All eight main categories are visible:** Session, CPU, Recording, Visual Feedback, Global Settings, Stage & RNG, Controls, Exit. The active tab stays highlighted even inside a submenu or value picker. An additional marker/outline indicates when controller focus is on that tab rather than a menu row. Left/Right moves one tab when the bar has focus; A/Down enters the list. B at a category root reaches the tabs in one press; the user does not have to climb through all rows to switch categories. A remembers the last focused row in a category, while Down explicitly enters its first row. Returning from a child preserves the parent's cursor, page, and viewport. Nested screens expose their breadcrumb without turning it into another focus region. At narrow browser widths the tabs form two rows, keeping every label visible without horizontal scrolling; Left/Right still follows their displayed order.

Left/Right editing is deliberately restricted to the focused setting. Value pickers support a longer list with a maximum of nine visible choices and a position counter; the viewport follows the controller cursor. Pages such as Analog/Buttons and overlay groups use a focused page selector with the same directions and A/B, requiring no shoulder buttons or mouse tabs. Long values are displayed in the right column and explained in the footer.

Keyboard repeat is ignored: each direction press moves once. The gamepad path similarly requires release/recentering between movements, resolves a diagonal stick to its dominant axis, and uses separate engage/release thresholds. A/B activate only on a press edge. Connection with a held input does not immediately activate an option. All input sources call the same navigation state machine; hover does not move the cursor. Pointer clicks on a row only select it, and the simulator's A activates it.

| Alternative | Benefit | Cost / recommendation |
| --- | --- | --- |
| Reorganized existing list | Familiar controls, easiest incremental change, existing DAT background. | More backtracking between categories; recommended first game implementation. |
| Category rail + short list + detail footer | Category stays visible; purpose/contents have a permanent home. | Historical option; superseded by the single-cursor controller selector after user feedback. |
| Two-column dashboard of settings cards | Related controls can be viewed simultaneously. | Less predictable controller navigation, tighter value widths and more visual density on CRT; not recommended first. |
| Search-first interface | Fast on a keyboard. | Poor fit for a GameCube controller; category navigation must remain primary. |

This direction reserves no L/R/Y bindings, leaving existing OSD paging, frame stepping, and hold-Y shortcuts for the later native implementation to preserve. The browser sketch's input behavior is implemented; integration with native pause handling is not.

The category rail remains a historical alternative in the comparison above, not the current recommended demo. Prototype the single-list layout at 640×480 with real fonts and overscan-safe bounds before committing native geometry. The browser's responsive layout is useful for review, not evidence of CRT fit.

## 9. Implementation constraints and a staged approach

### 9.1 Preserve behavior while changing presentation

The backing arrays are more than labels. Code directly references `LabOptions_General[OPTGEN_*]`, `LabOptions_CPU[OPTCPU_*]`, recording arrays, chance arrays, and overlay IDs. `Lab_ChangeOSDs` currently checks the active menu and converts its row position through `LabOSD_ID`. Splitting that menu without replacing the row-to-ID mapping would make edits fail or affect the wrong category. Copying `EventOption` structs into independent new arrays can also create stale `val`/`disable`/runtime-label state.

Recommended design: a **view of references to canonical options**, plus per-view metadata (section, short preview alias, optional display label). An accessor maps view index to the original option. Adapt navigation, drawing, selection, and callbacks to use that accessor; avoid assuming that every event has been migrated. The existing contiguous-array interface must keep working for other events. A Lab-only adapter or appended optional view pointer is safer than replacing every event's menu representation at once.

Review callbacks that infer identity from `curr_menu`, cursor or scroll. In particular: OSD palette writes, custom OSD removal, per-hit counters, Action Log selection, dynamic recording labels, local/global shield refresh, and slot-chance rebalance. `Lab_RemoveCustomOSD` currently derives its index from the cursor alone; a reworked scroll/page view needs the selected canonical identity. Do not carry that positional assumption into a new renderer.

Preserve exported recording value IDs and the serialized control choice orders noted in `lab.h`. Labels and view order may change; storage order must not casually change. The current metadata bump does not change the save format. New grouping/colors/previews are compiled UI data and runtime objects; they do not inherently consume persistent preference bits. Saving a preferred menu theme/category would be a separate storage decision.

### 9.2 Shared renderer and rebuild impact

`menu.c` and `menu.h` serve multiple events, not only Lab. Full-row styling should be opt-in per menu/theme or provide a conservative default for existing events. Adding fields changes sizes/ABI; rebuild every affected module. The current `settings_abi_stamp` in `build.sh` does not include `src/menu.h`; add the appropriate dependency/fingerprint if the menu structure is extended. A version change already forces a full build, but subsequent T3 header edits need reliable invalidation too.

The DAT asset references (`evMenu.menu`, `popup`, `scroll`) support geometry manipulation already used in C. A basic color/row change likely does not need new artwork; the rail/footer may need altered geometry or a new asset. That remains a prototype question, not a confirmed asset-free implementation. Check material sharing so per-row diffuse changes do not recolor unrelated instances.

### 9.3 Proposed phases

1. **Approve organization:** finalize the canonical destinations, depth ceiling, and labels from section 4. Keep this document as the decision log.
2. **Introduce views:** map existing options into short screens; explicitly update callbacks and shortcuts. Keep values/save format/recording IDs stable.
3. **Add descriptions:** purpose + contents + dynamic unavailable/override explanations; validate text fit using real labels.
4. **Restyle the existing renderer:** full-width backgrounds, zebra/block treatment, clear selection/value distinction and subsection captions.
5. **Evaluate the controller layout:** prototype the category/page selector and focus retention with native fonts/input; measure access and fit against the reorganized list.
6. **Verify and record implementation:** add implemented changes to the README T3 changelog and dated sections here. Mark live checks only after they are actually performed.

## 10. Acceptance checks for later implementation

- Every current row and conditional variant in section 2 has a reachable home; deliberate removals/renames are recorded explicitly.
- Root has eight rows; no ordinary page exceeds nine selectable rows; no generic settings path exceeds three entries.
- Submenu highlighting shows both purpose and all immediate contents before entry, including relevant unavailable reasons.
- Selection, On/Off, disabled status, action vs submenu, and local vs global scope remain understandable without color.
- Frame advance/decrement, hold-Y shortcuts, paused OSD paging, and ordinary Left/Right edits have no accidental new conflicts.
- Saving/restoring/deleting positions, recording/playback/re-record, mirroring, takeover, probability editing, copy/delete slots, and export retain behavior.
- Human/CPU settings remain distinct. Global edits agree with the native OSD editor and keep override precedence.
- Character RNG order/deduplication, stage availability, five recovery variants, empty/full custom OSD lists, and all ten counter/Action Log pages work.
- Scene changes, custom-editor return, pause/unpause, cursor/page retention, font support, long labels, and overscan fit are checked in Dolphin.
- Other events using the shared renderer remain functional; optimized DAT/release builds are checked if headers or engine rendering change.

## 11. Research validation and decision log

**October 7, 2026 — initial proposal.** Source definitions and runtime modifications were inspected. The centralized version is V1.4.1T3; the save identity stays TYRE01. This document is the starting inventory and design proposal. The demo is illustrative. No native menu implementation or new ISO is claimed by this research pass.

Completed checks for the initial, now-superseded sketch:

- Source-label coverage: every nonempty literal menu/option name in `lab.h` and `recovery.c`, plus the expanded shared global labels, appears in the inventory document. Dynamic inserted rows and alternate names were inspected separately in `lab.c`.
- `./build.sh --version`: reports V1.4.1T3, TYRE01, and `TM-Tyro-V1.4.1T3.iso`.
- Demo JavaScript syntax check passed. A temporary DOM harness exercised 44 menu destinations / 51 pages in both designs: row counts, links, entry/back navigation, focus previews, value changes, and the shared Analog/Buttons Frame selector passed. The menu graph has maximum depth 3 from the root.
- Whitespace/content checks passed. No browser surface was available for screenshot/layout inspection; the harness is not browser-rendering validation. Native Dolphin/CRT fit and gameplay checks remain pending.

Completed checks for the controller revision: JavaScript syntax passed; a temporary DOM harness traversed **44 reachable destinations and 261 row appearances** using only directional inputs and A/B. It checked submenu purpose/contents previews, exactly one controller selection, nine-row view limits, parent row/page/viewport restoration, category wrap and remembered rows, value-picker apply/cancel, one-value horizontal edits, keyboard repeat suppression, simulator dispatch, and simulated gamepad press edges/neutral thresholds/dominant diagonal direction. It also checked saved-state restoration and rejection of invalid menu destinations. These are code-level input checks; browser layout, actual gamepad hardware, and native game integration are still unverified.

Record future decisions using this format:

| Date | Decision / implemented change | Reason | Validation / remaining work |
| --- | --- | --- | --- |
| 2026-10-07 | Start T3 menu investigation and version bump. | Establish a complete inventory before reorganizing. | Source audit; metadata/diff checks. Native layout/gameplay validation remains pending. |
| 2026-10-07 | Replace rail/tree sketch with a single-cursor controller menu. | User prioritizes joystick directions plus A/B, with easy access to elements. | Browser prototype navigation implemented; native integration and actual hardware/visual checks remain pending. |
| 2026-10-07 | Show all main categories as persistent top tabs. | User wants the selected category and adjacent choices visible while navigating Left/Right. | Active-category and controller-focus indicators are separate; all tabs remain visible in descendants and pickers. |
| 2026-10-07 | Split the menu into equal selection/value and description panels beneath the tabs. | User wants descriptions beside the options, with each submenu member on its own row. | Read-only preview uses separate rows and page headings; controller focus remains on tabs and selections. |
| 2026-10-07 | Mark nested submenu entries in Included options with a right-aligned `>` symbol. | Distinguish another menu from a setting or action before entry. | Preview rows retain their menu target; only entries with a target receive the marker and accessible submenu label. |

**Open design choices:** approve the final nine-row Recording hub; decide whether the extra Fighter Displays hub is worth its compact Visual Feedback root; choose the Action Timing group name; choose whether Human replaces HMN in display labels; select styling treatment; measure the controller selector/list layout natively. The mouse-oriented rail is no longer the active direction.

## 12. First native implementation — October 7, 2026

The user authorized implementing the accepted prototype in Training Lab and requested moving Boost Grab into combat OSDs. The native implementation is in [src/lab_menu.h](src/lab_menu.h), with the shared renderer opt-in in [src/menu.h](src/menu.h), [src/menu.c](src/menu.c), [src/menu_controller_ui.h](src/menu_controller_ui.h), and [src/menu_controller.h](src/menu_controller.h). The README now records implemented behavior rather than only the sketch.

Implemented:

- Eight persistent category tabs: Session, CPU, Recording, Visuals, Global, Stage/RNG, Controls, Exit. Native tab labels use compact aliases; the panel titles retain the full category names.
- Two equal-width panels beneath the tabs: canonical options/current values on the left; description and one-row-per-option Included options on the right. Nested menus carry a `>` marker. Paged editors preview their members under page headings.
- Nine-row viewports, bounded semantic submenus, and peer pages for actor Info Display, actor Overlays, and Alter Inputs. Boost Grab now belongs only to **Combat & Defense OSDs**; Movement has six rows, Combat eight, Action Timing five.
- Separate active-tab and option-selection cues, alternating row luminance, and muted semantic color blocks. Disabled options are visible for inspection but block activation/editing; their generic unavailable context appears in the footer.
- One movement per native pad press/deflection. Up/Down traverses rows; at tab roots the selector is part of that sequence. Left/Right changes a tab/page when its selector is focused or changes an editable value by one. A enters menus or a value picker. B returns to the parent row/page, focuses the tabs at a category root, or resumes when already on the tabs. Start also resumes.
- Value pickers support the actual native integer ranges, including signed analog inputs and all 3,600 recording frames; A commits through the original callback, B cancels without a write. The viewport follows the draft choice.
- Canonical references keep dynamic labels, mode-dependent availability, actor banks, probability rebalancing, and original save/export identities intact. Global palette/suppression/cue callbacks now identify the selected canonical option rather than assuming the old long-list row position.
- Hold-Y shortcuts find their destination within the new tree and establish its real parent/active tab. Existing L/R OSD paging, frame-step bindings, bespoke Custom TDI and memory-card export/import flows retain their existing input paths.
- Other events retain the legacy renderer. Menu layout headers now participate in the shared ABI build fingerprint, so a partial build cannot pair old shared code with new menu structures. No settings/save-format change or appended shared function export was needed.

Native descriptions use bounded word wrapping into up to eight lines (the existing options still store four source strings). Preview position follows the wrapped description. The preview bank has 24 lines, enough for all current pages/rows, including the 17-condition overlay editor and its page headings. Future menu additions must recheck that bound and native fit.

Validation completed:

- Optimized PowerPC compilation of Lab and the shared menu renderer completed without new compiler diagnostics.
- `python tests/test_menu.py`: **5 tests passed**. Tests execute the actual PowerPC controller rules and menu views, check canonical identity/coverage, all 19 unique OSD destinations, nine-row/three-entry bounds, signed/large picker limits, and emitted optimized Lab DAT pointers. The DAT test applies native MEX relocation and executes the initial view/page setup before the first fighter dependency; names, purposes and description pointers remain inside the loaded payload.
- Existing `tests/run_settings_tests.sh`: **135 tests passed**, including optimized shared-DAT relocation, settings/recording identity, pending saves, overlays, layouts, cues and trails. The later refinement only increased wrapped-description capacity and supplied page help; the final menu suite and full release rebuild were repeated for that revision.
- Full optimized release build succeeded: `TM-Tyro-V1.4.1T3.iso` and `TM-Tyro-V1.4.1T3.zip`, using the stable TYRE01 identity.

**Still to validate in Dolphin:** native font/overscan fit and actual controller feel; retained parent selection across every editor/action; unsupported stage/fighter reasons; recording save/delete/mirroring/takeover/copy/chance transitions; custom TDI and card export return/unpause; and other-event menu regression checks. Compiled/relocated tests do not prove visual fit or gameplay input handling. The browser sketch remains a design aid, not a native screenshot.

## 13. Fonts, recording help and the first screenshot correction — October 8, 2026

The user's Dolphin screenshots establish that the first recording preview has vertically overlapping lines, and that the native menu leaves substantial horizontal space unused within its outer frame. This supersedes the initial assumption that 17-unit preview spacing would fit the stock glyphs. The font family has not been changed in this pass.

### 13.1 Available font/text options

There is no project font-family catalog comparable to CSS `font-family`, and no bundled `.ttf`, `.otf` or `.woff` assets. The practical options exposed by this project are:

| Option | Availability and consequences |
| --- | --- |
| **Stock Melee menu text via HSD/SIS** | Already used by Lab, most HUD messages and the watermark. `Text_CreateText`, `Text_SetScale`, kerning and aspect fitting support larger/smaller text and width adjustments. This is the current menu face and the smallest compatible implementation choice. The project does not provide a verified commercial typeface name for it. |
| **Native developer/debug fixed-width text** | Already used by the TM console through `DevelopText_*` and its row/column data table. Inspection of the native draw routine confirms a line/stroke font in fixed cells; lowercase is mapped to uppercase. It is not another `Text_CreateText` font-family argument. It would require replacing the rendering path for menu labels and descriptions. |
| **Scene-specific SIS bitmap glyph/message archives** | `Text_LoadSdFile` / native `HSD_SisLib_803A62A0` can load an archive table containing glyph images, widths and encoded strings. Existing disc assets are listed below. Their bank IDs and localized message sets are not a verified set of interchangeable Latin UI typefaces. Changing the ID alone is not a reliable font switch. |
| **A new custom bitmap face** | Technically possible through a new glyph atlas, width/kerning data, encoding and managed SIS-bank lifetime, or a dedicated textured-glyph renderer. No such alternative face is bundled and ready to select in this checkout. TrueType/OpenType fonts would have to be converted into compatible assets, not loaded directly by the current menu API. |
| **Pre-rendered letters/artwork in DAT models/textures** | Possible for fixed decorative text, but not a drop-in font for arbitrary state names, values and descriptions. It needs dedicated glyph/text layout work. |

Local sources: [MexTK/include/text.h](MexTK/include/text.h), [src/menu_controller_ui.h](src/menu_controller_ui.h), [src/lab_css.c](src/lab_css.c), [src/events.c](src/events.c), and the native ASCII lookup/width/draw tests in [tests/test_settings.py](tests/test_settings.py). The [native SIS structure](https://github.com/doldecomp/melee/blob/master/src/sysdolphin/baselib/sislib.h) describes per-bank images and widths; the [native SIS initialization](https://github.com/doldecomp/melee/blob/master/src/sysdolphin/baselib/sislib.c) allocates five vanilla archive slots. The current C code uses identifier 0 for CSS import, 2 for match/menu text and 10 for the watermark; that usage must not be presented as three named faces or ten freely interchangeable font banks. Built-in menu glyph encodings and archive-specific glyph encodings are different paths.

The local vanilla ISO contains these SIS/message archives, which are **asset choices to investigate**, not a catalog of distinct typefaces: `SdClr.dat/.usd`, `SdDec.dat/.usd`, `SdIntro.dat`, `SdMenu.dat/.usd`, `SdMsgBox.dat/.usd`, `SdPrize.dat/.usd`, `SdProge.dat/.usd`, `SdRst.dat/.usd`, `SdSlChr.dat/.usd`, `SdStRoll.dat`, `SdTou.dat/.usd`, `SdToy.dat/.usd`, `SdToyExp.dat/.usd`, `SdTrain.dat/.usd`, and `SdVsCam.dat/.usd`. `SdMenu.dat` and `SdMenu.usd` were extracted into ignored investigation scratch and both expose `SIS_MenuData`; the localized archive sizes are 317,509 and 87,557 bytes. Different strings/character coverage do not prove a different Latin font.

**Conclusion for the current pass:** keep the compatible stock face, provide enough width to avoid excessive aspect compression, and correct baseline spacing. A font-family replacement should be a separate asset prototype with measured glyph coverage and bank ownership.

### 13.2 Recording behavior research and improved descriptions

The full explanation is in [TRAINING-LAB-RECORDING-GUIDE.md](TRAINING-LAB-RECORDING-GUIDE.md). Native descriptions now cover all 18 recording rows, the six Slot Management rows, and chance/extra-percent controls. They distinguish:

- The saved situation from the separate human/CPU input clips, and the separate D-pad quicksave.
- CPU Control from CPU Record, including which fighter your controller drives.
- Playback Takeover, which leaves normal clips intact, from Re-Record, which commits replacement inputs when leaving that mode/changing slots.
- **Loop**, which repeats the input timeline without restoring positions/damage, from **Auto Restore**, which reloads the saved situation after a 20-update delay.
- **Re-Save**, which keeps full clips, from **Prune**, which removes their played beginning; both apply across all actor slots.
- Weighted Random slot selection from **Random Percent**, which adds 0..N damage on restores and is capped at 999%.
- Deleting one clip from deleting the complete recording setup, and exporting a card file from importing on character select.

The root menu supplies a Lab-owned unavailable-reason callback, so a disabled recording row explains Save Positions, mirror mode locks, recording-mode conflicts or empty probability slots. It adds no shared function export and does not alter save/export value IDs.

Research found an existing mirroring gate: `Record_ChangeMode_Common` tests CPU Playback twice in its `can_mirror` expression. Human-only playback therefore does not currently enable mirroring. This is documented accurately in help; that behavioral gate was not changed while rewriting descriptions. CPU Counter's end/hit settings were also described as a handoff to Lab AI rather than promising an immediate attack in every mode.

### 13.3 Width, font size and line spacing changes

Both panels widen from **21.6 to 25.4 world units** (about 18% more width), consuming most of the unused margin inside the existing outer frame. Row backgrounds and all eight tabs widen with them. Native name fitting width grows from 250 to 330 text units; the right description/preview fitting width grows from 390 to 476. The right title now has its own bounded text object, preventing its fitting width from inheriting the entire menu's width.

Text scales increase: names `.78 → .86`, values/tabs `.72 → .80`, descriptions `.78 → .84`, previews `.65 → .84` (about 29% larger), and panel headings also increase. Description spacing becomes **30**, preview spacing **32** rather than 22/17. A 32-unit glyph at `.84` is at most 26.88 units tall, leaving a positive gap between rows. The preview begins after the actual wrapped description, removing the earlier forced four-line blank allowance.

Preview capacity is bounded by the right panel's lower edge. Longer previews retain every entry across pages; with an unopened submenu focused, **Left/Right changes the preview page**, A enters the submenu as before, and B keeps its existing back behavior. Tab selectors and editor page selectors retain their original Left/Right behavior. Disabled included entries are dim rather than appending a long `(unavailable)` suffix to every row.

The new portable layout rules are in [src/menu_controller_layout.h](src/menu_controller_layout.h), which also participates in the build fingerprint. Native text/model code remains opt-in for Lab; other events keep the old layout. The tests check the larger glyph spacing and bounded final preview row for 0–8 wrapped description lines, plus native DAT relocation and the Lab recording-help callback. A new screenshot is still needed to verify the actual visual result and overscan fit in Dolphin.

### 13.4 Actual-glyph font comparison

The requested comparison is in `docs/training-lab-font-demo.html`. It embeds extracted glyph shapes rather than substituting Arial or a browser monospace font. Stock 32×32 I4 tiles come from native address `0x8040CD40`, with bearings at `0x8040CB00`; the existing native converter determines each ASCII glyph index. The debug renderer at `0x80391664` uses 13-byte stroke records at `0x80408630`; its code confirms digit/letter/symbol mappings, uppercase folding, and separately drawn dot/colon points. This refines the earlier generic description of debug text as pixel/grid text: it is a fixed-cell stroke renderer.

The comparison supports editable ASCII samples, a cell-height slider, and simulated horizontal aspect compression for the stock face. It shows native glyph shapes with approximate browser spacing/filtering, not an exact Dolphin screenshot. A custom-font sample is not shown because no additional custom typeface atlas is bundled. The font demonstration does not change the game build.

### 13.5 Widen the complete frame — October 8, 2026

The subsequent screenshot confirms that reducing interior margins helped, but the user requested a wider menu on the screen as well. The entire Lab model now expands horizontally by **8%**, controlled by `MC_HORIZONTAL_EXPANSION`. This scales the existing outer frame along with its panels, rows, highlights and tabs. Text X anchors and fitting bounds expand by the same factor; glyph scales, viewport scales, Y positions and menu height stay unchanged. The runtime preview positions use the same factor as initial text creation. This provides more screen coverage and usable text width without horizontally stretching letters. Other events and bespoke recording/card editors are unchanged. Final screen-edge/overscan fit remains a Dolphin trial check.
