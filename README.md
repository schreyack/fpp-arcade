# fpp-arcade (FPP Arcade Multi-Player)

Interactive games playable on Pixel Overlay Models for [Falcon Player (FPP)](https://github.com/FalconChristmas/fpp).

The plugin renders each game onto an FPP Pixel Overlay Model (typically an LED matrix), so a
prop becomes a playable screen. Input comes from FPP Commands, which means anything that can
trigger a command — physical buttons via GPIO, a USB gamepad, MQTT, a web request — can drive
the games.

## Games

| Game | Input used |
|------|-----------|
| **Tetris** | The four directional "Pressed" events. "Up" rotates the block. The "Pieces fall" option turns the board on its side (right to left or left to right); the D-pad turns with it: the falling direction drops, the opposite one rotates, the other two slide the piece. The theme speeds up with the drop. |
| **Pong** | Both "Pressed" and "Released" for all four directions. Player one uses Up/Down, player two uses Left/Right. The **Multi-Player** control mode instead gives each USB controller its own paddle: both players use Up/Down on their own gamepad. Until a second player moves the right paddle, the computer plays it. The ball speeds up with every return and each serve goes at an angle toward whoever lost the point. |
| **Snake** | The four directional "Pressed" events. A press straight back is ignored. The snake speeds up a little with every bite. |
| **Breakout** | Directional "Pressed"/"Released" events. Three balls a game; the ball speeds up after the 4th and 12th hits and on reaching the top rows. |
| **Pacman** | The four directional "Pressed" events; a turn pressed early waits for the corridor. A maze sized for a 60x30 matrix (scaled up and centered on bigger models), three lives, and power pellets that turn the ghosts blue so Pac-Man can eat them. A second controller can drive the cyan ghost; until then it chases on its own. |

Every game has retro sounds made in code, played on FPP's audio output next
to the show. The settings page turns them on or off and sets how loud they
are; it never changes the system volume.

## Menu

Any controller press while nothing is running opens a menu on the game model:

1. **PLAY or DANCE** - Up/Down to move, Start or Fire to pick, Select to go back.
2. **PLAY** lists the model's games; **DANCE** lists every playlist named `Dance - <song>`.

A picked game starts at once and only the D-pad reaches it until it ends;
a game nobody touches for 30 seconds is ended. A picked song is inserted with
"Insert Playlist Immediate", so a running show pauses, the song plays, and the
show resumes where it was. Every press is ignored until the song is over. The
menu closes itself after 20 seconds without input.

**Cancel:** hold the left shoulder button and press Start to close the menu,
end the game, or stop the dance song (a show it interrupted resumes). Map the
shoulder button to "Left Shoulder" on the Joysticks page.

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
