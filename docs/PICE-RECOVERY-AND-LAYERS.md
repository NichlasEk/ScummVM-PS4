# PICE interaction, recovery and scene depth (2026-09-20)

The experimental engine now merges actors and individual scenery parts by
ascending Y anchor. Previously every scenery part was painted after every
actor, so the door could incorrectly cover the attacking creature.

The local original executable sorts actor records by signed Y at +5
(comparator 0xac21), scenery records by signed Y at +3 (0xac4e), and merges
them at 0xb3b6–0xb458. Scenery is painted first at equal anchors. The native
renderer follows that ordering, preserving resource/slot order within equal
anchors. Scenery anchors come from the top-level composite parts, as in the
original loader at 0xb5f6. Nested parts retain their own internal draw order.
This is not a special case that always draws the creature in front.

Hovering an active hotspot with a registered action changes the pointer into
a hand and displays the object's name above the inventory. Dialogue, menus,
input locks and death suppress this feedback. Clicking outside the dialogue
choices closes them without sending that same click into the room; right
click and Escape also close the menu.

Opcode 131 now freezes room tasks and opens a native recovery overlay:

- Load latest save (also Enter)
- Load autosave
- Load manual save
- Quit

The original opcode dispatches a global-bank script. This overlay substitutes
for that death flow; it does not implement arbitrary global scripts or opcode
132. The recovery screen itself cannot be saved. Successful checkpoint loading
clears the death state. Missing/corrupt saves leave the recovery menu available.

Successful future saves record A or M in `<target>-pice-latest`. When the
marker is missing (older saves), latest prefers autosave, then manual; it
cannot infer historical order. Explicit buttons allow either slot to be
selected. The marker holds no game data. The Driscoll regression harness
preserves and restores it along with its checkpoint and configuration.

Validation: experimental PS4 package builds; sprite self-tests cover individual
nested-part drawing, invalid part indices and transactional cycle rejection.
The opcode test checks that death stops both the triggering and another active
task, and that absent recovery support is reported. The bounded Driscoll
emulator regression passes startup tests, restoration and both introductory
replies. Full-game playability and physical-PS4 validation remain pending.

The emulator death replay restored manual bank 1140, opened recovery reason 1,
then injected a click on Load latest save via the opt-in PICE_TESTBENCH key
`pice_test_recover_latest`. It restored bank 1147 and resumed Driscoll speech
1220. This checks the event/recovery path, not physical controller usability.
The overlay was visually inspected. The door/creature animation still needs
manual visual comparison; the corrected ordering is based on the original
renderer, not a claimed full animation fidelity test.
