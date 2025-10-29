#pragma once

#include <fpp-pch.h>

class FPPPacman : public FPPArcadeGame {
public:
    FPPPacman(Json::Value &config);
    virtual ~FPPPacman();

    virtual void button(const std::string &button) override;
    virtual const std::string &getName() override;
};
