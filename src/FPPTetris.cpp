#include <fpp-pch.h>

#include "FPPTetris.h"
#include "FPPArcadeSound.h"
#include <array>
#include <random>

#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"
#include "overlays/PixelOverlayEffects.h"


FPPTetris::FPPTetris(Json::Value &config) : FPPArcadeGame(config) {
    std::srand(time(NULL));
}
FPPTetris::~FPPTetris() {
}

using namespace ArcadeSound;

// Korobeiniki, the Russian folk song the Game Boy version made the Tetris
// theme, with a bass line under it. Both are eight bars, so they loop together.
static const char *THEME =
    "E5:4 B4:8 C5:8 D5:4 C5:8 B4:8 A4:4 A4:8 C5:8 E5:4 D5:8 C5:8 "
    "B4:4. C5:8 D5:4 E5:4 C5:4 A4:4 A4:4 -:4 "
    "D5:4. F5:8 A5:4 G5:8 F5:8 E5:4. C5:8 E5:4 D5:8 C5:8 "
    "B4:4 B4:8 C5:8 D5:4 E5:4 C5:4 A4:4 A4:4 -:4";
static std::string bassLine() {
    std::string out;
    for (const char *root : { "E2", "A2", "G#2", "A2", "D2", "C2", "E2", "A2" }) {
        std::string low = std::string(root);
        std::string high = low.substr(0, low.size() - 1) + "3";
        for (int x = 0; x < 4; x++) {
            out += low + ":8 " + high + ":8 ";
        }
    }
    return out;
}
static void startTheme() {
    Sound melody;
    melody.wave = Wave::Pulse;
    melody.notes = tune(THEME, 150);
    melody.loop = true;
    melody.level = 0.8f;
    Sound bass;
    bass.wave = Wave::Triangle;
    bass.notes = tune(bassLine(), 150);
    bass.loop = true;
    play(MUSIC, melody);
    play(BASS, bass);
}
static void stopTheme() {
    stop(MUSIC);
    stop(BASS);
}

static std::array<uint32_t, 7> COLORS = {
    0xFF0000,
    0x00FF00,
    0x0000FF,
    0xFF00FF,
    0xFFFF00,
    0x00FFFF,
    0xFFFFFF,
};

class Shape {
public:
    Shape() : row(0), col(0) {
        for (int x = 0; x < 16; x++) {
            data[x] = 0;
        }
        setRandomShape();
    }
    
    int row, col;
    
    
    uint8_t get(int x, int y) {
        return data[x * width + y];
    }
    void set(int x, int y, uint8_t d) {
        data[x * width + y] = d;
    }
    
    void rotate() {
        std::array<uint8_t, 16> d2(data);
        for (int x = 0; x < 16; x++) {
            data[x] = 0;
        }
        for (int x = 0; x < width; x++) {
            for (int y = 0, ny = width - 1; y < width; y++, ny--) {
                set(x, y, d2[ny * width + x]);
            }
        }
    }
    
    int getWidth() { return width;}

    uint32_t getColor() {
        return COLORS[type];
    }
private:
    void setRandomShape() {
        type = std::rand() % 7;
        switch (type) {
            case 0:
                width = 3;  // s shape
                set(1, 0, 1);
                set(2, 0, 1);
                set(0, 1, 1);
                set(1, 1, 1);
                break;
            case 1:
                width = 3;  // z shape
                set(0, 0, 1);
                set(1, 0, 1);
                set(1, 1, 1);
                set(2, 1, 1);
                break;
            case 2:
                width = 3;  // L shape 1
                set(0, 0, 1);
                set(1, 0, 1);
                set(1, 1, 1);
                set(1, 2, 1);
                break;
            case 3:
                width = 3;  // L shape 2
                set(1, 0, 1);
                set(0, 0, 1);
                set(0, 1, 1);
                set(0, 2, 1);
                break;
            case 4:
                width = 2;  // sq shape
                set(0, 0, 1);
                set(0, 1, 1);
                set(1, 0, 1);
                set(1, 1, 1);
                break;
            case 5:
                width = 3;  // T shape
                set(1, 0, 1);
                set(0, 1, 1);
                set(1, 1, 1);
                set(2, 1, 1);
                break;
            case 6: // I shape
            default:
                type = 6;
                width = 4;
                set(1, 0, 1);
                set(1, 1, 1);
                set(1, 2, 1);
                set(1, 3, 1);
                break;
        };
    }
    
