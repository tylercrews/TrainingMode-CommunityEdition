<p align="center"><img src="Logos/Training-Mode-banner.png"  alt=""  width="300"/></p>

# Training Mode - Community Edition

Training Mode - Community Edition is an expanded and updated version of UnclePunch's training modpack for Super Smash Bros. Melee.

To download the ISO, click [here](https://github.com/AlexanderHarrison/TrainingMode-CommunityEdition/releases/latest).

Join [the discord](https://discord.gg/2Khb8CVP7A) to discuss changes, new features, or ask for assistance.

## Development
Please read [DEVELOPMENT.md](DEVELOPMENT.md).

The Tyro version and stable game/save identity live in [version.h](version.h). Update `TM_VERSION` for a new release; keep `TM_GAME_ID` stable. Run `./build.sh --version` to see the version, game ID, and output filename.

## Tyro branch changelog

This section records Tyro-specific changes. Add a bullet under the matching Tyro version whenever a change is implemented; keep the newest version first. Proposed gameplay changes stay in the [investigation and implementation plan](OSD-GLOBAL-SETTINGS-INVESTIGATION.md) until implemented.

### T2 (V1.4.1T2, unreleased)

- Fixed repeated invalid reads after creating a save by preventing an unsupported MEX relocation in the game-ID comparison; added optimized DAT relocation regressions.
- Added a global translucent player-colored invincibility/intangibility overlay for ledge, respawn, dodges and moves, including Yoshi's active double-jump armor.
- Allocated byte 40, bit 1 for protection and migrated settings to format 3; 30 reserved bits remain.

- Added global Recovery Yellow/Green: two frames before normal landing/attack recovery and a two-frame completion pulse, with priority over missed L-cancel red.
- Added global Run Turnaround and missed L-cancel red flashes, plus full-health Infinite Shields for all main/subfighters.
- Allocated reserved byte 40, bit 0 to Run Turnaround; migrated packed settings to format 2 while retaining the other 31 reserved bits.

- Added color selection in the L-button OSD editor (B forward / Z backward), with saved color names and previews alongside each row.
- Added TURN OSDS OFF to both OSD editors; it hides global message text/backgrounds while preserving individual colors, trails and event feedback.
- Added saved Off/White/Red/Green/Blue/Yellow/Cyan/Magenta title choices in Lab's OSD editor, with canonical identities across C/native messages.
- Added best-frame-relative timing colors (Cyan/Green/White/Red) while retaining measured frame numbers and technique baselines, including Peach's frame-5 instant double jump.
- Separated overlapping message queue identities, preserved outcome/angle colors, split Wavedash title/timing runs within the existing three lines, and added explicit L-cancel Success/Missed feedback.
- Finalized shared global trail-row labels/bindings, independent edits, and an explicit Both On status in local trail menus; verified all four saved toggle combinations.
- Added global Very Fast and Instant hitbox trails to the L-button OSD menu and Lab OSD menu, with a shared renderer across gameplay matches.
- Added Very Very Fast decay to Lab and Eggs-ercise, softer historical trails, player/team-accent colors, and gray CPU trails.
- Included subfighters and owner-colored projectiles; removed duplicate event renderers and cleared shared history on scene/retry/restore/rewind boundaries.
- Prevented duplicate drawing when both global modes are enabled and repeated translucent samples during stationary hitlag.
- Added shared C/native settings accessors, explicit 44-byte packing, validation, and migration of Tyro's existing settings.
- Saved every Lab overlay condition instead of limiting each actor to eight enabled conditions; reserved space for global flags and OSD title colors.
- Preserved unrepresented OSD enable bits and existing colors when using the current Boolean menus; added in-memory defaults for unsupported/foreign settings without rewriting their record.
- Corrected Ledgedash's frame-advance button lookup to read its own nibble independently of the decrement button.
- Centralized version and identity metadata in root-level `version.h` and incremented the version to V1.4.1T2.
- Separated Tyro's disc/save identity (`TYRE01`) from upstream (`GTME01`), with a Tyro save caption and banner selection.
- Added versioned ISO and release ZIP names, including `TM-Tyro-V1.4.1T2.iso`, and matching Windows/Linux/macOS release patchers.
- Added generated banner titles and a Tyro-named Dolphin symbol map while preserving the existing banner artwork and credits.
- Made partial builds rebuild all modules when version/identity metadata changes, and stop before updating the ISO if any C module fails to compile.
- Added the investigation and implementation plan for global OSD settings, hitbox trails, Ledgedash improvements, save capacity, and a separate Tyro save identity.
- Expanded the plan with versioned ISO filenames, selectable Ledgedash success criteria, Falling-start reset investigation, hitlag-aware OSD timing, and recovery-state labels for Act OoWait.
- Initialized this Tyro branch changelog.

### T1 (v1.4.1 T1)

- Added hitbox trails to Eggs-ercise.
- Added Very Fast and Instant hitbox-trail decay options to Training Lab and Eggs-ercise.
- Added Tyro Edition names and version labels.
- Added Tyro banners and corrected the in-game banner colors.
- Removed the restriction on AI-assisted contributions from the development guidelines.

## Changes From the Original
- New Training Lab Features:
    - Recording:
        - Reworked recording UI. Allows re-saveing existing recordings with different percents or positioning.
        - Savestates now require holding DPad right, preventing accidental savestates.
        - Set chances for slots during random playback.
        - Option to auto-restore state when the CPU performs a counter action.
        - Takeover HMN or CPU playback at any point.
        - Press DPad left/right when browsing savestates to quickly change pages.
        - Savestates can now always be saved. (May cause crashes if you save during special moves).
        - Option for the CPU to counter on hit or recording end during playback.
        - Play a recording slot as a counter action.
        - Resave Positions option will start the recording from a new position, without removing inputs.
        - Prune Positions option will start the recording from a new position, truncating inputs.
        - Re-Record mode will allow your to record over a previously recorded slot using playback takeover.
        - Start Paused option will only start the replay on your first input.
        - Add random additional percent when loading a state.
        - Slot Management menu - delete, copy, and edit slots.
    - CPU Options:
        - Random custom DI option. The CPU will pick a random option from your custom TDI.
        - CPU Shield angling options.
        - Added Wavedash counter actions.
        - Tech animations can be set invisible after they are distinguishable.
        - Added usmash OoS and all specials moves as counter actions.
        - All special moves can be used as counter actions.
        - Neutral jump action, set as the default.
        - SDI and ASDI options.
        - SDI is now set by number of inputs rather than by chance.
        - SDI and mashing are set to none by default.
        - Move CPU option.
        - Choose a port to control the CPU option.
        - CPU will use random TDI after custom TDI ends.
        - Option to reverse Custom TDI if on the other side of the player.
        - Tech lockout option. This prevents the CPU from teching in quick succession.
        - Tech trap option. This prevents the CPU from teching for a short window after being hit.
        - Added dash through and dash back counter actions.
        - Option to force shielded projectiles to always be powershielded.
    - Other Changes:
        - Alter set OSDs with the OSD menu in the lab.
        - Alter character RNG - choose misfire, nana throws, peach pulls and fsmash, and GnW hammer.
        - Added advanced counter actions - manually choose counter actions for each specific hit.
        - Added "Freeze CPU" option to freeze a CPU's hitboxes in place.
        - Set a chance to wait in miss tech.
        - Added shield health option.
        - Added new shortcut system, currently only supporting frame advance (Press Y then A in the menu).
        - Hitboxes are colored by ID and sorted by priority.
        - Game speed option.
        - Color overlays.
        - Lock percents.
        - Hazard toggle.
        - Hide stage model.
        - Item grab range display.
        - Show input and info displays for both HMN and CPU.
        - Mirror recordings.
        - The overlay, taunt, and input display options are saved to the memcard.
        - R can be used as a frame advance button.
        - Stage options to control stadium transformations and FOD platform heights.
        - Custom action state OSDs.
    - Import Menu Changes:
        - Fixed glitches when importing using a port other than port 1.
        - Fixed Sheik/Zelda transformations when loading.
        - Recordings are now filtered by the selected HMN character.
        - Cursor can now wrap.
        - Deleting replays too fast will no longer crash.
- Ledgedash Event Changes:
    - Colours have been updated to be colourblind friendly.
    - The airdodge angle is now consistent with other events.
    - Invincible grab hitboxes show like other hitboxes.
    - Added reset delay option.
    - Camera is now consistent across reset options.
    - HUD now updates and displays every frame.
    - Game speed option.
    - Swap sides on auto reset option.
    - Option to always maintain full ledge invincibility while on the ledge.
    - The GALINT frame is now correct if ledgedashing without refreshing.
    - Will no longer immediately reset on aerial.
    - Option to show current state as an overlay.
    - Sopo is used instead of both climbers.
- New Edgeguard Event:
    - Replaces Armada Shine event.
    - Learn the basics of edgeguarding Fox, Falco, Marth, Sheik, and Falcon!
    - Adjust the options the opponent uses to change the difficulty or practice specific situations.
    - Choose between preset values or manually adjust hit angle, knockback, and damage.
- OSD Changes:
    - Act OoWait OSD will trigger even with intermediate buffered actions such as a frame of walk.
    - Removed Max OSDs and Recommended OSDs options, replacing with a new OSD Position option.
    - Wavedash OSD now shows if it was a short hop or full hop.
    - Removed broken OSDs, rewriting the most important ones.
    - Added new glide toss, aerial out of double jump, special out of jump, and double jump out of jump OSDs.
    - Added new lockout timer OSDs.
    - Added new Fighter-Specific Tech OSDs with:
        - Spacies: Act OoShine and Shorten Frame.
        - Peach: Act OoFloat.
        - Yoshi: Egg Toss angle and strength.
- Bugfixes/Small Changes:
    - **Fixed cpu acting too late out of sakurai angle and other non-knockdown hits (such as fox drill).**
    - Updated to UCF 0.84 (Allows practicing with dashback out of crouch).
    - All trigger-based functionality can now be performed with analog-only triggers.
    - Nametags will now show up in C events.
    - Slow down advanced camera with R.
    - DIDraw will now show for sheik and after throws.
    - System inputs in info display will now work for ports other than port 1.
    - The lab now saves a minor savestate on boot.
    - Act OoHitstun now works after being hit by falco laser.
    - The powershield event has been rewritten and given a new laser height option.
    - Adjustable timing in Amsah tech event.
    - Jump actions no longer make the CPU self-destruct.
    - Various OSDs have been fixed.
    - Lightshield now works in recordings.
    - Added successful counters to the LCancel and Wavedash events.
    - Fix CPU DI on back throws and moves that send backwards.
    - Can now use lightshield L with DPad to adjust percents.
    - CPUs now DI DK cargo throw.
    - Samus homing missiles will target the CPU.
    - Nana will not drop shield when Popo's shield is hit.
    - Added the polling drift fix.
    - Deleting replays too fast will no longer crash.
    - Every character can be used in Amsah Tech training.
    - Removed the maximum distance in Reversal training.
    - Added getup attacks and dash attack to Reversal training.
    - Added option to move to the platform in Reversal training.
    - Infinite shields now applies to nana.
- Work in progress:
    - Reaction Tech Chase Event
    - Improving the savestate format
- Developer Features:
    - Simple and easily reproducible builds on Windows and Linux.
    - Simple to add new events - no need to touch ASM.
    - Fast recompilation on Linux using make.
    - Simplified and performant [tool](https://github.com/AlexanderHarrison/gc_fst) to extract and rebuild ISOs.
