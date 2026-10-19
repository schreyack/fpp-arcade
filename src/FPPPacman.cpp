#include <fpp-pch.h>

#include "FPPPacman.h"
#include "FPPArcadeSound.h"
#include <climits>
#include <vector>
#include <set>
#include <random>
#include <algorithm>

#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"
#include "overlays/PixelOverlayEffects.h"

FPPPacman::FPPPacman(Json::Value &config) : FPPArcadeGame(config) {
    std::srand(time(NULL));
}

FPPPacman::~FPPPacman() {
}

using namespace ArcadeSound;

// Sound-alikes of the arcade game's sounds: the start tune, the waka of
// eating, the siren under play, the blue-ghost warble, and the death.
static void startTune() {
    Sound melody;
    melody.wave = Wave::Pulse;
    melody.notes = tune("B4:16 B5:16 F#5:16 D#5:16 B5:32 F#5:16. D#5:8 "
                        "C5:16 C6:16 G5:16 E5:16 C6:32 G5:16. E5:8 "
                        "B4:16 B5:16 F#5:16 D#5:16 B5:32 F#5:16. D#5:8 "
                        "D#5:32 E5:32 F5:16 F5:32 F#5:32 G5:16 G5:32 G#5:32 A5:16 B5:8", 115);
    Sound bass;
    bass.wave = Wave::Triangle;
    bass.notes = tune("B2:8. B3:16 B2:8. B3:16 C3:8. C4:16 C3:8. C4:16 "
                      "B2:8. B3:16 B2:8. B3:16 F#3:8 G#3:8 A#3:8 B3:8", 115);
    play(MUSIC, melody);
    play(BASS, bass);
}
static void startSiren() {
    Sound siren;
    siren.wave = Wave::Triangle;
    siren.notes = { slide(380, 620, 210), slide(620, 380, 210) };
    siren.level = 0.45f;
    siren.loop = true;
    play(MUSIC, siren);
}
static void startFright() {
    Sound fright;
    fright.wave = Wave::Square;
    fright.notes = { slide(250, 600, 90) };
    fright.level = 0.4f;
    fright.loop = true;
    play(MUSIC, fright);
}
static void ghostEatenSound() { play(FX2, Wave::Square, { slide(200, 1400, 250) }, 0.7f); }
static void deathSound() {
    stop(MUSIC);
    stop(BASS);
    std::vector<Note> notes = { rest(300) };
    for (int x = 0; x < 9; x++) {
        float top = 950 - x * 75;
        notes.push_back(slide(top, top - 300, 120));
    }
    notes.push_back(rest(80));
    notes.push_back(slide(120, 700, 90));
    notes.push_back(rest(40));
    notes.push_back(slide(120, 700, 90));
    play(FX1, Wave::Square, notes);
}
static void winSound() {
    stop(MUSIC);
    stop(BASS);
    play(FX1, Wave::Pulse, tune("C5:16 E5:16 G5:16 C6:8 G5:16 C6:4", 130));
}

// The maze, drawn for a 60x30 matrix as the center lines of its corridors
// (x1, y1, x2, y2), left half only - the right half mirrors it. Every
// corridor is 5 pixels wide, just room for Pac-Man, so he and the ghosts
// can only travel along these lines, and the walls fall between them.
static const int MAZE_W = 60, MAZE_H = 30;
static const int MAZE[][4] = {
    { 3, 3, 26, 3 }, { 26, 3, 26, 11 },
    { 3, 3, 3, 11 }, { 11, 3, 11, 26 },
    { 3, 11, 29, 11 },
    { 20, 11, 20, 18 },
    { 3, 18, 29, 18 }, { 3, 18, 3, 26 },
    { 3, 26, 26, 26 }, { 26, 18, 26, 26 },
};
static const int PAC_START_X = 29, PAC_START_Y = 18;
static const int LIVES = 3;
static const int FRIGHT_TICKS = 80;   // about 6 seconds of blue ghosts
static const int DYING_TICKS = 30;    // the pause after Pac-Man is caught
static const int EATEN_TICKS = 40;    // about 3 seconds home, as eyes, after being eaten
static const int DIRS[4][2] = { { -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 } }; // left, up, right, down

