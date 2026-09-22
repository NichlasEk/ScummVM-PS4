# PS4 stick sensitivity

ScummVM Options > Control now always exposes the existing joystick pointer
speed and deadzone settings on PS4, including before a pad is detected.
The speed label is Stick sensitivity (Stick speed in low-resolution layouts).
PICE Options exposes the same kbdmouse_speed setting. Values are 0–7;
3 is the existing default. This changes virtual pointer speed, not physical
mouse motion. The cubic response and R1 precision modifier are preserved.

Built both ScummVM and PICE packages. PICE ordinary menu input verified
the upper limit 7 and persistence across restarts in the isolated
frontend-check profile. The lower-limit interaction was inconclusive and needs
a clean controller check. Physical PS4 verification of the new UI remains.
The PICE build removes its exact generated bootstrap before updating shared
patches, then reinstates it, preventing patch-refresh conflicts.