    int width = 4;
    int type = 0;
    std::array<uint8_t, 16> data;
};




class TetrisEffect : public FPPArcadeGameEffect {
public:
    TetrisEffect(int r, int c, int offx, int offy, int sc, PixelOverlayModel *m) : FPPArcadeGameEffect(m), rows(r), cols(c) {
        table.resize(r);
        for (int x = 0; x < r; x++) {
            table[x].resize(c);
        }
        scale = sc;
        offsetX = offx;
        offsetY = offy;
        newShape();
        CopyToModel();
        startTheme();
    }
    ~TetrisEffect() {
        stopTheme();
        if (currentShape) {
            delete currentShape;
        }
    }
    const std::string &name() const override {
        static std::string NAME = "Tetris";
        return NAME;
    }

    void newShape() {
        if (currentShape) {
            delete currentShape;
        }
        currentShape = new Shape();
        currentShape->col = cols / 2 - 1;
        currentShape->row = 0;
        if (!CheckPosition(currentShape)) {
            GameOn = false;
            stopTheme();
            play(FX1, Wave::Square, tune("E5:8 C5:8 A4:8 E4:8 C4:8 A3:2", 160));
        }
    }
    bool CheckPosition(Shape *shape) {
        for (int i = 0; i < shape->getWidth(); i++) {
            for(int j = 0; j < shape->getWidth(); j++) {
                if ((shape->col+j < 0 || shape->col+j >= cols || shape->row+i >= rows)){ //Out of borders
                    if (shape->get(i, j)) {
                        return false;
                    }
                } else if (table[shape->row+i][shape->col+j] && shape->get(i, j)) {
                    return false;
                }
            }
        }
        return true;
    }
    void WriteToTable(){
        for (int i = 0; i < currentShape->getWidth(); i++) {
            for (int j = 0; j < currentShape->getWidth(); j++) {
                if (currentShape->get(i, j)) {
                    table[currentShape->row+i][currentShape->col+j] = currentShape->getColor();
                }
            }
        }
    }
    
    void CheckLines() {
        int before = score;
        for(int i = 0; i < rows; i++) {
            int sum = 0;
            for(int j = 0; j < cols; j++) {
                if (table[i][j]) {
                    sum++;
                }
            }
            if (sum == cols){
                score++;

                for (int k = i; k >= 1; k--) {
                    for (int l = 0; l < cols; l++) {
                        table[k][l] = table[k-1][l];
                    }
                }
                for (int l = 0; l < cols; l++) {
                    table[0][l] = 0;
                }
            }
        }
        timer -= 10;
        if (timer < 30) {
            timer = 30;
        }
        int lines = score - before;
        if (lines >= 4) {
            play(FX1, Wave::Square, tune("C5:16 E5:16 G5:16 C6:16 E6:16 G6:16 C7:8", 200));
        } else if (lines > 0) {
            play(FX1, Wave::Square, { slide(523, 1046, 90 + 60 * lines) });
        } else {
            play(FX2, Wave::Noise, { tone(700, 40) });
        }
        // The theme speeds up as the drop does, up to about half again.
        float speed = 1.0f + 0.6f * (500 - timer) / 470.0f;
        setTempo(MUSIC, speed);
        setTempo(BASS, speed);
    }
    
    void CopyToModel() {
        model->clearOverlayBuffer();
        if (offsetX) {
            for (int y = 0; y < rows*scale; y++) {
                model->setOverlayPixelValue(offsetX-1, offsetY + y, 128, 128, 128);
                model->setOverlayPixelValue(offsetX+cols*scale, offsetY + y, 128, 128, 128);
            }
            for (int x = -1; x <= cols*scale; x++) {
                model->setOverlayPixelValue(offsetX+x, offsetY + rows*scale, 128, 128, 128);
                if (offsetY) {
                    model->setOverlayPixelValue(offsetX+x, offsetY - 1, 128, 128, 128);
                }
            }
        }
        for(int i = 0; i < rows; i++) {
            for(int j = 0; j < cols; j++) {
                if (table[i][j]) {
                    int r = (table[i][j] >> 16) & 0xFF;
                    int g = (table[i][j] >> 8) & 0xFF;
                    int b = (table[i][j]) & 0xFF;
                    outputPixel(j, i, r, g, b);
                }
            }
        }
        if (currentShape) {
            uint32_t color = currentShape->getColor();
            int r = (color >> 16) & 0xFF;
            int g = (color >> 8) & 0xFF;
            int b = (color) & 0xFF;

            for (int i = 0; i < currentShape->getWidth(); i++) {
                for (int j = 0; j < currentShape->getWidth(); j++) {
                    if (currentShape->get(i, j)) {
                        outputPixel(currentShape->col+j, currentShape->row+i, r, g, b);
                    }
                }
            }

        }
        model->flushOverlayBuffer();
    }
    
