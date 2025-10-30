#include <fpp-pch.h>

#include "FPPPacman.h"
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
    int pacDir = -1;  // -1 = no movement until input
    int playerGhostDir = 0;
    std::vector<std::vector<int>> grid; // 0 empty, 1 pellet, 2 wall
    std::set<std::pair<int, int>> eatenPellets; // Track which pellets have been eaten
    std::vector<Ghost> ghosts;
    Ghost playerGhost; // Special ghost controlled by player 2
    bool GameOn = true;
    bool Paused = false;
    bool WaitingUntilOutput = false;
    long long timer = 150;

    PacmanEffect(PixelOverlayModel *m) : FPPArcadeGameEffect(m) {
        m->getSize(cols, rows);
        cols /= scale; rows /= scale;
        if (cols < 8) cols = 8;
        if (rows < 8) rows = 8;

        // Initialize grid with empty spaces
        grid.resize(rows);
        for (int r = 0; r < rows; r++) {
            grid[r].resize(cols, 0); // 0 = empty
        }
        
        // Create central room sized to contain approximately 15x9 pellets
        // Since pellets are every 2 units, 15 pellets wide = 30 units, 9 pellets high = 18 units
        int targetPelletsWide = 15;
        int targetPelletsHigh = 9;
        int pelletSpacing = 2;
        
        int targetWidth = targetPelletsWide * pelletSpacing;
        int targetHeight = targetPelletsHigh * pelletSpacing;
        
        // Adjust to fit within grid bounds with wall thickness
        int wallThickness = 2;
        int maxInteriorWidth = cols - 2 * wallThickness;
        int maxInteriorHeight = rows - 2 * wallThickness;
        
        int roomWidth = (targetWidth < maxInteriorWidth) ? targetWidth : maxInteriorWidth;
        int roomHeight = (targetHeight < maxInteriorHeight) ? targetHeight : maxInteriorHeight;
        
        int roomStartX = (cols - roomWidth) / 2;
        int roomStartY = (rows - roomHeight) / 2;
        int roomEndX = roomStartX + roomWidth - 1;
        int roomEndY = roomStartY + roomHeight - 1;
        
        // Create walls (2 units thick)
        // Top wall
        for (int r = roomStartY; r < roomStartY + wallThickness; r++) {
            for (int c = roomStartX; c <= roomEndX; c++) {
                if (r < rows && c < cols) grid[r][c] = 2;
            }
        }
        // Bottom wall
        for (int r = roomEndY - wallThickness + 1; r <= roomEndY; r++) {
            for (int c = roomStartX; c <= roomEndX; c++) {
                if (r >= 0 && c < cols) grid[r][c] = 2;
            }
        }
        // Left wall
        for (int c = roomStartX; c < roomStartX + wallThickness; c++) {
            for (int r = roomStartY; r <= roomEndY; r++) {
                if (c < cols && r < rows) grid[r][c] = 2;
            }
        }
        // Right wall with doorway
        for (int c = roomEndX - wallThickness + 1; c <= roomEndX; c++) {
            for (int r = roomStartY; r <= roomEndY; r++) {
                // Leave middle section open for doorway (wide enough for Pacman)
                int doorStart = roomStartY + roomHeight/2 - 2; // 4 units wide door
                int doorEnd = roomStartY + roomHeight/2 + 2;
                if (r < doorStart || r > doorEnd) {
                    if (c >= 0 && r < rows) grid[r][c] = 2;
                }
            }
        }
        
        // Place pellets everywhere except on walls
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (grid[r][c] != 2) { // Not a wall
                    grid[r][c] = 1; // Pellet
                }
            }
        }
        
        // Place Pacman in the center of the central room
        pacmanX = cols / 2;
        pacmanY = rows / 2;
        
        // Place ghosts in random locations (avoiding Pacman)
        ghosts.clear();
        int numGhosts = 3 + (rand() % 4);
        
        for (int i = 0; i < numGhosts; i++) {
            Ghost g;
            // Random position, but at least 5 units away from Pacman
            int attempts = 0;
            do {
                g.x = pacRadius + 1 + (rand() % (cols - 2 * pacRadius - 2));
                g.y = pacRadius + 1 + (rand() % (rows - 2 * pacRadius - 2));
                attempts++;
            } while ((abs(g.x - pacmanX) + abs(g.y - pacmanY) < 5) && attempts < 20);
            
            g.dir = rand() % 4;
            ghosts.push_back(g);
        }
        
        // Place player-controlled ghost at random location (avoiding Pacman)
        int attempts = 0;
        do {
            playerGhost.x = pacRadius + 1 + (rand() % (cols - 2 * pacRadius - 2));
            playerGhost.y = pacRadius + 1 + (rand() % (rows - 2 * pacRadius - 2));
            attempts++;
        } while ((abs(playerGhost.x - pacmanX) + abs(playerGhost.y - pacmanY) < 5) && attempts < 20);
        playerGhost.dir = 0;

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
        // Draw pellet inside mouth if Pacman has uneaten pellets within his radius
        bool hasNearbyPellet = false;
        for (int pr = y - pacRadius; pr <= y + pacRadius && !hasNearbyPellet; pr += 2) {
            for (int pc = x - pacRadius; pc <= x + pacRadius && !hasNearbyPellet; pc += 2) {
                if (pr >= 0 && pr < rows && pc >= 0 && pc < cols) {
                    int dx = pc - x;
                    int dy = pr - y;
                    if (dx*dx + dy*dy <= pacRadius*pacRadius && 
                        eatenPellets.find({pc, pr}) == eatenPellets.end()) {
                        hasNearbyPellet = true;
                    }
                }
            }
        }
        if (hasNearbyPellet) {
            // Show pellet in mouth at a slight offset
            int mouthX = x, mouthY = y;
            if (dir == 0) mouthX = x - 1;
            else if (dir == 2) mouthX = x + 1;
            else if (dir == 1) mouthY = y - 1;
            else if (dir == 3) mouthY = y + 1;
            outputPixel(mouthX, mouthY, 48, 48, 0);
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
        
        // Draw walls first (behind everything)
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (grid[r][c] == 2) { // Wall
                    outputPixel(c, r, 0, 0, 100); // Dark blue walls
                }
            }
        }
        
        // Draw pellets in a grid pattern (if not eaten)
        for (int r = 0; r < rows; r += 2) {
            for (int c = 0; c < cols; c += 2) {
                if (eatenPellets.find({c, r}) == eatenPellets.end()) {
                    outputPixel(c, r, 255, 200, 0);
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
        if (pacDir < 0) return;  // Don't move if no direction set
        
        int nx = pacmanX; int ny = pacmanY;
        switch (pacDir) {
            case 0: nx--; break; // left
            case 1: ny--; break; // up
            case 2: nx++; break; // right
            case 3: ny++; break; // down
        }
        if (canMoveTo(nx, ny, pacRadius)) {
            pacmanX = nx; pacmanY = ny;
        }
        
        // Eat pellets within Pacman's radius
        for (int pr = pacmanY - pacRadius; pr <= pacmanY + pacRadius; pr += 2) {
            for (int pc = pacmanX - pacRadius; pc <= pacmanX + pacRadius; pc += 2) {
                if (pr >= 0 && pr < rows && pc >= 0 && pc < cols) {
                    int dx = pc - pacmanX;
                    int dy = pr - pacmanY;
                    if (dx*dx + dy*dy <= pacRadius*pacRadius) {
                        eatenPellets.insert({pc, pr});
                    }
                }
            }
        }
    }

    bool isPositionOccupied(int x, int y, int excludeIndex = -1) {
        // Check if position is occupied by any ghost (except optionally excluded one)
        for (size_t i = 0; i < ghosts.size(); i++) {
            if ((int)i != excludeIndex) {
                int dx = ghosts[i].x - x;
                int dy = ghosts[i].y - y;
                int distanceSquared = dx*dx + dy*dy;
                int radiusSum = ghostSize/2 + ghostSize/2; // Both have same radius
                if (distanceSquared <= radiusSum * radiusSum) {
                    return true;
                }
            }
        }
        // Also check player ghost
        if (excludeIndex != -2) {
            int dx = playerGhost.x - x;
            int dy = playerGhost.y - y;
            int distanceSquared = dx*dx + dy*dy;
            int radiusSum = ghostSize/2 + ghostSize/2;
            if (distanceSquared <= radiusSum * radiusSum) {
                return true;
            }
        }
        return false;
    }

    void moveGhosts() {
        for (size_t i = 0; i < ghosts.size(); i++) {
            auto &gh = ghosts[i];
            int dirs[4][2] = {{-1,0},{0,-1},{1,0},{0,1}};
            std::vector<int> opts;
            for (int d = 0; d < 4; d++) {
                int nx = gh.x + dirs[d][0];
                int ny = gh.y + dirs[d][1];
                if (canMoveTo(nx, ny, ghostSize/2) && !isPositionOccupied(nx, ny, i)) {
                    opts.push_back(d);
                }
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
        if (canMoveTo(nx, ny, ghostSize/2) && !isPositionOccupied(nx, ny, -2)) {
            playerGhost.x = nx;
            playerGhost.y = ny;
            playerGhost.dir = playerGhostDir;
        }
    }

    bool checkCollision(int ax, int ay, int ar, int bx, int by, int br) {
        // Circular collision detection
        int dx = ax - bx;
        int dy = ay - by;
        int distanceSquared = dx*dx + dy*dy;
        int radiusSum = ar + br;
        return distanceSquared <= radiusSum * radiusSum;
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

        // check win: all pellets eaten
        int totalPellets = 0;
        for (int r = 0; r < rows; r += 2) {
            for (int c = 0; c < cols; c += 2) {
                if (grid[r][c] != 2) { // Only count pellets not on walls
                    totalPellets++;
                }
            }
        }
        if ((int)eatenPellets.size() >= totalPellets) {
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
