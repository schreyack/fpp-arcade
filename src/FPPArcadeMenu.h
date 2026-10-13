#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "FPPArcade.h"

// The pick-a-game / pick-a-song menu drawn on a game model.
//
// Screen one offers PLAY and DANCE; PLAY lists the model's games and DANCE
// lists the "Dance - <name>" playlists. Up/Down move the highlight, Start or
// Fire picks, Select goes back a screen. A picked game waits on a "press a
// button to start" screen so it does not start until the player is ready.
// The menu closes itself after MENU_IDLE_SECONDS without input, handing the
// model back to the show.
class FPPArcadeMenu : public FPPArcadeGameEffect {
public:
    struct Item {
        std::string label;
        int game = 0;          // game idx to start, or 0
        std::string playlist;  // playlist to play, or ""
    };

    static constexpr int MENU_IDLE_SECONDS = 20;

    FPPArcadeMenu(PixelOverlayModel *m, std::vector<Item> games, std::vector<Item> songs);

    const std::string &name() const override;
    int32_t update() override;

    // Feed one press ("Up", "Down", "Start", "Fire", "Select"). Returns the
    // item picked, if this press picked one; the menu then closes itself.
    std::optional<Item> press(const std::string &button);
    // Close at the next update, picking nothing.
    void cancel() { closing = true; }

private:
    enum Screen { TOP, GAMES, SONGS, READY };

    const std::vector<Item> &itemsFor(Screen s) const;
    void drawTop();
    void drawList(const std::string &title, const std::vector<Item> &items);
    void drawReady();
    void closeNow();

    std::vector<Item> games;
    std::vector<Item> songs;
    Screen screen = TOP;
    int selected = 0;
    Item ready;  // the game waiting on the READY screen
    bool closing = false;
    std::chrono::steady_clock::time_point lastInput;
};
