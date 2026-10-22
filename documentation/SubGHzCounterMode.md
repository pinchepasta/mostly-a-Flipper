# Sub-GHz Rolling-Code Counter Mode (desync bypass)

Lets a clone live next to the original remote: the counter step is altered so the
receiver keeps accepting both, instead of clone and remote knocking each other
out of sync.

Reach it on the device: **Saved -> pick a signal -> Signal Settings**.
`Counter Mode` is per signal (written into the `.sub` as `CounterMode: X`);
`Counter Mult` is global and persists across reboots.

With no `CounterMode` line, mode `0` applies: behave exactly like a normal
remote, counter += `Counter Mult` (default `+1`).

## Modes

| Protocol | Modes |
|---|---|
| Nice FloR-S | 0 default, 1 `0001/FFFE`, 2 `0000/0001` |
| CAME Atomo | 0 default, 1 `0000/0001/FFFE/FFFF`, 2 `807B/807C/007B/007C`, 3 freeze |
| Alutech AT-4N | 0 default, 1 `0000/0001/FFFE/FFFF`, 2 `0000..0005` |
| KeeLoq | 0 default, 1 `0000/0001/FFFE/FFFF`, 2 `+0x3333`, 3 `8006/8007/0006/0007`, 4 `807B/807C/007B/007C`, 5 `0000/FFFF`, 6 freeze, 7 `+0x3333` burst |
| V2 Phoenix | 0 default, 1 `0000/0001/FFFE/FFFF`, 2 `0000..0004` |

An out-of-range mode falls back to that protocol's last mode. `Counter Mult` at
`0` freezes the counter for every dynamic protocol.

## Manual editing

~~~
Filetype: Flipper SubGhz Key File
Version: 1
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok650Async
Protocol: KeeLoq
Bit: 64
Key: DE AD BE EF CA FE BA BE
Manufacture: Doorhan
CounterMode: 2
~~~

## Warning

Any mode other than the default can desync a real remote. If you cannot put the
original remote back into the receiver, do not experiment on a gate you do not
control.
