# Stage control protocol

Web app and display use FluidNC's line-oriented GRBL channel as the common controller interface. USB, WebSocket
and display UART are transports for the same commands and status reports; no client owns a separate position.

## Commands

| Intent | Wire command |
| --- | --- |
| Request status | realtime byte `?` |
| Cancel jogging | realtime byte `0x85` |
| Reset controller | realtime byte `0x18` |
| Relative XY jog | `$J=G91 G21 X{x} Y{y} F{feed}` |
| Set XY work zero | `G10 L20 P0 X0 Y0` |
| Unlock alarm | `$X` |

Jog clients must send short bounded segments and cancel on release, visibility loss or transport loss. Scan jobs
will use ordinary absolute G-code and may only be started when the machine profile reports calibrated axes.

## Status

Clients consume standard reports such as:

```text
<Idle|MPos:0.000,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000>
```

The first field is the controller state. `MPos` is authoritative machine position and `WCO` is the work-coordinate
offset. Clients may display work position but must not manufacture independent coordinates.

## Arbitration

Only one live-jog client may actively hold a pointer or touch gesture. A scan may start only from `Idle`; all jog
controls are disabled while a scan is queued or running. Any client may issue jog cancel. Software reset is a
recovery action and does not replace a power-cutting emergency stop.
