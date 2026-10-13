# fpp-arcade (FPP Arcade Multi-Player)

Interactive games playable on Pixel Overlay Models for [Falcon Player (FPP)](https://github.com/FalconChristmas/fpp).

The plugin renders each game onto an FPP Pixel Overlay Model (typically an LED matrix), so a
prop becomes a playable screen. Input comes from FPP Commands, which means anything that can
trigger a command — physical buttons via GPIO, a USB gamepad, MQTT, a web request — can drive
the games.

## Games

| Game | Input used |
|------|-----------|
| **Tetris** | The four directional "Pressed" events. "Up" rotates the block. |
| **Pong** | Both "Pressed" and "Released" for all four directions. Player one uses Up/Down, player two uses Left/Right. The **Multi-Player** control mode instead gives each USB controller its own paddle: both players use Up/Down on their own gamepad. |
| **Snake** | The four directional "Pressed" events. |
| **Breakout** | Directional "Pressed"/"Released" events. |
| **Pacman** | The four directional "Pressed" events. A second controller can drive a ghost. |

## Menu

Any controller press while nothing is running opens a menu on the game model:

1. **PLAY or DANCE** - Up/Down to move, Start or Fire to pick, Select to go back.
2. **PLAY** lists the model's games; **DANCE** lists every playlist named `Dance - <song>`.

A picked game starts at once and only the D-pad reaches it until it ends;
a game nobody touches for 30 seconds is ended. A picked song is inserted with
"Insert Playlist Immediate", so a running show pauses, the song plays, and the
show resumes where it was. Every press is ignored until the song is over. The
menu closes itself after 20 seconds without input.

## Installation

Install from **Content Setup → Plugins** in the FPP web UI, then restart FPPD.

## Configuration

Two pages are added to the FPP UI:

- **Content Setup → Arcade Games** — pick the game and the Pixel Overlay Model to play it on.
- **Input/Output Setup → FPP Arcade Joysticks** — map a connected USB joystick or gamepad to
  the arcade button/axis commands. Detected controllers are listed automatically; "Set Defaults"
  fills in a standard mapping for a game pad or a dance mat.

## Commands

The plugin registers three FPP Commands:

- **FPP Arcade Select Game** — start a game on a given overlay model.
- **FPP Arcade Button** — deliver a button press/release (Up, Down, Left, Right, Fire, Select, Start).
- **FPP Arcade Axis** — deliver an analog axis position.

Wire these to GPIO inputs, the joystick page, or any other FPP event source to provide controls.

## License

GPLv2 — see [LICENSE](LICENSE).
