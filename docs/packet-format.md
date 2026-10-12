# Wake packet format

The wake beacon is a 31-byte **legacy BLE advertisement** (the maximum legacy
payload) made of two AD structures: *Flags* and *Manufacturer Specific Data*.

```text
02 01 06  1B FF  53 05  01 00 03  7E 05  66 20  00 01 81  02 EE DD CC BB AA  0F  00 00 00 00 00 00 00
└ flags ┘ └ hdr┘ └ CID┘ └ fixed ┘ └ VID┘ └ PID┘ └ state ┘ └ console address ┘ └┘  └──── padding ───┘
```

| Bytes | Value | Meaning |
|---|---|---|
| 0–2 | `02 01 06` | Flags: LE General Discoverable, BR/EDR not supported |
| 3–4 | `1B FF` | Manufacturer Specific Data, 27 bytes follow |
| 5–6 | `53 05` | Company ID `0x0553` (Nintendo), little endian |
| 7–9 | `01 00 03` | Constant |
| 10–11 | `7E 05` | USB vendor ID `0x057E` (Nintendo), little endian |
| 12–13 | `66 20` | Controller product ID, little endian (`0x2066` here) |
| 14–16 | `00 01 81` | Last byte is the state: `0x81` reconnect / wake, `0x00` pairing |
| 17–22 | `02 EE DD CC BB AA` | Console Bluetooth address, **least-significant byte first** (`AA:BB:CC:DD:EE:02`) |
| 23 | `0F` | Constant |
| 24–30 | `00 …` | Zero padding |

## What the console checks

- The **sender address** must belong to a controller bonded with the console.
  This is the only authentication: there is no encryption, no nonce and no
  pairing handshake. Consecutive presses produce identical packets.
- The **console address** in the payload must be its own.
- The **state byte** must be `0x81`.

## Product IDs

| Controller | Product ID | `product_id` |
|---|---|---|
| Joy-Con 2 (L) | `0x2066` | `joycon2_left` |
| Joy-Con 2 (R) | `0x2067` | `joycon2_right` |
| Pro Controller 2 | `0x2069` | `pro_controller2` |

Values as reported by community research; always prefer the one in your own
capture.

## Transmission

A real controller sends the beacon in a short burst. The component reproduces it
as **connectable undirected advertising (`ADV_IND`) from a public address**, on
all three primary channels (37, 38, 39), every 20 ms for 2 seconds by default.
Captured controller beacons are non-connectable (`ADV_NONCONN_IND`); community
research reports that both types wake the console, and `ADV_IND` with a 20 ms
interval is the configuration tested with this component.

## Sources

- [alexvnesta/switch2controller](https://github.com/alexvnesta/switch2controller) — `docs/research/wake-packet.md`
- [tv/switch2-wake-up](https://github.com/tv/switch2-wake-up)
- [ndeadly/switch2_controller_research](https://github.com/ndeadly/switch2_controller_research)