class PacmanEffect : public FPPArcadeGameEffect {
public:
    struct Ghost {
        int x, y, dir;
        int homeX, homeY;
        int r, g, b;
        bool scared = false;
        int wait = 0;   // ticks left at home after being eaten
    };

    PacmanEffect(PixelOverlayModel *m) : FPPArcadeGameEffect(m) {
        startTune();
        // The biggest whole scale the maze fits at, centered on the model.
        int w, h;
        m->getSize(w, h);
        scale = std::max(1, std::min(w / MAZE_W, h / MAZE_H));
        offsetX = std::max(0, (w - MAZE_W * scale) / 2);
        offsetY = std::max(0, (h - MAZE_H * scale) / 2);
        buildMaze();
        // Blinky and Pinky are the computer's; the cyan one is a second
        // player's, and chases on its own until somebody steers it.
        ghosts.push_back(Ghost{ 26, 3, -1, 26, 3, 255, 0, 0 });
        ghosts.push_back(Ghost{ 33, 3, -1, 33, 3, 255, 120, 200 });
        ghosts.push_back(Ghost{ 29, 11, -1, 29, 11, 0, 255, 255 });
        resetActors();
    }
    ~PacmanEffect() {
        stop(MUSIC);
        stop(BASS);
    }

    const std::string &name() const override {
        static std::string NAME = "Pacman";
        return NAME;
    }

