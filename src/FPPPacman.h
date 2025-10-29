#ifndef __FPPARCADE_PACMAN_
#define __FPPARCADE_PACMAN_

#include "FPPArcade.h"


class FPPPacman : public FPPArcadeGame {
public:
    FPPPacman(Json::Value &config);
    virtual ~FPPPacman();

    virtual void button(const std::string &button) override;
    virtual const std::string &getName() override;
};

#endif



