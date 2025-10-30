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

        // Designed maze with proper corridors sized for large sprites
        grid.resize(rows);
        for (int r = 0; r < rows; r++) {
            grid[r].resize(cols, 0); // 0 = empty
        }
        
        // Fill everything with pellets first
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                grid[r][c] = 1;
            }
        }
        
        // Draw thick walls (3-4 pixels wide) to form corridors
        int wallThickness = 3;
        
        // Vertical walls (left and right sides)
        int leftWall = 6;
        int rightWall = cols - 7;
        for (int r = 0; r < rows; r++) {
            for (int w = 0; w < wallThickness; w++) {
                if (leftWall + w < cols) grid[r][leftWall + w] = 2;
                if (rightWall - w >= 0) grid[r][rightWall - w] = 2;
            }
        }
        
        // Horizontal walls (top and bottom)
        int topWall = 5;
        int bottomWall = rows - 6;
        for (int c = 0; c < cols; c++) {
            for (int w = 0; w < wallThickness; w++) {
                if (topWall + w < rows) grid[topWall + w][c] = 2;
                if (bottomWall - w >= 0) grid[bottomWall - w][c] = 2;
            }
        }
        
        // Create center vertical wall
        int centerWall = cols / 2;
        for (int r = 0; r < rows; r++) {
            // Skip the middle third to create passage
            if (r < rows/3 || r > 2*rows/3) {
                for (int w = 0; w < wallThickness; w++) {
                    if (centerWall + w < cols) grid[r][centerWall + w] = 2;
                }
            }
        }
        
        // Create some internal divisions with gaps
        int divideCol1 = cols / 4;
        int divideCol2 = 3 * cols / 4;
        
        // Left division (with gap in middle)
        for (int r = 0; r < rows; r++) {
            if (r < rows/3 || r > 2*rows/3) {
                for (int w = 0; w < wallThickness; w++) {
                    if (divideCol1 + w < cols) grid[r][divideCol1 + w] = 2;
                }
            }
        }
        
        // Right division (with gap in middle)
        for (int r = 0; r < rows; r++) {
            if (r < rows/3 || r > 2*rows/3) {
                for (int w = 0; w < wallThickness; w++) {
                    if (divideCol2 + w < cols) grid[r][divideCol2 + w] = 2;
                }
            }
        }
        
        // Clear some areas to create rooms
        for (int r = 2; r < rows - 2; r++) {
            for (int c = 2; c < cols - 2; c++) {
                if (grid[r][c] == 0) grid[r][c] = 1;
            }
        }
        // Place Pacman in a guaranteed open area
        pacmanX = cols/2;
        pacmanY = rows/2;
        // Place ghosts in guaranteed open areas
        ghosts.clear();
        int numGhosts = 3 + (rand() % 4);
        std::vector<std::pair<int, int>> ghostPositions;
        for (int r = pacRadius; r < rows-pacRadius; r += 3) {
            for (int c = pacRadius; c < cols-pacRadius; c += 3) {
                if (grid[r][c] != 2 && !(abs(c-pacmanX)<=3 && abs(r-pacmanY)<=3)) {
                    ghostPositions.push_back({c, r});
                }
            }
        }
        // Place player-controlled ghost in a guaranteed open area
        playerGhost.x = pacRadius+1;
        playerGhost.y = pacRadius+1;
        playerGhost.dir = 0;
        // Remove this position from ghostPositions
        ghostPositions.erase(
            std::remove_if(ghostPositions.begin(), ghostPositions.end(),
                [&](const std::pair<int,int>& pos) {
                    return pos.first == playerGhost.x && pos.second == playerGhost.y;
                }),
            ghostPositions.end());
        for (int i = 0; i < numGhosts && i < (int)ghostPositions.size(); ++i) {
            Ghost g;
            g.x = ghostPositions[i].first;
            g.y = ghostPositions[i].second;
            ghosts.push_back(g);
        }

        timer = 150;
    }

    struct Ghost { int x; int y; int dir = 0; };
    Ghost playerGhost; // Special ghost controlled by player 2
    int playerGhostDir = 0;

    const std::string &name() const override {
        static std::string NAME = "Pacman";
        return NAME;
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
        // Eyes (white)
        outputPixel(x-1, y-2, 255,255,255);
        outputPixel(x+0, y-2, 255,255,255);
        // Pupils (black)
        outputPixel(x-1, y-2, 0,0,0);
        outputPixel(x+0, y-2, 0,0,0);
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
                    // Only draw pellet if Pacman is not overlapping
                    if (!(abs(pacmanX-gx)<=1 && abs(pacmanY-gy)<=1))
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

    // Update pellet spacing and wall gap for larger sprites
    int pacRadius = 2; // Pacman radius
    int ghostSize = 4; // Ghost width/height
    int minWallGap = std::max(5, pacRadius*2+1); // Minimum gap between walls

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
