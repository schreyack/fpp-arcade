#include <fpp-pch.h>

#include "FPPArcadeMenu.h"

#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"

static const int TITLE_R = 255, TITLE_G = 96, TITLE_B = 0;
static const int PICK_R = 255, PICK_G = 220, PICK_B = 0;
static const int OTHER_R = 110, OTHER_G = 110, OTHER_B = 110;

FPPArcadeMenu::FPPArcadeMenu(PixelOverlayModel *m, std::vector<Item> g, std::vector<Item> s) :
    FPPArcadeGameEffect(m), games(std::move(g)), songs(std::move(s)),
    lastInput(std::chrono::steady_clock::now()) {
    // With only one kind of thing to pick, skip the PLAY / DANCE question.
    if (songs.empty()) {
        screen = GAMES;
    } else if (games.empty()) {
        screen = SONGS;
    }
}

const std::string &FPPArcadeMenu::name() const {
    static std::string NAME = "Arcade Menu";
    return NAME;
}

const std::vector<FPPArcadeMenu::Item> &FPPArcadeMenu::itemsFor(Screen s) const {
    return s == SONGS ? songs : games;
}

std::optional<FPPArcadeMenu::Item> FPPArcadeMenu::press(const std::string &button) {
    lastInput = std::chrono::steady_clock::now();
    if (screen == READY) {
        // Any press at all starts the waiting game.
        closing = true;
        return ready;
    }
    int count = screen == TOP ? 2 : (int)itemsFor(screen).size();
    if (button == "Up") {
        selected = std::max(0, selected - 1);
    } else if (button == "Down") {
        selected = std::min(count - 1, selected + 1);
    } else if (button == "Select") {
        if (screen != TOP && !games.empty() && !songs.empty()) {
            screen = TOP;
            selected = 0;
        }
    } else if (button == "Start" || button == "Fire") {
        if (screen == TOP) {
            screen = selected == 0 ? GAMES : SONGS;
            selected = 0;
        } else if (selected < count) {
            const Item &item = itemsFor(screen)[selected];
            if (item.game) {
                ready = item;
                screen = READY;
                return std::nullopt;
            }
            closing = true;
            return item;
        }
    }
    return std::nullopt;
}

int32_t FPPArcadeMenu::update() {
    auto idle = std::chrono::steady_clock::now() - lastInput;
    if (closing || idle > std::chrono::seconds(MENU_IDLE_SECONDS)) {
        closeNow();
        return 0;
    }
    model->clearOverlayBuffer();
    if (screen == TOP) {
        drawTop();
    } else if (screen == READY) {
        drawReady();
    } else if (screen == GAMES) {
        drawList("PLAY", games);
    } else {
        drawList("DANCE", songs);
    }
    model->flushOverlayBuffer();
    return 100;
}

void FPPArcadeMenu::drawTop() {
    // Double size: a 60x30 matrix is 30x15 units at this scale, 3x5 letters
    // on a 4 unit pitch.
    int cols, rows;
    model->getSize(cols, rows);
    const int scl = 2;
    int w = cols / scl;
    int h = rows / scl;
    int x = std::max(4, (w - 19) / 2);  // "DANCE" is the widest at 19 units
    const char *labels[2] = { "PLAY", "DANCE" };
    int ys[2] = { h / 2 - 6, h / 2 + 1 };
    for (int i = 0; i < 2; i++) {
        bool pick = i == selected;
        if (pick) {
            outputLetter(x - 4, ys[i], '>', PICK_R, PICK_G, PICK_B, scl);
        }
        outputString(labels[i], x, ys[i],
                     pick ? PICK_R : OTHER_R, pick ? PICK_G : OTHER_G, pick ? PICK_B : OTHER_B, scl);
    }
}

void FPPArcadeMenu::drawList(const std::string &title, const std::vector<Item> &items) {
    int cols, rows;
    model->getSize(cols, rows);
    outputString(title, (cols - ((int)title.size() * 4 - 1)) / 2, 0, TITLE_R, TITLE_G, TITLE_B, 1);

    // One title line, then as many 6 pixel lines as fit, scrolling to keep
    // the highlighted item on screen.
    int visible = std::max(1, (rows - 6) / 6);
    int first = std::max(0, selected - visible + 1);
    size_t maxChars = std::max(1, (cols - 5) / 4);
    for (int i = first; i < (int)items.size() && i < first + visible; i++) {
        int y = 6 + (i - first) * 6;
        bool pick = i == selected;
        if (pick) {
            outputLetter(1, y, '>', PICK_R, PICK_G, PICK_B, 1);
        }
        outputString(items[i].label.substr(0, maxChars), 5, y,
                     pick ? PICK_R : OTHER_R, pick ? PICK_G : OTHER_G, pick ? PICK_B : OTHER_B, 1);
    }
}

void FPPArcadeMenu::drawReady() {
    int cols, rows;
    model->getSize(cols, rows);
    auto centered = [cols](const std::string &s) { return std::max(0, (cols - ((int)s.size() * 4 - 1)) / 2); };
    outputString(ready.label, centered(ready.label), rows / 2 - 11, TITLE_R, TITLE_G, TITLE_B, 1);
    // Blink the prompt so it reads as something to act on.
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if ((ms / 600) % 2 == 0) {
        outputString("PRESS A BUTTON", centered("PRESS A BUTTON"), rows / 2 - 2, PICK_R, PICK_G, PICK_B, 1);
        outputString("TO START", centered("TO START"), rows / 2 + 4, PICK_R, PICK_G, PICK_B, 1);
    }
}

void FPPArcadeMenu::closeNow() {
    model->clearOverlayBuffer();
    model->flushOverlayBuffer();
    model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
}