    void buildMaze() {
        open.assign(MAZE_H, std::vector<bool>(MAZE_W, false));
        track.assign(MAZE_H, std::vector<bool>(MAZE_W, false));
        std::set<std::pair<int, int>> lines;
        for (auto &s : MAZE) {
            for (int mirror = 0; mirror < 2; mirror++) {
                int x1 = mirror ? MAZE_W - 1 - s[0] : s[0];
                int x2 = mirror ? MAZE_W - 1 - s[2] : s[2];
                for (int x = std::min(x1, x2); x <= std::max(x1, x2); x++) {
                    for (int y = std::min(s[1], s[3]); y <= std::max(s[1], s[3]); y++) {
                        lines.insert({ x, y });
                    }
                }
            }
        }
        for (auto &p : lines) {
            track[p.second][p.first] = true;
            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    open[p.second + dy][p.first + dx] = true;
                }
            }
            // A pellet every other pixel along the corridors.
            if ((p.first + p.second) % 2 == 0) {
                pellets.insert(p);
            }
        }
        for (auto c : { std::make_pair(3, 3), std::make_pair(56, 3), std::make_pair(3, 26), std::make_pair(56, 26) }) {
            pellets.erase(c);
            powerPellets.insert(c);
        }
    }

    void resetActors() {
        pacX = PAC_START_X;
        pacY = PAC_START_Y;
        pacDir = wantDir = -1;
        for (auto &g : ghosts) {
            g.x = g.homeX;
            g.y = g.homeY;
            g.dir = -1;
            g.scared = false;
            g.wait = 0;
        }
        frightened = 0;
        ghostHold = 27;
    }

    // Pac-Man and the ghosts travel only along the corridors' center lines,
    // so a turn happens exactly at a junction and never into a corner.
    bool canMoveTo(int x, int y) {
        return x >= 0 && y >= 0 && x < MAZE_W && y < MAZE_H && track[y][x];
    }
    bool canGo(int x, int y, int dir) {
        return dir >= 0 && canMoveTo(x + DIRS[dir][0], y + DIRS[dir][1]);
    }

    void movePacman() {
        // A turn pressed early is held until the corridor opens.
        if (canGo(pacX, pacY, wantDir)) {
            pacDir = wantDir;
        }
        if (!canGo(pacX, pacY, pacDir)) {
            return;
        }
        pacX += DIRS[pacDir][0];
        pacY += DIRS[pacDir][1];
        moving = true;
        eat(pellets, false);
        eat(powerPellets, true);
    }

    void eat(std::set<std::pair<int, int>> &from, bool power) {
        for (auto it = from.begin(); it != from.end();) {
            if (std::abs(it->first - pacX) <= 1 && std::abs(it->second - pacY) <= 1) {
                it = from.erase(it);
                if (power) {
                    frighten();
                } else if (!isPlaying(FX1)) {
                    // "Wa" and "ka" take turns, one per bite.
                    play(FX1, Wave::Triangle, { waka ? slide(520, 260, 90) : slide(260, 520, 90) });
                    waka = !waka;
                }
            } else {
                ++it;
            }
        }
    }

    void frighten() {
        frightened = FRIGHT_TICKS;
        for (auto &g : ghosts) {
            if (g.wait > 0) {
                continue;
            }
            g.scared = true;
            if (g.dir >= 0) {
                g.dir = (g.dir + 2) % 4;
            }
        }
        startFright();
    }

    // Where a computer ghost heads: Blinky for Pac-Man, the others for a
    // spot ahead of him, so they come at him from different sides.
    std::pair<int, int> target(size_t i) {
        int ahead = i == 0 ? 0 : (i == 1 ? 8 : 16);
        int d = pacDir < 0 ? 0 : pacDir;
        return { pacX + DIRS[d][0] * ahead, pacY + DIRS[d][1] * ahead };
    }

    // One step for a computer ghost: never straight back unless it has to;
    // at a junction it usually takes the way toward its target, sometimes
    // a random one, and a scared ghost always wanders.
    void moveComputerGhost(size_t i) {
        Ghost &g = ghosts[i];
        std::vector<int> ways;
        for (int d = 0; d < 4; d++) {
            if (canGo(g.x, g.y, d) && (g.dir < 0 || d != (g.dir + 2) % 4)) {
                ways.push_back(d);
            }
        }
        if (ways.empty() && g.dir >= 0 && canGo(g.x, g.y, (g.dir + 2) % 4)) {
            ways.push_back((g.dir + 2) % 4);
        }
        if (ways.empty()) {
            return;
        }
        int pick = ways[std::rand() % ways.size()];
        if (!g.scared && std::rand() % 4 != 0) {
            auto t = target(i);
            int best = INT_MAX;
            for (int d : ways) {
                int dx = g.x + DIRS[d][0] - t.first, dy = g.y + DIRS[d][1] - t.second;
                if (dx * dx + dy * dy < best) {
                    best = dx * dx + dy * dy;
                    pick = d;
                }
            }
        }
        g.dir = pick;
        g.x += DIRS[pick][0];
        g.y += DIRS[pick][1];
    }

    void movePlayerGhost() {
        Ghost &g = ghosts[2];
        if (canGo(g.x, g.y, playerWant)) {
            g.dir = playerWant;
        }
        int steps = playerBoost > 0 ? 2 : 1;
        for (int s = 0; s < steps && canGo(g.x, g.y, g.dir); s++) {
            g.x += DIRS[g.dir][0];
            g.y += DIRS[g.dir][1];
        }
        if (playerBoost > 0) {
            playerBoost--;
        }
    }

    // Ghosts run at four fifths of Pac-Man's speed, and half while blue.
    void moveGhosts() {
        if (ghostHold > 0) {
            ghostHold--;
            return;
        }
        for (size_t i = 0; i < ghosts.size(); i++) {
            if (ghosts[i].wait > 0) {
                ghosts[i].wait--;
                continue;
            }
            bool step = ghosts[i].scared ? (tick % 2 == 0) : (tick % 5 != 0);
            if (!step) {
                continue;
            }
            if (i == 2 && playerHuman) {
                movePlayerGhost();
            } else {
                moveComputerGhost(i);
            }
        }
    }

    // Returns true when Pac-Man was caught.
    bool checkGhosts() {
        for (auto &g : ghosts) {
            int dx = g.x - pacX, dy = g.y - pacY;
            if (g.wait > 0 || dx * dx + dy * dy > 9) {
                continue;
            }
            if (g.scared) {
                // Eaten: home as a pair of eyes for a while, then dangerous
                // again.
                g.scared = false;
                g.x = g.homeX;
                g.y = g.homeY;
                g.dir = -1;
                g.wait = EATEN_TICKS;
                ghostEatenSound();
                continue;
            }
            return true;
        }
        return false;
    }

    void drawPacman() {
        // The mouth opens and shuts as he moves: a wedge cut from the front.
        bool mouthOpen = moving && (tick / 2) % 2 == 0;
        for (int dx = -2; dx <= 2; dx++) {
            for (int dy = -2; dy <= 2; dy++) {
                if (dx * dx + dy * dy > 4) {
                    continue;
                }
                bool mouth = false;
                if (mouthOpen && pacDir >= 0) {
                    int along = dx * DIRS[pacDir][0] + dy * DIRS[pacDir][1];
                    int across = std::abs(dx * DIRS[pacDir][1] - dy * DIRS[pacDir][0]);
                    mouth = along > 0 && across < along;
                }
                if (!mouth) {
                    outputPixel(pacX + dx, pacY + dy, 255, 255, 0);
                }
            }
        }
    }

    void drawGhost(const Ghost &g) {
        if (g.wait > 0) {
            outputPixel(g.x - 1, g.y - 1, 255, 255, 255);
            outputPixel(g.x, g.y - 1, 255, 255, 255);
            return;
        }
        int r = g.r, gr = g.g, b = g.b;
        if (g.scared) {
            // Blue, flashing white as the fright wears off.
            bool flash = frightened < 25 && (tick / 3) % 2 == 0;
            r = flash ? 255 : 0;
            gr = flash ? 255 : 0;
            b = 255;
        }
        for (int dx = -2; dx <= 1; dx++) {
            for (int dy = -2; dy <= 1; dy++) {
                outputPixel(g.x + dx, g.y + dy, r, gr, b);
            }
        }
        outputPixel(g.x - 1, g.y - 1, 255, 255, 255);
        outputPixel(g.x, g.y - 1, 255, 255, 255);
    }

    void drawMaze() {
        // Walls as outlines: only the wall pixels that touch a corridor.
        for (int y = 0; y < MAZE_H; y++) {
            for (int x = 0; x < MAZE_W; x++) {
                if (open[y][x]) {
                    continue;
                }
                bool edge = false;
                for (int dy = -1; dy <= 1 && !edge; dy++) {
                    for (int dx = -1; dx <= 1 && !edge; dx++) {
                        int nx = x + dx, ny = y + dy;
                        edge = nx >= 0 && ny >= 0 && nx < MAZE_W && ny < MAZE_H && open[ny][nx];
                    }
                }
                if (edge) {
                    outputPixel(x, y, 0, 0, 170);
                }
            }
        }
        for (auto &p : pellets) {
            outputPixel(p.first, p.second, 110, 70, 40);
        }
        bool blink = (tick / 4) % 2 == 0;
        for (auto &p : powerPellets) {
            if (blink) {
                for (auto d : { std::make_pair(0, 0), std::make_pair(-1, 0), std::make_pair(1, 0),
                                std::make_pair(0, -1), std::make_pair(0, 1) }) {
                    outputPixel(p.first + d.first, p.second + d.second, 255, 180, 140);
                }
            }
        }
        // Spare lives, as yellow marks on the bottom wall.
        for (int l = 0; l < lives - 1; l++) {
            outputPixel(2 + l * 3, MAZE_H - 1, 255, 255, 0);
            outputPixel(3 + l * 3, MAZE_H - 1, 255, 255, 0);
        }
    }

    void centered(const std::string &text, int y) {
        int x = (MAZE_W - ((int)text.size() * 4 - 1)) / 2;
        outputString(text, x, y);
    }

    void draw() {
        model->clearOverlayBuffer();
        drawMaze();
        if (!GameOn) {
            // The message on a cleared patch of the maze, nothing over it.
            for (int y = MAZE_H / 2 - 7; y < MAZE_H / 2 + 6; y++) {
                for (int x = MAZE_W / 2 - 10; x < MAZE_W / 2 + 10; x++) {
                    outputPixel(x, y, 0, 0, 0);
                }
            }
            centered(won ? "YOU" : "GAME", MAZE_H / 2 - 6);
            centered(won ? "WIN" : "OVER", MAZE_H / 2);
            model->flushOverlayBuffer();
            return;
        }
        if (dying == 0 || (tick / 3) % 2 == 0) {
            drawPacman();
        }
        if (dying == 0) {
            for (auto &g : ghosts) {
                drawGhost(g);
            }
        }
        model->flushOverlayBuffer();
    }

    // After a catch: blink Pac-Man while the death sound plays, then start
    // the next life, or end the game.
    void caught() {
        if (--dying > 0) {
            return;
        }
        if (--lives > 0) {
            resetActors();
        } else {
            GameOn = false;
            endTimer = 67;
        }
    }

    virtual int32_t update() override {
        tick++;
        if (!GameOn) {
            if (endTimer > 0) {
                endTimer--;
                draw();
                return timer;
            }
            if (WaitingUntilOutput) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
                return 0;
            }
            model->clearOverlayBuffer();
            model->flushOverlayBuffer();
            WaitingUntilOutput = true;
            return -1;
        }
        if (dying > 0) {
            caught();
            draw();
            return timer;
        }
        // The siren takes over once the start tune or the fright is done.
        if (!isPlaying(MUSIC)) {
            startSiren();
        }
        moving = false;
        movePacman();
        bool hit = checkGhosts();
        moveGhosts();
        hit = checkGhosts() || hit;
        if (frightened > 0 && --frightened == 0) {
            for (auto &g : ghosts) {
                g.scared = false;
            }
            stop(MUSIC);
        }
        if (hit) {
            dying = DYING_TICKS;
            deathSound();
        } else if (pellets.empty() && powerPellets.empty()) {
            GameOn = false;
            won = true;
            endTimer = 67;
            winSound();
        }
        draw();
        return timer;
    }

    void button(const std::string &butt) {
        size_t bar = butt.find('|');
        std::string button = butt.substr(0, bar);
        std::string joystickName = bar == std::string::npos ? "" : butt.substr(bar + 1);
        int dir = -1;
        if (button == "Left - Pressed") {
            dir = 0;
        } else if (button == "Up - Pressed") {
            dir = 1;
        } else if (button == "Right - Pressed") {
            dir = 2;
        } else if (button == "Down - Pressed") {
            dir = 3;
        }
        if (dir < 0) {
            return;
        }
        if (joystickName.ends_with("2")) {
            // The second controller takes over the cyan ghost; pressing the
            // way it is already going gives it a short burst of speed.
            playerHuman = true;
            if (dir == playerWant && playerBoost == 0) {
                playerBoost = 5;
            }
            playerWant = dir;
        } else {
            wantDir = dir;
        }
    }

    std::vector<std::vector<bool>> open;    // corridor pixels, for drawing the walls
    std::vector<std::vector<bool>> track;   // corridor center lines, for moving
    std::set<std::pair<int, int>> pellets;
    std::set<std::pair<int, int>> powerPellets;
    std::vector<Ghost> ghosts;
    int pacX = PAC_START_X, pacY = PAC_START_Y;
    int pacDir = -1, wantDir = -1;
    bool moving = false;
    bool playerHuman = false;
    int playerWant = -1, playerBoost = 0;
    int lives = LIVES;
    int frightened = 0;
    int ghostHold = 27;   // about 2 seconds before the ghosts set off
    int dying = 0;
    int tick = 0;
    bool waka = false;
    bool won = false;
    bool GameOn = true;
    bool WaitingUntilOutput = false;
    int endTimer = 0;
    long long timer = 75;
};

const std::string &FPPPacman::getName() {
    static const std::string name = "Pacman";
    return name;
}

void FPPPacman::button(const std::string &button) {
    PixelOverlayModel *m = PixelOverlayManager::INSTANCE.getModel(modelName);
    if (m != nullptr) {
        PacmanEffect *effect = dynamic_cast<PacmanEffect*>(m->getRunningEffect());
        if (!effect) {
            if (findOption("overlay", "Overwrite") == "Transparent") {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::TransparentRGB));
            } else {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
            }
            effect = new PacmanEffect(m);
            m->setRunningEffect(effect, 50);
        } else {
            effect->button(button);
        }
    }
}
