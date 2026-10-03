// logic.h
// 게임 로직 모듈: 이동, 충돌, 열쇠/문/함정/출구, 제한 시간, 기록 갱신, 스냅샷
// Raylib에 의존하지 않는다.
#pragma once

#include "types.h"

GridPos    DirToDelta(Direction dir);
void       StartLevel(Game& game, const Map& levelMap, GameMode mode, float timeLimit);
MoveResult TryMove(Game& game, Direction dir);
bool       IsInside(const Map& map, GridPos p);
bool       IsBorder(const Map& map, GridPos p);
BreakToggle ToggleBreakMode(Player& player);
bool       CanEnter(const Map& map, const Player& player, GridPos p);
void       ApplyTileEffect(Game& game, GridPos p, MoveResult& result);
bool       UpdateTimer(GameSession& session, float dt);
bool       UpdateBestRecord(std::map<std::string, int>& records, const std::string& mazeName, int moves);
SaveData   MakeSnapshot(const Game& game);
bool       RestoreSnapshot(const SaveData& save, Game& game);
