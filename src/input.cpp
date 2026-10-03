// input.cpp
#include "input.h"

#include "raylib.h"

namespace {

struct MoveKey {
    int       key;
    Direction dir;
};
const MoveKey MOVE_KEYS[4] = {
    {KEY_W, Direction::Up}, {KEY_S, Direction::Down}, {KEY_A, Direction::Left}, {KEY_D, Direction::Right}};

constexpr double HOLD_DELAY = 0.18;  // 이 시간 이상 누르고 있으면 연속 이동
int    gHeld      = -1;              // 누르고 있는 키 번호 (MOVE_KEYS 인덱스)
double gPressTime = 0.0;

}  // namespace

std::optional<Direction> ReadMoveInput() {
    for (int i = 0; i < 4; ++i) {
        if (IsKeyPressed(MOVE_KEYS[i].key)) {
            gHeld      = i;
            gPressTime = GetTime();
            return MOVE_KEYS[i].dir;
        }
    }
    if (gHeld >= 0) {
        if (IsKeyDown(MOVE_KEYS[gHeld].key)) {
            if (GetTime() - gPressTime >= HOLD_DELAY) return MOVE_KEYS[gHeld].dir;
        } else {
            gHeld = -1;
        }
    }
    return std::nullopt;
}

GameCommand ReadGameCommand() {
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) return GameCommand::Pause;
    if (IsKeyPressed(KEY_F5)) return GameCommand::Save;
    if (IsKeyPressed(KEY_H)) return GameCommand::ToggleGuide;
    if (IsKeyPressed(KEY_T)) return GameCommand::ToggleTrail;
    if (IsKeyPressed(KEY_F)) return GameCommand::ToggleBreak;
    return GameCommand::None;
}

MenuInput ReadMenuInput() {
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) return MenuInput::Up;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) return MenuInput::Down;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE)) return MenuInput::Confirm;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) return MenuInput::Back;
    return MenuInput::None;
}