    virtual int32_t update() override {
        if (!GameOn) {
            if (currentShape) {
                delete currentShape;
                currentShape = nullptr;
                model->clearOverlayBuffer();
                outputLetter(0, 0, 'G');
                outputLetter(4, 0, 'A');
                outputLetter(8, 0, 'M');
                outputLetter(12, 0, 'E');
                outputLetter(0, 6, 'O');
                outputLetter(4, 6, 'V');
                outputLetter(8, 6, 'E');
                outputLetter(12, 6, 'R');

                char buf[20];
                snprintf(buf, sizeof(buf), "%d", score);
                int x = 4;
                if (score < 10) {
                    x = 6;
                }
                char *b2 = buf;
                while (*b2) {
                    outputLetter(x, 13, *b2);
                    x += 4;
                    b2++;
                }

                model->flushOverlayBuffer();
                return 3000;
            }
            model->clearOverlayBuffer();
            model->flushOverlayBuffer();
            
            if (WaitingUntilOutput) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
                return 0;
            }
            WaitingUntilOutput = true;
            return -1;
        }
        button("Down - Pressed");
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
        if (!GameOn) {
            return;
        }
        Shape tmp(*currentShape);
        if (button == "Left - Pressed") {
            tmp.col--;  //move left
            if (CheckPosition(&tmp)) {
                currentShape->col--;
                play(FX2, Wave::Pulse, { tone(1200, 12) }, 0.5f);
            }
        } else if (button == "Right - Pressed") {
            tmp.col++;  //move left
            if (CheckPosition(&tmp)) {
                currentShape->col++;
                play(FX2, Wave::Pulse, { tone(1200, 12) }, 0.5f);
            }
        } else if (button == "Up - Pressed") {
            tmp.rotate();
            if (CheckPosition(&tmp)) {
                currentShape->rotate();
                play(FX2, Wave::Square, { slide(700, 1000, 30) }, 0.6f);
            }
        } else if (button == "Down - Pressed") {
            tmp.row++;  //move down
            if (CheckPosition(&tmp)) {
                currentShape->row++;
            } else {
                WriteToTable();
                CheckLines(); //check full lines, after putting it down
                newShape();
            }
        //} else {
            //printf("Unknown -%s-\n", button.c_str());
        }
        CopyToModel();
    }
    
    

    int rows = 20;
    int cols = 11;
    std::vector<std::vector<uint32_t>> table;
    int score = 0;
    bool GameOn = true;
    bool WaitingUntilOutput = false;
    
    Shape *currentShape = nullptr;
    long long timer = 500; //half second

};

const std::string &FPPTetris::getName() {
    static const std::string name = "Tetris";
    return name;
}


void FPPTetris::button(const std::string &button) {
    PixelOverlayModel *m = PixelOverlayManager::INSTANCE.getModel(modelName);
    if (m != nullptr) {
        TetrisEffect *effect = dynamic_cast<TetrisEffect*>(m->getRunningEffect());
        if (!effect) {
            if (findOption("overlay", "Overwrite") == "Transparent") {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::TransparentRGB));
            } else {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
            }
            int pixelScaling = std::stoi(findOption("Pixel Scaling", "1"));
            int rows = std::stoi(findOption("Rows", "20"));
            int cols = std::stoi(findOption("Colums", "11"));
            int offsetX = (m->getWidth() - (cols * pixelScaling)) / 2;
            if (offsetX < 0) {
                offsetX = 0;
            }
            int offsetY = (m->getHeight() - (rows * pixelScaling)) / 2;
            if (offsetY < 0) {
                offsetY = 0;
            }
            effect = new TetrisEffect(rows, cols, offsetX, offsetY, pixelScaling, m);
            effect->button(button);
            m->setRunningEffect(effect, 50);
        } else {
            effect->button(button);
        }
    }
}
