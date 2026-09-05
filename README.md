# SkyWatcher X2 Driver

An X2 plugin for controlling a Sky-Watcher EQ mount from TheSkyX via an EQDIR
cable (or Synscan Wi-Fi dongle).

## Key changes in 3.4

- **More reliable, faster serial communication with the mount.** Fixed a
  reentrant-locking deadlock risk in the mutex coverage around every call into
  the mount communication layer. Replaced the unreliable per-byte read timeout
  with a poll-and-batch-read approach (mirroring the same fix already proven
  out on this project's sibling AstroTrac driver) - measured on real hardware,
  this cut average command round-trip time from ~27ms to ~19ms and removed
  all communication errors in testing.
- **Centralized, leveled debug logging.** See **Debug logging** below if you need
  to turn it on to diagnose a problem.

## Connecting to the mount

TheSkyX's Serial Device Settings dialog is used for the EQDIR cable
connection:

![Serial Device Settings dialog](images/Serial%20Settings.png)

- **Serial device**: set this to whichever USB serial device your EQDIR cable
  actually enumerates as - **not** the example port shown above (that's
  whatever happened to be selected when this screenshot was taken, not a
  Sky-Watcher device). The baud rate is auto-detected on connect (9600 or
  115200).

Click **More Settings...** in that same dialog to reach the driver's own
settings (below), including the option to connect over the mount's Synscan
Wi-Fi dongle instead of the EQDIR cable.

## Driver settings

Reached via **More Settings...** in the Serial Device Settings dialog above:

![Setup Skywatcher dialog](images/More%20Settings.png)

- **Polar Alignment**: choose which of the four clock positions (12/3/6/9
  o'clock) the polar-scope reticle's home position corresponds to on your
  setup. Slew the mount in DEC so that the polar scope is visible (set Dec to 
  plus or minus 90 deg). Slew the mount in RA so that the Polaris marker moves 
  to the clock position selected above and press **Set Polar Alignment Home**. 
  Once calibrated, pressing **Move to Alignment Position** will slew the scope
  so that Polaris marker is in the correct position to polar align.
- **Polar Illuminator Brightness**: controls brightness of the polar-scope 
  reticle LED if the mount supports this.
- **Slew Limits**:
  - **East Limit** (hours): how far before the meridian the mount move
    can slew after a meridian flip - will generally be zero or negative.
    Negative will result in a slightly weights up position
  - **West Limit** (hours): how far past the meridian the
    mount is allowed to track before the driver stops tracking. A positive
    West Limit allows tracking slightly into the weights-up position. 
    The West Limit must be ≥ East Limit.
  - **Flip Hour Angle**: the hour angle at which the mount performs a
    meridian flip when slewing. Usually set equal to the East Limit; must lie
    between the East and West Limits. Note that although all three limits are 
    sent to TheSky correctly, only the West Limit is implemented by TheSky.
  - **Horizon Limit** (degrees): tracking stops once the target sets below
    this altitude.
  - **Post Slew Delay** (seconds): a pause added after every slew completes.
    Allows a delay before e.g. taking a Closed Loop Slew image if
    the stars are streaked to allow the scope to settle.
- **Guide Rate**: the guide rate (as a fraction of sidereal) used for
  autoguiding pulses. 0.25x sidereal is recommended - there's a floor to how
  quickly the mount can respond, so a longer pulse at a lower rate gives more
  accurate results than a short pulse at a high one.
- **PPEC Training**: enable/disable PPEC (if the mount has trained data
  stored, it's enabled by default) and start a new training run. Training can
  take up to 20 minutes depending on the mount - make sure autoguiding is
  running before you start it.
- **Synscan Wi-Fi**: IP address and port of the mount's Synscan Wi-Fi dongle
  (default `192.168.4.1:11880`), and a checkbox to connect over Wi-Fi instead
  of the EQDIR cable.

## Debug logging

For normal use, leave debug logging off - this is the default, shipped with
both macros commented out, so `LogDebug` compiles away to nothing and no log
files are written. To diagnose a problem, uncomment one line in each of two
headers and rebuild:

- `Skywatcher.h`: `#define SKYW_DEBUG <level>` - logs to `~/Skylog.txt`.
  Covers the low-level mount protocol: command send/receive timing, retries,
  timeouts, and open-loop-move (guiding/jog) durations.
- `x2mount.h`: `#define HEQ5_DEBUG <level>` - logs to `~/X2Mountlog.txt`.
  Covers the X2/TheSkyX-facing interface layer: link state, UI events,
  coordinate/tracking-limit checks.

Both use the same 0-3 level scale (lower number = shown at a lower verbosity
setting; each level also includes everything below it):

| Level | Skywatcher.h (`SKYW_DEBUG`) | x2mount.h (`HEQ5_DEBUG`) |
|---|---|---|
| 0 | Open-loop-move tracing (relevant to guiding) | *(unused - lives in Skywatcher.h)* |
| 1 | Notable/unexpected events - command failures, timeouts, retries | Open-loop-move tracing, plus notable/unexpected events |
| 2 | The send-command wire trace (every command sent/received) | *(unused - lives in Skywatcher.h)* |
| 3 | Everything else - connection lifecycle, coordinate/math tracing | Everything else - connection lifecycle, coordinate/math tracing, UI events |

A typical starting point for diagnosing a guiding or comms issue is level 1
in `Skywatcher.h` - it reports each command's round-trip time, try count,
wire text and response, plus any timeouts or failures, without the volume of
levels 2-3.

## Installation

- **macOS**: see `INSTALL.TXT`.
- **Raspberry Pi**: run `rpi_install.sh` (see `INSTALL_RPI.TXT`) after
  launching TheSkyX at least once, so it can locate TheSkyX's install path.
- **Ubuntu/Linux**: run `linux_install.sh` (see `INSTALL_Ubuntu.TXT`),
  likewise after launching TheSkyX at least once.
- **Windows**: use the installer built from `SkyWatcher.iss` (Inno Setup).
