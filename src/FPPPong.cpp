#include <fpp-pch.h>

#include "FPPPong.h"
#include "FPPArcadeSound.h"
#include <array>
#include <cmath>

#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"
#include "overlays/PixelOverlayEffects.h"


FPPPong::FPPPong(Json::Value &config) : FPPArcadeGame(config) {
}
FPPPong::~FPPPong() {
}

using namespace ArcadeSound;

// The 1972 arcade cabinet's three sounds: a blip off a paddle, a lower and
// shorter one off the top or bottom wall, and a long low tone for a point.
static void paddleSound() { play(FX1, Wave::Square, { tone(490, 40) }); }
static void wallSound() { play(FX2, Wave::Square, { tone(245, 25) }); }
static void pointSound() { play(FX1, Wave::Square, { tone(245, 260) }); }

class PongEffect : public FPPArcadeGameEffect {
public:
    PongEffect(int sc, int c, PixelOverlayModel *m) : FPPArcadeGameEffect(m), controls(c) {
        m->getSize(cols, rows);
        scale = sc;
        cols /= sc;
        rows /= sc;
        
        int initialSize = rows / 5;
        if (initialSize < 3) {
            initialSize = 3;
        }
        initialRacketSize = initialSize;
        racketP1Size = racketP2Size = initialSize;
        racketP1Pos = racketP2Pos = (rows - initialSize)/2;
        offsetX = 0;
        offsetY = 0;
        
        serve(1);
    }
    ~PongEffect() {
    }
    
    
    void CopyToModel() {
        model->clearOverlayBuffer();
        char buf[25];
        snprintf(buf, sizeof(buf), "%d:%d", p1Score, p2Score);
        int len = strlen(buf);
        float scl = scale;
        
        while (((model->getHeight() / scl) < 40) && scl > 1) {
            scl *= 0.80;
        }
        if (scl < 1) {
            scl = 1;
        }
        
        outputString(buf, (model->getWidth()/2 - len*2) / scl, 0, 128, 128, 128, scl);
        
        for (int y = 0; y < racketP1Size; y++) {
            outputPixel(0, racketP1Pos + y, 255, 255, 255);
        }
        for (int y = 0; y < racketP2Size; y++) {
            outputPixel(cols-1, racketP2Pos + y, 255, 255, 255);
        }
        outputPixel(ballPosX, ballPosY, 255, 255, 255);
        
    }
    const std::string &name() const override {
        static std::string NAME = "Pong";
        return NAME;
    }

    // From the middle toward 'dir' (-1 left, 1 right), at an angle, at the
    // starting speed.
    void serve(int dir) {
        ballPosX = cols / 2;
        ballPosY = rows / 2;
        ballSpeed = 1;
        ballDirX = dir;
        float angle = 0.25f + (std::rand() % 30) / 100.0f;
        ballDirY = std::rand() % 2 ? angle : -angle;
        vec2_norm(ballDirX, ballDirY);
    }

    // The computer plays the right paddle until a second player moves it:
    // it follows the ball coming its way, aiming a little off so it can be
    // beaten, and a touch slower than the ball at full speed.
    void computerPaddle() {
        float center = racketP2Pos + racketP2Size / 2.0f;
        float target = ballDirX > 0 ? ballPosY + aiAim : rows / 2.0f;
        racketP2Speed = 0;
        if (aiTick++ % 4 == 3) {
            return;
        }
        if (target < center - 1) {
            racketP2Speed = -1;
        } else if (target > center + 1) {
            racketP2Speed = 1;
        }
    }

    // Each return speeds the ball up a little, as in the arcade game.
    void returned() {
        paddleSound();
        ballSpeed = std::min(1.8f, ballSpeed + 0.1f);
    }

