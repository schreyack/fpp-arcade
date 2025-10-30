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
    int playerGhostSpeedBoostTimer = 0; // Timer for speed boost effect
    std::vector<std::vector<int>> grid; // 0 empty, 1 pellet, 2 wall
    std::set<std::pair<int, int>> eatenPellets; // Track which pellets have been eaten
    std::set<std::pair<int, int>> specialPellets; // Track special pellets
    std::vector<Ghost> ghosts;
    Ghost playerGhost; // Special ghost controlled by player 2
    bool GameOn = true;
    bool Paused = false;
    bool WaitingUntilOutput = false;
    long long timer = 75;
    int speedBoostTimer = 0;

    void drawRoom(int startX, int startY, int width, int height, int doorWall = -1) {
        int wallThickness = 2;
        int endX = startX + width - 1;
        int endY = startY + height - 1;
        
        // Create walls (2 units thick)
        // Top wall
        for (int r = startY; r < startY + wallThickness; r++) {
            for (int c = startX; c <= endX; c++) {
                if (r < rows && c < cols) {
                    if (doorWall == 0) { // Top wall has doorway
                        int doorStart = startX + width/2 - 3; // 7 units wide door
                        int doorEnd = startX + width/2 + 3;
                        if (c < doorStart || c > doorEnd) {
                            grid[r][c] = 2;
                        }
                    } else {
                        grid[r][c] = 2; // Solid wall
                    }
                }
            }
        }
        // Bottom wall
        for (int r = endY - wallThickness + 1; r <= endY; r++) {
            for (int c = startX; c <= endX; c++) {
                if (r >= 0 && c < cols) {
                    if (doorWall == 1) { // Bottom wall has doorway
                        int doorStart = startX + width/2 - 3; // 7 units wide door
                        int doorEnd = startX + width/2 + 3;
                        if (c < doorStart || c > doorEnd) {
                            grid[r][c] = 2;
                        }
                    } else {
                        grid[r][c] = 2; // Solid wall
                    }
                }
            }
        }
        // Left wall
        for (int c = startX; c < startX + wallThickness; c++) {
            for (int r = startY; r <= endY; r++) {
                if (c < cols && r < rows) {
                    if (doorWall == 2) { // Left wall has doorway
                        int doorStart = startY + height/2 - 3; // 7 units wide door
                        int doorEnd = startY + height/2 + 3;
                        if (r < doorStart || r > doorEnd) {
                            grid[r][c] = 2;
                        }
                    } else {
                        grid[r][c] = 2; // Solid wall
                    }
                }
            }
        }
        // Right wall
        for (int c = endX - wallThickness + 1; c <= endX; c++) {
            for (int r = startY; r <= endY; r++) {
                if (c >= 0 && r < rows) {
                    if (doorWall == 3) { // Right wall has doorway
                        int doorStart = startY + height/2 - 3; // 7 units wide door
                        int doorEnd = startY + height/2 + 3;
                        if (r < doorStart || r > doorEnd) {
                            grid[r][c] = 2;
                        }
                    } else {
                        grid[r][c] = 2; // Solid wall
                    }
                }
            }
        }
    }

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
        
        // Calculate central room size
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
        
        // Create four corner rooms, same size as central
        // Top-left room
        int tlStartX = 8;
        int tlStartY = 8;
        drawRoom(tlStartX, tlStartY, roomWidth, roomHeight, rand() % 4);
        
        // // Top-right room
        // int trStartX = cols - 8 - roomWidth;
        // int trStartY = 8;
        // drawRoom(trStartX, trStartY, roomWidth, roomHeight, rand() % 4);
        
        // // Bottom-left room
        // int blStartX = 8;
        // int blStartY = rows - 8 - roomHeight;
        // drawRoom(blStartX, blStartY, roomWidth, roomHeight, rand() % 4);
        
        // // Bottom-right room
        // int brStartX = cols - 8 - roomWidth;
        // int brStartY = rows - 8 - roomHeight;
        // drawRoom(brStartX, brStartY, roomWidth, roomHeight, rand() % 4);
        
        // // Draw central room
        // int roomStartX = (cols - roomWidth) / 2;
        // int roomStartY = (rows - roomHeight) / 2;
        // drawRoom(roomStartX, roomStartY, roomWidth, roomHeight, rand() % 4);
        

        // Place pellets at even coordinates (multiples of 2) except on walls
        for (int r = 0; r < rows; r += 2) {
            for (int c = 0; c < cols; c += 2) {
                if (grid[r][c] != 2) { // Not a wall
                    grid[r][c] = 1; // Pellet
                }
            }
        }
        
        // Change perimeter to walls
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (r == 0 || r == rows - 1 || c == 0 || c == cols - 1) {
                    grid[r][c] = 2;
                }
            }
        }
        
        // Replace some regular pellets with special pellets (2x2 blocks)
        int numSpecial = 1; // 1 special pellets
        for (int i = 0; i < numSpecial; i++) {
            int attempts = 0;
            bool placed = false;
            do {
                int r = (rand() % (rows / 2)) * 2; // Even row
                int c = (rand() % (cols / 2)) * 2; // Even column
                // Check if 2x2 block (4 pellet positions) are regular pellets
                if (r + 2 < rows && c + 2 < cols &&
                    grid[r][c] == 1 && grid[r][c + 2] == 1 &&
                    grid[r + 2][c] == 1 && grid[r + 2][c + 2] == 1) {
                    // Replace with special pellet block
                    grid[r][c] = 3;
                    grid[r][c + 2] = 3;
                    grid[r + 2][c] = 3;
                    grid[r + 2][c + 2] = 3;
                    specialPellets.insert({c, r}); // Store top-left position
                    placed = true;
                }
                attempts++;
            } while (!placed && attempts < 100);
        }
        
        // Place Pacman in the center of the central room
        pacmanX = cols / 2;
        pacmanY = rows / 2;
        
        // Place ghosts in random locations (avoiding Pacman and walls)
        ghosts.clear();
        int numGhosts = 1;
        
        for (int i = 0; i < numGhosts; i++) {
            Ghost g;
            // Random position, but at least 8 units away from Pacman (Euclidean distance), not on walls, and not too close to other ghosts
            int attempts = 0;
            bool tooClose = false;
            do {
                g.x = pacRadius + 1 + (rand() % (cols - 2 * pacRadius - 2));
                g.y = pacRadius + 1 + (rand() % (rows - 2 * pacRadius - 2));
                attempts++;
                // Check distance from existing ghosts
                tooClose = false;
                for (size_t j = 0; j < ghosts.size(); j++) {
                    int dx = g.x - ghosts[j].x;
                    int dy = g.y - ghosts[j].y;
                    if (dx*dx + dy*dy < 64) { // Less than 8 units apart
                        tooClose = true;
                        break;
                    }
                }
            } while ((((g.x - pacmanX)*(g.x - pacmanX) + (g.y - pacmanY)*(g.y - pacmanY) < 64 || grid[g.y][g.x] == 2) || tooClose) && attempts < 20);
            
            g.dir = rand() % 4;
            ghosts.push_back(g);
        }
        
        // Place player-controlled ghost at random location (avoiding Pacman and walls and other ghosts)
        int attempts = 0;
        bool tooClose = false;
        do {
            playerGhost.x = pacRadius + 1 + (rand() % (cols - 2 * pacRadius - 2));
            playerGhost.y = pacRadius + 1 + (rand() % (rows - 2 * pacRadius - 2));
            attempts++;
            // Check distance from existing ghosts
            tooClose = false;
            for (size_t j = 0; j < ghosts.size(); j++) {
                int dx = playerGhost.x - ghosts[j].x;
                int dy = playerGhost.y - ghosts[j].y;
                if (dx*dx + dy*dy < 64) { // Less than 8 units apart
                    tooClose = true;
                    break;
                }
            }
        } while ((((playerGhost.x - pacmanX)*(playerGhost.x - pacmanX) + (playerGhost.y - pacmanY)*(playerGhost.y - pacmanY) < 64 || grid[playerGhost.y][playerGhost.x] == 2) || tooClose) && attempts < 20);
        playerGhost.dir = 0;
        playerGhostSpeedBoostTimer = 0;

    }

    const std::string &name() const override {
        static std::string NAME = "Pacman";
        return NAME;
    }

    bool canMoveTo(int x, int y, int radius, bool isGhost = false) {
        // Check bounding box for wall collisions (ghosts can pass through walls)
        for (int dx = -radius; dx <= radius; dx++) {
            for (int dy = -radius; dy <= radius; dy++) {
                int nx = x+dx, ny = y+dy;
                if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) return false;
                if (!isGhost && (dx*dx + dy*dy <= radius*radius) && grid[ny][nx] == 2) return false;
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
                        grid[pr][pc] == 1 && eatenPellets.find({pc, pr}) == eatenPellets.end()) {
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
        
        // Draw pellets in a grid pattern (if not eaten and actually exist)
        for (int r = 0; r < rows; r += 2) {
            for (int c = 0; c < cols; c += 2) {
                if (grid[r][c] == 1 && eatenPellets.find({c, r}) == eatenPellets.end()) {
                    outputPixel(c, r, 255, 200, 0);
                }
            }
        }
        
        // Draw special pellets as 4x4 blocks with circular shape
        for (auto &p : specialPellets) {
            int tlx = p.first;
            int tly = p.second;
            for (int dr = 0; dr < 4; dr++) {
                for (int dc = 0; dc < 4; dc++) {
                    double dx = dc - 1.5;
                    double dy = dr - 1.5;
                    if (dx*dx + dy*dy <= 3.0) {
                        int px = tlx + dc;
                        int py = tly + dr;
                        if (px < cols && py < rows) {
                            outputPixel(px, py, 255, 0, 255);
                        }
                    }
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

    void eatPelletsAtPosition(int x, int y) {
        // Eat pellet at Pacman's current position (rounded to nearest pellet grid)
        int pelletX = (x + 1) / 2 * 2; // Round to nearest even coordinate
        int pelletY = (y + 1) / 2 * 2;
        
        if (pelletX >= 0 && pelletX < cols && pelletY >= 0 && pelletY < rows) {
            eatenPellets.insert({pelletX, pelletY});
        }
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
            eatPelletsAtPosition(pacmanX, pacmanY); // Eat pellets immediately after moving
            // Check for special pellet (within 4x4 area)
            for (auto it = specialPellets.begin(); it != specialPellets.end(); ) {
                int tlx = it->first;
                int tly = it->second;
                if (pacmanX >= tlx && pacmanX < tlx + 4 && pacmanY >= tly && pacmanY < tly + 4) {
                    it = specialPellets.erase(it);
                    speedBoostTimer = 10; // Speed boost for 10 updates
                } else {
                    ++it;
                }
            }
        }
    }

    bool isPositionOccupied(int x, int y, int excludeIndex = -1) {
        // Check if position is occupied by any ghost (except optionally excluded one)
        for (size_t i = 0; i < ghosts.size(); i++) {
            if ((int)i != excludeIndex && ghosts[i].x == x && ghosts[i].y == y) {
                return true;
            }
        }
        // Also check player ghost
        if (excludeIndex != -2 && playerGhost.x == x && playerGhost.y == y) {
            return true;
        }
        return false;
    }

    void moveGhosts() {
        for (size_t i = 0; i < ghosts.size(); i++) {
            auto &gh = ghosts[i];
            int dirs[4][2] = {{-1,0},{0,-1},{1,0},{0,1}}; // left, up, right, down
            
            // Calculate current distance to Pacman
            int currentDist = (gh.x - pacmanX)*(gh.x - pacmanX) + (gh.y - pacmanY)*(gh.y - pacmanY);
            
            std::vector<std::pair<int, int>> candidates; // pair<direction, score>
            for (int d = 0; d < 4; d++) {
                int nx = gh.x + dirs[d][0];
                int ny = gh.y + dirs[d][1];
                if (canMoveTo(nx, ny, ghostSize/2, true) && !isPositionOccupied(nx, ny, i)) {
                    // Calculate new distance to Pacman
                    int newDist = (nx - pacmanX)*(nx - pacmanX) + (ny - pacmanY)*(ny - pacmanY);
                    // Score based on distance reduction (higher is better)
                    int score = currentDist - newDist;
                    candidates.push_back({d, score});
                }
            }
            
            if (!candidates.empty()) {
                // Sort by score (best moves first)
                std::sort(candidates.begin(), candidates.end(), 
                         [](const std::pair<int,int>& a, const std::pair<int,int>& b) {
                             return a.second > b.second; // Higher score first
                         });
                
                // Choose direction with weighted probability favoring better moves
                int totalWeight = 0;
                std::vector<int> weights;
                for (size_t j = 0; j < candidates.size(); j++) {
                    int weight = candidates.size() - j; // Best move gets highest weight
                    weights.push_back(weight);
                    totalWeight += weight;
                }
                
                int randVal = rand() % totalWeight;
                int cumulative = 0;
                int chosenDir = candidates[0].first; // Default to best
                for (size_t j = 0; j < candidates.size(); j++) {
                    cumulative += weights[j];
                    if (randVal < cumulative) {
                        chosenDir = candidates[j].first;
                        break;
                    }
                }
                
                gh.x += dirs[chosenDir][0];
                gh.y += dirs[chosenDir][1];
                gh.dir = chosenDir;
            }
        }
        // Move player-controlled ghost
        int dirs[4][2] = {{-1,0},{0,-1},{1,0},{0,1}};
        int moveCount = (playerGhostSpeedBoostTimer > 0) ? 2 : 1; // Move twice during speed boost
        
        for (int move = 0; move < moveCount; move++) {
            int nx = playerGhost.x + dirs[playerGhostDir][0];
            int ny = playerGhost.y + dirs[playerGhostDir][1];
            if (canMoveTo(nx, ny, ghostSize/2, true) && !isPositionOccupied(nx, ny, -2)) {
                playerGhost.x = nx;
                playerGhost.y = ny;
                playerGhost.dir = playerGhostDir;
            } else {
                break; // Stop moving if we hit a wall or occupied space
            }
        }
        
        // Decrement speed boost timer
        if (playerGhostSpeedBoostTimer > 0) {
            playerGhostSpeedBoostTimer--;
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
                outputString("OVER", cols/2 - 4, rows/2+3);
                model->flushOverlayBuffer();
                return 5000;
            }
        }
        // check collision with player-controlled ghost
        if (checkCollision(pacmanX, pacmanY, pacRadius, playerGhost.x, playerGhost.y, ghostSize/2)) {
            GameOn = false;
            outputString("GAME", cols/2 - 4, rows/2-3);
            outputString("OVER", cols/2 - 4, rows/2+3);
            model->flushOverlayBuffer();
            return 5000;
        }

        // check win: all pellets eaten
        int pelletsLeft = 0;
        for (int r = 0; r < rows; r += 2) {
            for (int c = 0; c < cols; c += 2) {
                if (grid[r][c] == 1 && eatenPellets.find({c, r}) == eatenPellets.end()) {
                    pelletsLeft++;
                }
            }
        }
        if (pelletsLeft == 0 && specialPellets.empty()) {
            GameOn = false;
            outputString("YOU", cols/2 - 3, rows/2-3);
            outputString("WIN", cols/2 - 3, rows/2+3);
            model->flushOverlayBuffer();
            return 5000;
        }

        CopyToModel();
        int currentTimer = timer;
        if (speedBoostTimer > 0) {
            currentTimer /= 2; // Faster updates during speed boost
            speedBoostTimer--;
        }
        return currentTimer;
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
            int newDir = -1;
            if (button == "Left - Pressed") {
                newDir = 0;
            } else if (button == "Up - Pressed") {
                newDir = 1;
            } else if (button == "Right - Pressed") {
                newDir = 2;
            } else if (button == "Down - Pressed") {
                newDir = 3;
            }
            
            if (newDir != -1) {
                // Check if this direction matches current movement direction and no boost is active
                if (newDir == playerGhostDir && playerGhostSpeedBoostTimer == 0) {
                    playerGhostSpeedBoostTimer = 5; // Speed boost for 5 update cycles
                }
                playerGhostDir = newDir;
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
