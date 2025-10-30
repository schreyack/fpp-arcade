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
    struct Ghost { int x; int y; int dir = 0; };
    
    // Member variables declared first
    int rows = 20;
    int cols = 11;
    int scale = 1;
    int pacRadius = 2; // Pacman radius
    int ghostSize = 4; // Ghost width/height
    int minWallGap = 5; // Minimum gap between walls
    int pacmanX = 1;
    int pacmanY = 1;
    int pacDir = 0;
    int playerGhostDir = 0;
    std::vector<std::vector<int>> grid; // 0 empty, 1 pellet, 2 wall
    std::vector<Ghost> ghosts;
    Ghost playerGhost; // Special ghost controlled by player 2
    bool GameOn = true;
    bool Paused = false;
    bool WaitingUntilOutput = false;
    long long timer = 200;

    PacmanEffect(PixelOverlayModel *m) : FPPArcadeGameEffect(m) {
        m->getSize(cols, rows);
        cols /= scale; rows /= scale;
        if (cols < 8) cols = 8;
        if (rows < 8) rows = 8;

        // Start with all walls
        grid.resize(rows);
        for (int r = 0; r < rows; r++) {
            grid[r].resize(cols, 2); // 2 = wall
        }
        
        // Helper lambda to carve a horizontal corridor
        auto hCorridor = [this](int row, int colStart, int colEnd) {
            for (int c = colStart; c <= colEnd && c < cols; c++) {
                for (int w = 0; w < 3; w++) {
                    int r = row + w;
                    if (r < rows) grid[r][c] = 0;
                }
            }
        };
        
        // Helper lambda to carve a vertical corridor
        auto vCorridor = [this](int col, int rowStart, int rowEnd) {
            for (int r = rowStart; r <= rowEnd && r < rows; r++) {
                for (int w = 0; w < 3; w++) {
                    int c = col + w;
                    if (c < cols) grid[r][c] = 0;
                }
            }
        };
        
        // Create main boundary with borders
        hCorridor(1, 1, cols - 2);          // top border
        hCorridor(rows - 4, 1, cols - 2);   // bottom border
        vCorridor(1, 1, rows - 4);          // left border
        vCorridor(cols - 4, 1, rows - 4);   // right border
        
        // Main horizontal thoroughfare through center
        int centerRow = rows / 2 - 1;
        hCorridor(centerRow, 1, cols - 2);
        
        // Left-side vertical spine
        int leftSpine = cols / 4;
        vCorridor(leftSpine, 1, rows - 4);
        
        // Right-side vertical spine
        int rightSpine = 3 * cols / 4;
        vCorridor(rightSpine, 1, rows - 4);
        
        // Connect left spine to top (quarter points)
        int q1 = cols / 8;
        vCorridor(q1, 1, centerRow + 1);
        hCorridor(6, q1, leftSpine);
        
        // Connect right spine to top
        int q3 = 7 * cols / 8;
        vCorridor(q3, 1, centerRow + 1);
        hCorridor(6, rightSpine, q3);
        
        // Connect left spine to bottom
        hCorridor(rows - 8, q1, leftSpine);
        
        // Connect right spine to bottom
        hCorridor(rows - 8, rightSpine, q3);
        
        // Upper left nook
        hCorridor(3, 2, leftSpine - 2);
        
        // Upper right nook
        hCorridor(3, rightSpine + 3, cols - 3);
        
        // Lower left nook
        hCorridor(rows - 6, 2, leftSpine - 2);
        
        // Lower right nook
        hCorridor(rows - 6, rightSpine + 3, cols - 3);
        
        // Small cross-corridors for connectivity
        vCorridor(cols / 2 - 1, centerRow - 3, centerRow + 6);
        hCorridor(centerRow - 3, leftSpine, rightSpine);
        hCorridor(centerRow + 4, leftSpine, rightSpine);
        
        // Fill open corridor cells with pellets
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (grid[r][c] == 0) {
                    grid[r][c] = 1; // pellet
                }
            }
        }
        // Place Pacman in a guaranteed open area (center)
        pacmanX = cols/2;
        pacmanY = rows/2;
        while (!canMoveTo(pacmanX, pacmanY, pacRadius)) {
            pacmanX++;
            if (pacmanX >= cols - pacRadius) {
                pacmanX = pacRadius + 1;
                pacmanY++;
            }
        }
        
        // Place ghosts in guaranteed open areas away from Pacman
        ghosts.clear();
        int numGhosts = 3 + (rand() % 4);
        std::vector<std::pair<int, int>> ghostPositions;
        
        for (int r = pacRadius + 1; r < rows - pacRadius; r++) {
            for (int c = pacRadius + 1; c < cols - pacRadius; c++) {
                if (grid[r][c] != 2) {
                    int distToPac = abs(c - pacmanX) + abs(r - pacmanY);
                    if (distToPac > 5) {
                        ghostPositions.push_back({c, r});
                    }
                }
            }
        }
        
        // Place player-controlled ghost
        playerGhost.x = pacRadius + 1;
        playerGhost.y = pacRadius + 1;
        if (!canMoveTo(playerGhost.x, playerGhost.y, ghostSize/2)) {
            playerGhost.x = cols - pacRadius - 2;
            playerGhost.y = rows - pacRadius - 2;
        }
        playerGhost.dir = 0;
        
        // Spawn AI ghosts
        for (int i = 0; i < numGhosts && i < (int)ghostPositions.size(); ++i) {
            Ghost g;
            g.x = ghostPositions[i].first;
            g.y = ghostPositions[i].second;
            g.dir = 0;
            ghosts.push_back(g);
        }

        timer = 150;
    }

    const std::string &name() const override {
        static std::string NAME = "Pacman";
        return NAME;
    }

    bool canMoveTo(int x, int y, int radius) {
        // Check bounding box for wall collisions
        for (int dx = -radius; dx <= radius; dx++) {
            for (int dy = -radius; dy <= radius; dy++) {
                int nx = x+dx, ny = y+dy;
                if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) return false;
                if ((dx*dx + dy*dy <= radius*radius) && grid[ny][nx] == 2) return false;
            }
        }
        return true;
    }

    void drawPacman(int x, int y, int dir) {
        int r = 2; // radius in grid units
        // Draw Pacman body and mouth
        for (int dx = -r; dx <= r; dx++) {
            for (int dy = -r; dy <= r; dy++) {
                if (dx*dx + dy*dy <= r*r) {
                    // mouth opening: skip pixels in direction of movement except for front edge
                    bool mouth = false;
                    bool front = false;
                    if (dir == 0) { mouth = (dx < 0 && abs(dy) <= r/2); front = (dx == -r && abs(dy) <= r/2); }
                    if (dir == 1) { mouth = (dy < 0 && abs(dx) <= r/2); front = (dy == -r && abs(dx) <= r/2); }
                    if (dir == 2) { mouth = (dx > 0 && abs(dy) <= r/2); front = (dx == r && abs(dy) <= r/2); }
                    if (dir == 3) { mouth = (dy > 0 && abs(dx) <= r/2); front = (dy == r && abs(dx) <= r/2); }
                    if (!mouth || front) outputPixel(x+dx, y+dy, 255, 255, 0);
                }
            }
        }
        // Fill the front edge of Pacman in the direction of movement
        for (int i = -r/2; i <= r/2; i++) {
            if (dir == 0) outputPixel(x-r, y+i, 255,255,0);
            if (dir == 1) outputPixel(x+i, y-r, 255,255,0);
            if (dir == 2) outputPixel(x+r, y+i, 255,255,0);
            if (dir == 3) outputPixel(x+i, y+r, 255,255,0);
        }
        // Draw eye
        int eyeX = x, eyeY = y;
        if (dir == 0) { eyeX = x; eyeY = y-1; }
        if (dir == 1) { eyeX = x+1; eyeY = y; }
        if (dir == 2) { eyeX = x; eyeY = y-1; }
        if (dir == 3) { eyeX = x-1; eyeY = y; }
        outputPixel(eyeX, eyeY, 0,0,0);
        // Draw pellet inside mouth if Pacman overlaps a pellet
        int pelletX = x, pelletY = y;
        if (dir == 0) pelletX = x-2;
        if (dir == 1) pelletY = y-2;
        if (dir == 2) pelletX = x+2;
        if (dir == 3) pelletY = y+2;
        if (pelletX >= 0 && pelletY >= 0 && pelletX < cols && pelletY < rows && grid[pelletY][pelletX] == 1) {
            outputPixel(pelletX, pelletY, 48, 48, 0);
        }
    }
    void drawGhost(int x, int y, int r, int g, int b) {
        // Draw ghost as a larger rectangle with eyes
        for (int dx = -2; dx <= 1; dx++) {
            for (int dy = -2; dy <= 1; dy++) {
                outputPixel(x+dx, y+dy, r, g, b);
            }
        }
        // Eyes: draw white squares first
        outputPixel(x-1, y-2, 255, 255, 255);
        outputPixel(x+0, y-2, 255, 255, 255);
        // Pupils: small black center in each eye (offset inward slightly)
        outputPixel(x-1, y-1, 0, 0, 0);
        outputPixel(x+0, y-1, 0, 0, 0);
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
                    // Only draw pellet if Pacman is not overlapping (within pacRadius)
                    if (!(abs(pacmanX-c) <= pacRadius && abs(pacmanY-r) <= pacRadius))
                        outputPixel(gx, gy, 48, 48, 0);
                }
            }
        }
        // draw Pacman as scaled circle with mouth
        drawPacman(pacmanX, pacmanY, pacDir);
        // draw ghosts
        for (auto &gh : ghosts) {
            drawGhost(gh.x, gh.y, 255, 0, 0);
        }
        // player-controlled ghost (draw in cyan)
        drawGhost(playerGhost.x, playerGhost.y, 0, 255, 255);
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
        if (canMoveTo(nx, ny, pacRadius)) {
            pacmanX = nx; pacmanY = ny;
            // eat pellet
            if (grid[ny][nx] == 1) {
                grid[ny][nx] = 0;
            }
        }
    }

    void moveGhosts() {
        for (auto &gh : ghosts) {
            int bestDir = gh.dir;
            int dirs[4][2] = {{-1,0},{0,-1},{1,0},{0,1}};
            std::vector<int> opts;
            for (int d = 0; d < 4; d++) {
                int nx = gh.x + dirs[d][0];
                int ny = gh.y + dirs[d][1];
                if (canMoveTo(nx, ny, ghostSize/2)) opts.push_back(d);
            }
            if (!opts.empty()) {
                int pick = opts[rand() % opts.size()];
                gh.x += dirs[pick][0];
                gh.y += dirs[pick][1];
                gh.dir = pick;
            }
        }
        // Move player-controlled ghost
        int dirs[4][2] = {{-1,0},{0,-1},{1,0},{0,1}};
        int nx = playerGhost.x + dirs[playerGhostDir][0];
        int ny = playerGhost.y + dirs[playerGhostDir][1];
        if (canMoveTo(nx, ny, ghostSize/2)) {
            playerGhost.x = nx;
            playerGhost.y = ny;
            playerGhost.dir = playerGhostDir;
        }
    }

    bool checkCollision(int ax, int ay, int ar, int bx, int by, int br) {
        // Simple bounding box overlap
        return abs(ax-bx) <= (ar+br-1) && abs(ay-by) <= (ar+br-1);
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
            if (checkCollision(pacmanX, pacmanY, pacRadius, gh.x, gh.y, ghostSize/2)) {
                GameOn = false;
                outputString("GAME", cols/2 - 4, rows/2-3);
                outputString("OVER", cols/2 - 4, rows/2+1);
                model->flushOverlayBuffer();
                return 2000;
            }
        }
        // check collision with player-controlled ghost
        if (checkCollision(pacmanX, pacmanY, pacRadius, playerGhost.x, playerGhost.y, ghostSize/2)) {
            GameOn = false;
            outputString("GAME", cols/2 - 4, rows/2-3);
            outputString("OVER", cols/2 - 4, rows/2+1);
            model->flushOverlayBuffer();
            return 2000;
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
        // Player 1 controls Pacman, Player 2 controls special ghost
        if (joystickName.ends_with("2")) {
            // Player 2 controls the ghost
            if (button == "Left - Pressed") {
                playerGhostDir = 0;
            } else if (button == "Up - Pressed") {
                playerGhostDir = 1;
            } else if (button == "Right - Pressed") {
                playerGhostDir = 2;
            } else if (button == "Down - Pressed") {
                playerGhostDir = 3;
            }
        } else {
            // Player 1 controls Pacman
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
    }
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
