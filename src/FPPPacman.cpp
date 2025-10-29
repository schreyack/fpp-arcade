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

        // add a more complex Pac-Man-like maze while ensuring no fully enclosed regions
        int midC = cols / 2;
        int midR = rows / 2;

        int minGap = 3;
        int gapW = std::max(minGap, std::min(minGap, cols - 6));
        int gapH = std::max(minGap, std::min(minGap, rows - 8));

        int gapColStart = std::max(2, midC - (gapW / 2));
        int gapColEnd = std::min(cols-3, gapColStart + gapW - 1);
        int gapRowStart = std::max(2, midR - (gapH / 2));
        int gapRowEnd = std::min(rows-3, gapRowStart + gapH - 1);

        // vertical corridors: left, center-left, center-right, right
        std::vector<int> vcols;
        vcols.push_back(2);
        if (cols > 10) vcols.push_back(std::max(3, midC - 3));
        if (cols > 12) vcols.push_back(std::min(cols-4, midC + 3));
        vcols.push_back(cols - 3);

        for (int vc : vcols) {
            for (int r = 1; r < rows-1; r++) {
                // leave center opening and small top/bottom openings to avoid isolating areas
                bool inCenterGap = (r >= gapRowStart && r <= gapRowEnd);
                bool inTopGap = (r >= 2 && r <= 2 + (gapRowStart/3));
                bool inBottomGap = (r >= rows-3-(gapRowStart/3) && r <= rows-2);
                if (!inCenterGap && !inTopGap && !inBottomGap) {
                    if (vc > 1 && vc < cols-1) grid[r][vc] = 2;
                }
            }
        }

        // horizontal corridors: top, middle-top, middle-bottom, bottom
        std::vector<int> hrows;
        hrows.push_back(3);
        if (rows > 12) hrows.push_back(std::max(4, midR - 2));
        if (rows > 14) hrows.push_back(std::min(rows-5, midR + 2));
        hrows.push_back(rows - 4);

        for (int hr : hrows) {
            for (int c = 1; c < cols-1; c++) {
                bool inCenterGap = (c >= gapColStart && c <= gapColEnd);
                bool inLeftGap = (c >= 2 && c <= 2 + (gapColStart/3));
                bool inRightGap = (c >= cols-3-(gapColStart/3) && c <= cols-2);
                if (!inCenterGap && !inLeftGap && !inRightGap) {
                    if (hr > 1 && hr < rows-1) grid[hr][c] = 2;
                }
            }
        }

        // central ghost house: small rectangle with an opening
        int houseW = std::min(cols - 6, 7);
        int houseH = 3;
        int houseLeft = midC - houseW / 2;
        int houseTop = midR - 1;
        if (houseLeft < 2) houseLeft = 2;
        if (houseTop < 2) houseTop = 2;
        for (int x = houseLeft; x < houseLeft + houseW; x++) {
            grid[houseTop][x] = 2;
            grid[houseTop + houseH - 1][x] = 2;
        }
        for (int y = houseTop; y < houseTop + houseH; y++) {
            grid[y][houseLeft] = 2;
            grid[y][houseLeft + houseW - 1] = 2;
        }
        int doorW = std::min(3, houseW - 2);
        int doorStart = houseLeft + (houseW / 2) - (doorW / 2);
        for (int d = 0; d < doorW; d++) {
            grid[houseTop][doorStart + d] = 0;
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
        ghosts.clear();
        int numGhosts = 3 + (rand() % 8); // 3-10 ghosts
        int px = pacmanX;
        int py = pacmanY;
        std::vector<std::pair<int, int>> ghostPositions = {
            {1, 1}, {cols-2, 1}, {1, rows-2}, {cols-2, rows-2}, {cols/2, 1}, {cols/2, rows-2}, {1, cols/2}, {rows-2, cols/2}, {cols/2, rows/2}, {cols/2-1, rows/2}
        };
        // Filter out positions too close to Pacman (distance <= 1)
        std::vector<std::pair<int, int>> validGhostPositions;
        for (auto &pos : ghostPositions) {
            int dx = abs(pos.first - px);
            int dy = abs(pos.second - py);
            if (dx > 1 || dy > 1) {
                validGhostPositions.push_back(pos);
            }
        }
        // If not enough valid positions, fill with random positions far from Pacman
        while (validGhostPositions.size() < (size_t)numGhosts) {
            int gx = rand() % cols;
            int gy = rand() % rows;
            int dx = abs(gx - px);
            int dy = abs(gy - py);
            if ((dx > 1 || dy > 1) && grid[gy][gx] != 2) {
                validGhostPositions.push_back({gx, gy});
            }
        }
        for (int i = 0; i < numGhosts; ++i) {
            Ghost g;
            int idx = i % validGhostPositions.size();
            g.x = validGhostPositions[idx].first;
            g.y = validGhostPositions[idx].second;
            ghosts.push_back(g);
        }

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
