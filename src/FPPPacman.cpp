#include <fpp-pch.h>

#include "FPPPacman.h"
#include <vector>
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

class PacmanEffect : public FPPArcadeGameEffect {
public:
    PacmanEffect(PixelOverlayModel *m) : FPPArcadeGameEffect(m) {
        // Dynamic maze generation: inspired by classic Pac-Man, but scalable and interesting
        m->getSize(cols, rows);
        cols /= scale; rows /= scale;
        if (cols < 8) cols = 8;
        if (rows < 8) rows = 8;

        grid.resize(rows);
        for (int r = 0; r < rows; r++) {
            grid[r].resize(cols, 1); // 1 = pellet
        }
        // Border walls
        for (int r = 0; r < rows; r++) {
            grid[r][0] = 2;
            grid[r][cols-1] = 2;
        }
        for (int c = 0; c < cols; c++) {
            grid[0][c] = 2;
            grid[rows-1][c] = 2;
        }

        // Add vertical corridors
        int vSpacing = std::max(3, cols / 4);
        for (int c = vSpacing; c < cols-1; c += vSpacing) {
            for (int r = 1; r < rows-1; r++) {
                // Leave gaps every 2 rows for accessibility
                if ((r % 4) != 1) grid[r][c] = 2;
            }
        }

        // Add horizontal corridors
        int hSpacing = std::max(3, rows / 4);
        for (int r = hSpacing; r < rows-1; r += hSpacing) {
            for (int c = 1; c < cols-1; c++) {
                // Leave gaps every 2 columns for accessibility
                if ((c % 4) != 1) grid[r][c] = 2;
            }
        }

        // Central ghost house (rectangle with opening)
        int houseW = std::max(5, cols / 4);
        int houseH = 3;
        int houseLeft = cols/2 - houseW/2;
        int houseTop = rows/2 - 1;
        for (int x = houseLeft; x < houseLeft + houseW; x++) {
            grid[houseTop][x] = 2;
            grid[houseTop + houseH - 1][x] = 2;
        }
        for (int y = houseTop; y < houseTop + houseH; y++) {
            grid[y][houseLeft] = 2;
            grid[y][houseLeft + houseW - 1] = 2;
        }
        // Opening in ghost house
        int doorW = std::min(3, houseW - 2);
        int doorStart = houseLeft + (houseW / 2) - (doorW / 2);
        for (int d = 0; d < doorW; d++) {
            grid[houseTop][doorStart + d] = 0;
        }

        // Ensure no unreachable areas: clear pellets in corridors
        for (int r = 2; r < rows-2; r+=2) {
            for (int c = 2; c < cols-2; c+=3) {
                if (grid[r][c] != 2) grid[r][c] = 0;
            }
        }

        // Place ghosts near center
        ghosts.clear();
        ghosts.push_back(Ghost{cols/2, rows/2, 0});
        ghosts.push_back(Ghost{cols/2-1, rows/2, 1});
        ghosts.push_back(Ghost{cols/2+1, rows/2, 2});
        pacmanX = 1;
        pacmanY = 1;
        pacDir = 0;
        timer = 150;
    }

    struct Ghost { int x; int y; int dir = 0; };

    const std::string &name() const override {
        static std::string NAME = "Pacman";
        return NAME;
    }

    void CopyToModel() {
        model->clearOverlayBuffer();

        // draw grid
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                int gx = c; int gy = r;
                if (grid[r][c] == 2) {
                    // wall - bright blue
                    outputPixel(gx, gy, 0, 0, 255);
                } else if (grid[r][c] == 1) {
                    // pellet (very dim yellow)
                    outputPixel(gx, gy, 48, 48, 0);
                }
            }
        }

        // pacman
        // draw Pac-Man in bright yellow
        outputPixel(pacmanX, pacmanY, 255, 255, 0);

        // ghosts
        for (auto &gh : ghosts) {
            outputPixel(gh.x, gh.y, 255, 0, 0);
        }

        model->flushOverlayBuffer();
    }

    void movePacman() {
        int nx = pacmanX; int ny = pacmanY;
        switch (pacDir) {
            case 0: nx--; break; // left
            case 1: ny--; break; // up
            case 2: nx++; break; // right
            case 3: ny++; break; // down
        }
        if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) return;
        if (grid[ny][nx] != 2) {
            pacmanX = nx; pacmanY = ny;
            // eat pellet
            if (grid[ny][nx] == 1) {
                grid[ny][nx] = 0;
            }
        }
    }

    void moveGhosts() {
        for (auto &gh : ghosts) {
            // simple random move
            int bestDir = gh.dir;
            int dirs[4][2] = {{-1,0},{0,-1},{1,0},{0,1}};
            std::vector<int> opts;
            for (int d = 0; d < 4; d++) {
                int nx = gh.x + dirs[d][0];
                int ny = gh.y + dirs[d][1];
                if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) continue;
                if (grid[ny][nx] != 2) opts.push_back(d);
            }
            if (!opts.empty()) {
                int pick = opts[rand() % opts.size()];
                gh.x += dirs[pick][0];
                gh.y += dirs[pick][1];
                gh.dir = pick;
            }
        }
    }

    virtual int32_t update() override {
        if (!GameOn) {
            if (WaitingUntilOutput) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
                return 0;
            }
            model->clearOverlayBuffer();
            model->flushOverlayBuffer();
            WaitingUntilOutput = true;
            return -1;
        }

        if (Paused) {
            // when paused, just redraw current state and don't advance
            CopyToModel();
            return timer;
        }

        movePacman();
        moveGhosts();

        // check collisions
        for (auto &gh : ghosts) {
            if (gh.x == pacmanX && gh.y == pacmanY) {
                GameOn = false;
                outputString("GAME", cols/2 - 4, rows/2-3);
                outputString("OVER", cols/2 - 4, rows/2+1);
                model->flushOverlayBuffer();
                return 2000;
            }
        }

        // check win: no pellets
        bool any = false;
        for (int r = 0; r < rows && !any; r++) {
            for (int c = 0; c < cols; c++) {
                if (grid[r][c] == 1) { any = true; break; }
            }
        }
        if (!any) {
            GameOn = false;
            outputString("YOU", cols/2 - 3, rows/2-3);
            outputString("WIN", cols/2 - 3, rows/2+1);
            model->flushOverlayBuffer();
            return 2000;
        }

        CopyToModel();
        return timer;
    }

    void button(const std::string &butt) {
        std::string button = butt;
        std::string joystickName = "";
        size_t pos = butt.find('|');
        if (pos != std::string::npos) {
            button = butt.substr(0, pos);
            joystickName = butt.substr(pos+1);
        }
        if (button == "Left - Pressed") {
            pacDir = 0;
        } else if (button == "Up - Pressed") {
            pacDir = 1;
        } else if (button == "Right - Pressed") {
            pacDir = 2;
        } else if (button == "Down - Pressed") {
            pacDir = 3;
        } else if (button == "Fire - Pressed") {
            // toggle pause (don't mark GameOn false which indicates game over)
            Paused = !Paused;
        }
    }

    int rows = 20;
    int cols = 11;
    int scale = 1;
    std::vector<std::vector<int>> grid; // 0 empty, 1 pellet, 2 wall
    int pacmanX = 1;
    int pacmanY = 1;
    int pacDir = 0;
    std::vector<Ghost> ghosts;
    bool GameOn = true;
    bool Paused = false;
    bool WaitingUntilOutput = false;
    long long timer = 200;
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
