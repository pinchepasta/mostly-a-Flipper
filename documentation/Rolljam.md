# Rolljam (two devices, ESP-NOW coordinated)

Rolljam captures two rolling-code presses and feeds the victim receiver the
*first* one. The receiver is deaf to press #1 because the second device jams, so
it accepts our replay of #1 as its next valid code, while press #2 is stored and
has never been seen by the receiver.

Two Cardputer ADVs, each with a CC1101. They find each other over ESP-NOW
beacons on Wi-Fi channel 1 and coordinate jam / replay timing.

## Setup

On both devices: **Sub-GHz -> Rolljam**

1. `Role:` - one device **SNIFFER**, the other **JAMMER**.
2. `Frequency:` - must match. Set it on the sniffer; the jammer takes the
   frequency from the arm packet.
3. `Link + arm` - starts ESP-NOW and beacons. Wait for `peer XX:XX -NNdBm`.
4. Press **OK** to arm. The sniffer listens and tells the jammer to raise the
   carrier.

## Attack

1. Press the remote once. Nothing happens at the gate - it is jammed. The
   sniffer captures code #1.
2. Press the remote again. The sniffer captures code #2, immediately mutes the
   jammer, replays code #1, then re-mutes.
3. The gate opens. Code #2 is kept and auto-saved to
   `/ext/subghz/rolljam_<freq>_<tick>.sub`.

**Replay** sends the kept code. **Again** restarts the hunt for another round.

## Placement matters

The sniffer shares the band with the jammer's carrier, so put the **sniffer near
the remote** and the **jammer near the victim receiver**. If the sniffer is
closer to the jammer than to the remote, its own RX is desensed and it decodes
nothing - the UI sits at `codes:0`.

## Protocol

Broadcast ESP-NOW frames, `RolljamPacket` in `helpers/rolljam_espnow.h`:

| Type | Direction | Meaning |
|---|---|---|
| `Beacon` | both, 4 Hz | role, frequency, state - the discovery mechanism |
| `Arm` | initiator | start the attack |
| `JamOn` / `JamOff` | sniffer -> jammer | carrier up / down |
| `Code` | sniffer -> jammer | code index and protocol, for display |
| `Bye` | on exit | peer forgets immediately |

A peer that stops beaconing for 3 s is dropped and the carrier is released, so a
walk-off does not leave the victim deaf.

## Notes

- Both devices on the same role: the UI shows `clash: same role`.
- ESP-NOW is 2.4 GHz, the attack is sub-GHz, so they do not interfere.
- The jammer transmits a constant carrier (250 us re-arm), not noise: the
  victim's data slicer never sees an edge, so the remote's press is invisible.
