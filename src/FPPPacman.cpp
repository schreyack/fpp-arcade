#include <fpp-pch.h>

#include "FPPPacman.h"
#include <vector>
#include <random>

#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"
#include "overlays/PixelOverlayEffects.h"

//Test
FPPPacman::FPPPacman(Json::Value &config) : FPPArcadeGame(config) {
    std::srand(time(NULL));
}

FPPPacman::~FPPPacman() {
}

class PacmanEffect : public FPPArcadeGameEffect {
public:
    PacmanEffect(PixelOverlayModel *m) : FPPArcadeGameEffect(m) {
        m->getSize(cols, rows);
        cols /= scale; rows /= scale;
        if (cols < 8) cols = 8;
        if (rows < 8) rows = 8;

        // build a simple map: border walls + scattered pellets
        grid.resize(rows);
        for (int r = 0; r < rows; r++) {
            grid[r].resize(cols, 1); // 1 = pellet
        }
        // walls on borders
        for (int r = 0; r < rows; r++) {
            grid[r][0] = 2; // wall
            grid[r][cols-1] = 2;
        }
        for (int c = 0; c < cols; c++) {
            grid[0][c] = 2;
            grid[rows-1][c] = 2;
        }

        // clear some pellets to make corridors
        for (int r = 2; r < rows-2; r+=2) {
            for (int c = 2; c < cols-2; c+=3) {
                grid[r][c] = 0; // empty
            }
        }

        pacmanX = cols/2;
        pacmanY = rows/2;
        pacDir = 0; // left

        // ghosts
        Ghost g;
        g.x = 1; g.y = 1;
        ghosts.push_back(g);
        g.x = cols-2; g.y = 1; ghosts.push_back(g);
        g.x = 1; g.y = rows-2; ghosts.push_back(g);

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
                    // wall
                    outputPixel(gx, gy, 80, 80, 160);
                } else if (grid[r][c] == 1) {
                    // pellet (small bright dot)
                    outputPixel(gx, gy, 200, 200, 0);
                }
            }
        }

        // pacman
        outputPixel(pacmanX, pacmanY, 255, 220, 0);

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
