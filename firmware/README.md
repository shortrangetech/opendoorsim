# OpenDoorSim Firmware Guide

Welcome to the firmware folder for OpenDoorSim! The main firmware files live here.

*For full flashing instructions, please refer to the [Official Documentation Flash Guide](https://docs.shortrange.tech/opendoorsim/user-manuals/flashing-firmware).*

## Flashing Your OpenDoorSim's Firmware

OpenDoorSim's firmware can be flashed to your ESP32 device using the shell script ```./flash_board.sh``` in the parent folder, or the PlatformIO extension in your favorite IDE.






## Reader Interfaces

OpenDoorSim reads credentials over **Wiegand** (default) or **OSDP**. Only one
is brought up per boot — the D0/D1 terminals carry either the Wiegand data
lines or the RS-485 pair — so changing the interface prompts for a reboot, from
either the web UI (Settings → Reader) or the on-device menu (GENERAL → Reader).

### OSDP

OpenDoorSim acts as the ACU (controller) and the reader is the PD: it polls the
reader and card reads arrive as `osdp_RAW` replies, which feed the same
pipeline as Wiegand bits — the Wiegand formats, parity checking, user matching,
display and log all behave identically.

| Setting | Default | Notes |
| --- | --- | --- |
| PD Address | 0 | 0–126; must match the reader's configured address |
| Baud Rate | 9600 | 9600 / 19200 / 38400 / 57600 / 115200 / 230400 |

The Reader tab shows the link state (ONLINE once the reader answers a poll,
OFFLINE after 8 s of silence, per the spec).

Wiring on the v2.2 board goes through the MAX3485 transceiver on UART2:
`TX_OSDP` = GPIO17, `RX_OSDP` = GPIO16, `DE_OSDP` = GPIO4 (DE and #RE tied).

### Secure Channel

Secure Channel is off by default and can be switched on without a reboot:

| Mode | What it does |
| --- | --- |
| Off | Clear text. A reader configured for secure-only operation will refuse it. |
| Install mode | Handshakes with the spec's well-known default key (SCBK-D), which is how an out-of-the-box reader is commissioned. |
| Secure | Handshakes with the per-installation SCBK stored on this device. |

Once a session is up every command is encrypted and MAC'd, the Reader tab shows
the channel as SECURE, and the firmware re-handshakes on its own after a session
loss (backing off when a key keeps failing).

**Installing a key.** With a session up in install mode, press *Generate* for a
random 16-byte key (or type your own 32 hex characters) and then *Install Key
on Reader*. **The reader answers only to that key afterwards.** *Show* reads
the stored key back if you need it again -- but a reader keyed from a device
that has since been wiped needs a factory reset, so keep a copy elsewhere too. The firmware sends `osdp_KEYSET`, and on
the reader's ACK it stores the key and switches itself to secure mode. **The
reader answers only to that key afterwards — keep a copy**, because the device
never hands the key back out.

**Where the key lives.** In ESP32 NVS, not `settings.json` — it survives a
filesystem reflash. `/getSettings` reports only whether a key is stored; the
key itself comes from `/osdpScbk`, which is only requested when you press
*Show*. Note that anyone on the device's access point can call that endpoint,
so put a password on the AP if the key matters. AES-128 comes from mbedTLS and RND.A from the hardware
RNG; that RNG is only a true random source while WiFi is running, so commission
keys with the access point on.

This is Secure Channel 1 (AES-128) only. SC2 -- the AES-256-GCM channel that
uses security blocks SCS_21..28 and the 32-byte SCBK -- is not used: the ACU
never initiates it, and `osdp_KEYSET` always carries key type 0x01 (SCBK).

Still not implemented: reader identification (`osdp_ID` / `osdp_CAP`), keypad
entry, LED/buzzer commands to the reader, and multiple PDs on one bus.

The protocol stack is [OSDP-Embedded](https://registry.platformio.org/libraries/z-bit-systems/osdp-embedded),
pulled from the PlatformIO registry as `z-bit-systems/osdp-embedded` and
resolved at build time — nothing to vendor or sync by hand. The version range
lives in `platformio.ini`.