    virtual int32_t update() override {
        if (GameOn) {
            if (!p2Human) {
                computerPaddle();
            }
            moveRackets();
            moveBall();
        }
        CopyToModel();
        if (!GameOn) {
            model->clearOverlayBuffer();
            model->flushOverlayBuffer();
            
            if (WaitingUntilOutput) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
                return 0;
            }
            WaitingUntilOutput = true;
            return -1;
        }
        if (p1Score >= 5 || p2Score >= 5) {
            GameOn = false;
            play(FX1, Wave::Square, { tone(245, 260), rest(140), tone(245, 260), rest(140), tone(245, 500) });
            outputString("GAME", (cols-15)/ 2, rows/2-6);
            outputString("OVER", (cols-15)/ 2, rows/2);
            model->flushOverlayBuffer();
            return 2000;
        }
        model->flushOverlayBuffer();
        return timer;
    }
    
    void moveRackets() {
        racketP1Pos += racketP1Speed;
        if (racketP1Pos < 0) {
            racketP1Pos = 0;
        } else if ((racketP1Pos + racketP1Size) > rows) {
            racketP1Pos = rows - racketP1Size;
        }
        racketP2Pos += racketP2Speed;
        if (racketP2Pos < 0) {
            racketP2Pos = 0;
        } else if ((racketP2Pos + racketP2Size) > rows) {
            racketP2Pos = rows - racketP2Size;
        }
    }
    void moveBall() {
        ballPosX += ballDirX * ballSpeed;
        ballPosY += ballDirY * ballSpeed;
        
         // hit by left racket?
         if (ballPosX <= 1 &&
             ballPosY <= (racketP1Pos + racketP1Size) &&
             ballPosY >= racketP1Pos) {
             // set fly direction depending on where it hit the racket
             // (t is 0.5 if hit at top, 0 at center, -0.5 at bottom)
             float t = ((ballPosY - racketP1Pos) / racketP1Size) - 0.5f;
             ballDirX = std::fabs(ballDirX);
             ballDirY = t;
             returned();
             // Where the computer will aim for this one.
             aiAim = (std::rand() % (racketP2Size + 1)) - racketP2Size / 2.0f;
         }         // hit by right racket?
         if (ballPosX >= (cols-2) &&
             ballPosY <= (racketP2Pos + racketP2Size) &&
             ballPosY >= racketP2Pos) {
             // set fly direction depending on where it hit the racket
             // (t is 0.5 if hit at top, 0 at center, -0.5 at bottom)
             float t = ((ballPosY - racketP2Pos) / racketP2Size) - 0.5f;
             ballDirX = -std::fabs(ballDirX);
             ballDirY = t;
             returned();
         }

         if (ballPosX < 0) {
             //left wall
             ++p2Score;
             pointSound();
             serve(-1);  // toward the player who missed
         }

         // hit right wall?
         if (ballPosX >= cols) {
             //right wall
             ++p1Score;
             pointSound();
             serve(1);
         }

         if (ballPosY >= rows) {
             //hit bottom
             ballDirY = -std::fabs(ballDirY);
             ballPosY = rows-1;
             wallSound();
         }
         if (ballPosY < 0) {
             //hit top
             ballDirY = std::fabs(ballDirY);
             ballPosY = 0;
             wallSound();
         }

         // make sure that length of dir stays at 1
         vec2_norm(ballDirX, ballDirY);
    }
    void vec2_norm(float& x, float &y) {
        // sets a vectors length to 1 (which means that x + y == 1)
        float length = std::sqrt((x * x) + (y * y));
        if (length != 0.0f) {
            length = 1.0f / length;
            x *= length;
            y *= length;
        }
    }
    
    void button(const std::string &butt) {
        std::string button = butt;
        std::string joystickName = "";
        size_t pos = butt.find('|');
        if (pos != std::string::npos) {
            button = butt.substr(0, pos);
            joystickName = butt.substr(pos+1);
        }
        if (controls == 4){
            if (joystickName.ends_with("2")){
                if (button == "Up - Pressed") {
                    racketP2Speed = -1; p2Human = true;
                } else if (button == "Down - Pressed") {
                    racketP2Speed = 1; p2Human = true;
                } else if (button == "Down - Released" || button == "Up - Released") {
                    racketP2Speed = 0; p2Human = true;
                }
            } else {
                if (button == "Up - Pressed") {
                    racketP1Speed = -1;
                } else if (button == "Down - Pressed") {
                    racketP1Speed = 1;
                } else if (button == "Down - Released" || button == "Up - Released") {
                    racketP1Speed = 0;
                }
            }
        } else if (controls == 2) {
            if (button == "Up/Right - Pressed") {
                racketP2Speed = -1; p2Human = true;
            } else if (button == "Down/Right - Pressed") {
                racketP2Speed = 1; p2Human = true;
            } else if (button == "Up/Right - Released" || button == "Down/Right - Released") {
                racketP2Speed = 0; p2Human = true;
            } else if (button == "Up/Left - Pressed") {
                racketP1Speed = -1;
            } else if (button == "Down/Left - Pressed") {
                racketP1Speed = 1;
            } else if (button == "Down/Left - Released" || button == "Up/Left - Released") {
                racketP1Speed = 0;
            }
        } else if (controls == 3) {
            if (button == "Right - Pressed") {
                racketP2Speed = -1; p2Human = true;
            } else if (button == "Down - Pressed") {
                racketP2Speed = 1; p2Human = true;
            } else if (button == "Right - Released" || button == "Down - Released") {
                racketP2Speed = 0; p2Human = true;
            } else if (button == "Up - Pressed") {
                racketP1Speed = -1;
            } else if (button == "Left - Pressed") {
                racketP1Speed = 1;
            } else if (button == "Left - Released" || button == "Up - Released") {
                racketP1Speed = 0;
            }
        } else {
            if (button == "Left - Pressed") {
                racketP2Speed = -1; p2Human = true;
            } else if (button == "Right - Pressed") {
                racketP2Speed = 1; p2Human = true;
            } else if (button == "Right - Released" || button == "Left - Released") {
                racketP2Speed = 0; p2Human = true;
            } else if (button == "Up - Pressed") {
                racketP1Speed = -1;
            } else if (button == "Down - Pressed") {
                racketP1Speed = 1;
            } else if (button == "Down - Released" || button == "Up - Released") {
                racketP1Speed = 0;
            }
        }
    }
    
    int controls;

    int rows = 20;
    int cols = 20;
    
    int p1Score = 0;
    int p2Score = 0;
    
    int initialRacketSize;
    int racketP1Size;
    int racketP2Size;
    int racketP1Pos;
    int racketP1Speed = 0;
    int racketP2Pos;
    int racketP2Speed = 0;
    
    float ballPosX = 0;
    float ballPosY = 0;
    float ballDirX = 1;
    float ballDirY = 0;
    float ballSpeed = 1;
    bool p2Human = false;   // a second player has moved the right paddle
    float aiAim = 0;
    int aiTick = 0;

    
    bool GameOn = true;
    bool WaitingUntilOutput = false;
    
    long long timer = 50;
};

const std::string &FPPPong::getName() {
    static const std::string name = "Pong";
    return name;
}


void FPPPong::button(const std::string &button) {
    PixelOverlayModel *m = PixelOverlayManager::INSTANCE.getModel(modelName);
    if (m != nullptr) {
        PongEffect *effect = dynamic_cast<PongEffect*>(m->getRunningEffect());
        if (!effect) {
            if (findOption("overlay", "Overwrite") == "Transparent") {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::TransparentRGB));
            } else {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
            }
            int controls = std::stoi(findOption("Controls", "1"));
            int pixelScaling = std::stoi(findOption("Pixel Scaling", "1"));
            effect = new PongEffect(pixelScaling, controls, m);
            effect->button(button);
            m->setRunningEffect(effect, 50);
        } else {
            effect->button(button);
        }
    }
}
