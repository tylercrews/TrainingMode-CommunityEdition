# Training Lab recording controls

**Audited October 8, 2026 against this T3 checkout.** This guide follows the implementation in [src/lab.c](src/lab.c), particularly `Record_Think`, `Record_Update`, `Record_ChangeMode_Common`, `Record_ResaveState`, `Record_PruneState`, and `Event_Think_LabState_Normal`. The [upstream project overview](https://github.com/AlexanderHarrison/TrainingMode-CommunityEdition#changes-from-the-original) also identifies re-recording, takeover, resaving/pruning, weighted slots and automatic restores. Local code determines the exact behavior described here.

## Starting state and clips

One recording setup has **one saved starting situation** shared by both fighters, plus **six human input clips and six CPU input clips**. HMN means the human fighter. A clip stores controller inputs over time; it is not a separate complete savestate. Each clip can contain up to 3,600 simulation frames, approximately 60 seconds at normal game speed.

**Save Positions** captures a supported match snapshot, including fighter positions, damage and action state, and initializes the recording setup. It unlocks the recording controls and becomes **Restore Positions**. It also clears existing clips when creating that fresh setup. **Restore Positions** reloads the snapshot and restarts the timeline while keeping the clips. Restoration rerolls applicable Random slots and Random Percent.

The configurable D-pad quicksave is a separate snapshot. It does not redefine the recording start the same way as Save/Re-Save Positions. The first recording save also initializes the quicksave, which can make them appear equivalent until you save a different quicksave.

## Two ways to make a clip

For a human clip:

1. Place the fighters and choose their starting damage; use Save Positions.
2. Choose a numbered HMN Record Slot and set HMN Mode to Record.
3. Restore Positions if you want capture to begin from the saved situation; unpause and perform your inputs.
4. Set HMN Mode to Playback to restore and replay the clip. Use Playback Takeover = None when you want to watch without accidentally taking control.

For a CPU clip:

1. Save Positions and choose a numbered CPU Record Slot.
2. Set CPU Mode to Record. Your controller now drives the CPU; the human fighter does not receive your controls.
3. Restore/unpause and perform the CPU sequence.
4. Set CPU Mode to Playback. The recorded CPU sequence runs and the human fighter can be used to practice against it.

CPU Mode = Control provides the same direct CPU control without capturing a clip. These instructions assume the ordinary Lab control path, rather than a separate Controlled By override.

## Every main recording option

| Option | Meaning and relevant dependencies |
| --- | --- |
| Save Positions / Restore Positions | Create the starting snapshot and unlock controls, or reload the existing snapshot and keep the clips. This is more than an XY-position bookmark. |
| HMN Mode: Off | Use live human control rather than replaying/capturing the human clip. |
| HMN Mode: Record | Capture your live inputs into the numbered human slot. Restore first if you need the clip aligned to the saved start. |
| HMN Mode: Playback | Replay the selected human clip. Selecting this mode restores the starting state. Live input may cancel playback depending on Playback Takeover. |
| HMN Mode: Re-Record | Replay the clip until you input, then record replacement inputs from that point into a working copy. Selecting it restores the start. |
| HMN Record Slot | Choose the human clip. Changing the slot restores the start. Random chooses from recorded slots using Set HMN Chances; recording modes require a numbered slot and fall back to Slot 1 if Random was selected. |
| CPU Mode: Off | Use the behavior/counter logic in CPU Options. |
| CPU Mode: Control | Your controller drives the CPU without recording; human control is disconnected for this path. |
| CPU Mode: Record | Your controller drives the CPU and its inputs are captured in the numbered CPU slot. |
| CPU Mode: Playback | Replay the CPU clip. Selecting this mode restores the start. CPU Counter may hand the CPU back to Lab AI; CPU takeover may give it to your controller. |
| CPU Mode: Re-Record | Replay the CPU clip until takeover, then record replacement inputs in its working copy. |
| CPU Record Slot | CPU equivalent of HMN Record Slot, with its own six clips and chance weights. |
| Mirrored Playback | Reverse left/right positions, facing and horizontal main/C-stick inputs. On mirrors each restore; Random picks mirrored/unmirrored on restore. Intended for symmetric stages. Turn it Off before changing actor modes. |
| CPU Counter | Decide when CPU playback hands control back to Lab's CPU behavior/counter system; see the value table below. |
| Loop Input Playback | Restart the input timeline after the longest active normal Playback clip ends. **Does not restore fighter position, damage or action state.** Random clip selections reroll. Looping does not run during Record/Re-Record or CPU Control. |
| Auto Restore | Reload the saved situation after an end/counter trigger, following the 20-update delay. Unlike Loop, this resets state and rerolls restore-time randomness. It does not run during Record/Re-Record or CPU Control. |
| Start Paused | At the first recording frame, wait for an action-button, stick, C-stick or trigger input before simulation starts. It is separate from opening the Lab menu. Takeover settings can also react to that input. |
| Playback Takeover | Live input takes over HMN or CPU, or None disables input-triggered takeover. Normal Playback leaves the original clip intact. Human playback also hands back control at its end. Re-Record forces the takeover actor and writes replacement inputs instead. |
| Re-Save Positions | Replace the shared starting situation while keeping complete clips. Start-frame offsets are shifted to preserve their relative timing. Applies to all human/CPU slots. |
| Prune Positions | After moving forward into playback, replace the start with the current situation and discard the earlier part of every clip. Clips shorter than the removed prefix can become empty. Applies to all human/CPU slots. |
| Delete Positions | Clear the saved recording state and all human/CPU clips; reset modes and lock recording controls until a new Save Positions. It does not delete an already exported card file. |
| Slot Management | Choose one actor and slot, then edit, copy or delete that clip while keeping the shared starting situation. |
| Set HMN Chances / Set CPU Chances | Configure each actor's weighted Random slots and restore-time added damage. |
| Export | Save the starting state, both input banks, recording settings, match metadata and screenshot to a memory-card recording file. Choose A/B, enter a name and confirm. Import belongs to the character-select interface. |

### CPU Counter values

| Value | What the implementation does |
| --- | --- |
| Off | Does not automatically cancel the CPU's input playback for a counter/handoff. |
| After Playback Ends | Cancel playback after the CPU clip ends and return to Lab AI. The configured behavior/counter system then controls the CPU. |
| On CPU Hit | Track a hit/grab on the CPU and use its configured counter action, per-hit logic and delay. Hand off when the counter is allowed. |
| On HMN Hit | Cancel CPU playback when the human fighter is a hitlag victim, then use Lab AI. |
| On Any Hit | Cancel CPU playback when either fighter is a hitlag victim, then use Lab AI. |

These choices do not specify the move. Set the actual actions/delays in CPU → Counter Actions. When Playback Takeover targets CPU, the normal CPU Counter handoff branch is bypassed so your controller can take over instead.

### Auto Restore values

| Value | Trigger |
| --- | --- |
| Off | Restore manually. |
| Playback Ends | The longest active normal-playback recording has ended. |
| CPU Counters | The CPU begins countering, enters a death state, or waits at the ledge. |

Once a trigger starts the restore timer, it continues until the 20-update threshold and reloads the snapshot. Loop and Auto Restore are separate rules; use Auto Restore when you need every attempt to start from the same positions/damage.

## Re-Record and takeover

Use Re-Record to keep a useful beginning and replace the remainder with live inputs. The selected clip is copied into an editing buffer. Each replayed input is carried into that buffer; after takeover the live inputs fill it instead. Switching out of Re-Record or changing numbered slots copies the editing buffer back to the clip bank.

**Leave Re-Record before exporting**, because Export copies the regular clip banks, not the active working buffer. Edit one actor at a time: if both actors are in Re-Record, the current code gives human takeover-target selection priority. Re-Record counts as recording for Loop/Auto Restore availability, even while it is replaying the beginning.

## Slot Management

| Row | Meaning |
| --- | --- |
| Player | Human or CPU bank; the banks are separate. |
| Slot | Source clip to edit/delete/copy. |
| Modify Inputs | Pick a Frame, then edit analog axes/trigger or button states. Analog and Buttons pages share the Frame selector. Choosing a frame beyond the clip length extends it. |
| Delete Slot | Empty only this actor's selected clip and remove it from Random playback availability. |
| Copy Slot To | Select a destination in the same actor's bank; selection alone does not copy. |
| Copy Slot | Overwrite the destination with the source's inputs/timing and availability, then redistribute probability weights. Same source/destination is a no-op. |

## Playback Chances

Slots 1–6 are probabilities for the actor's **Record Slot = Random** setting. Empty slots are unavailable. Editing one probability rebalances the other available slots so their total remains 100%. Human and CPU probabilities are independent.

**Random Percent** adds a uniformly chosen integer from 0 through the selected maximum to the saved damage on each restore, capped at 999%. It works independently of selecting a Random clip. Input-only Loop does not apply new Random Percent because it does not reload the saved state.

## Why an option is unavailable

- Before Save Positions: recording modes, slots and tools need the initial snapshot.
- With Mirrored Playback On/Random: human and CPU mode controls are locked until mirroring is Off.
- During Record/Re-Record: Loop and Auto Restore are disabled and reset to Off.
- Empty clip slots: Random probabilities stay unavailable until that slot has inputs.
- Mirroring availability: this checkout's `can_mirror` expression checks CPU Playback twice. It currently requires **CPU Playback**, even if only human playback might conceptually be mirrorable. This was investigated and documented, not silently changed in the description/layout pass.

Some options can be selectable but ineffective in a mode, notably Loop/Auto Restore while CPU Control is active. This guide states the effective behavior rather than assuming availability alone implies the option is running.
