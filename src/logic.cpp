// logic.cpp
#include "logic.h"

namespace {

void AddTrail(Trail& trail, const Map& map, GridPos p) {
    trail.order.push_back(p);
    if ((int)trail.visited.size() != map.CellCount()) trail.visited.assign(map.CellCount(), false);
    trail.visited[map.Index(p)] = true;
}

}  // namespace

GridPos DirToDelta(Direction dir) {
    switch (dir) {
        case Direction::Up:    return {0, -1};
        case Direction::Down:  return {0, 1};
        case Direction::Left:  return {-1, 0};
        case Direction::Right: return {1, 0};
    }
    return {0, 0};
}

void StartLevel(Game& game, const Map& levelMap, GameMode mode, float timeLimit) {
    game.map    = levelMap;
    game.player = Player{};
    game.player.pos = levelMap.start;

    game.trail.order.clear();
    game.trail.visited.assign(levelMap.CellCount(), false);
    AddTrail(game.trail, game.map, game.player.pos);

    GameSession& s  = game.session;
    s.mode          = mode;
    s.timeLimit     = timeLimit;
    s.timeRemaining = timeLimit;
    s.elapsed       = 0.0f;
    s.newRecord     = false;
    s.showGuide     = false;
    s.guidePath.clear();
    s.message.clear();
    s.messageTimer  = 0.0f;
    s.menuIndex     = 0;
    s.resumedFromSave = false;
    s.screen        = Screen::Playing;
}

bool IsInside(const Map& map, GridPos p) {
    return p.x >= 0 && p.x < map.width && p.y >= 0 && p.y < map.height;
}

bool IsBorder(const Map& map, GridPos p) {
    return p.x == 0 || p.y == 0 || p.x == map.width - 1 || p.y == map.height - 1;
}

BreakToggle ToggleBreakMode(Player& player) {
    if (player.breakUsed) return BreakToggle::AlreadyUsed;
    player.breakArmed = !player.breakArmed;
    return player.breakArmed ? BreakToggle::Armed : BreakToggle::Disarmed;
}

bool CanEnter(const Map& map, const Player& player, GridPos p) {
    if (!IsInside(map, p)) return false;
    const Tile& t = map.At(p);
    if (t.type == TileType::Wall) return false;
    if (t.type == TileType::Door) return t.id >= 0 && t.id < MAX_KEYS && player.keys.test(t.id);
    return true;
}

MoveResult TryMove(Game& game, Direction dir) {
    MoveResult r;
    r.dir  = dir;
    r.from = r.to = game.player.pos;

    GridPos d = DirToDelta(dir);
    GridPos target{game.player.pos.x + d.x, game.player.pos.y + d.y};

    // 1) 범위 검사를 가장 먼저 수행하여 배열 밖 접근을 차단한다.
    if (!IsInside(game.map, target)) {
        r.events.push_back(GameEvent::BumpWall);
        return r;
    }
    // 2) 벽: 부수기 대기 상태이면 부수고, 아니면 충돌
    Tile& targetTile = game.map.At(target);
    if (targetTile.type == TileType::Wall) {
        r.breakPos = target;
        if (game.player.breakArmed) {
            if (IsBorder(game.map, target)) {          // 바깥 테두리는 부술 수 없음 (대기 유지)
                r.events.push_back(GameEvent::BreakBlocked);
                return r;
            }
            targetTile = Tile{TileType::Path, -1};      // 벽을 길로 바꿈 (플레이어는 제자리)
            game.player.breakArmed = false;
            game.player.breakUsed  = true;
            game.player.brokenAt   = target;
            r.brokeWall = true;
            r.events.push_back(GameEvent::WallBroken);
            return r;
        }
        r.events.push_back(GameEvent::BumpWall);
        return r;
    }
    // 3) 잠긴 문인데 열쇠가 없는 경우 (문은 부술 수 없음)
    if (!CanEnter(game.map, game.player, target)) {
        r.events.push_back(GameEvent::DoorLocked);
        r.keyId = game.map.At(target).id;
        return r;
    }
    // 4) 이동
    game.player.pos = target;
    game.player.moveCount++;
    AddTrail(game.trail, game.map, target);
    r.moved = true;
    r.events.push_back(GameEvent::Moved);

    // 5) 도착한 칸의 효과
    ApplyTileEffect(game, target, r);
    r.to = game.player.pos;
    return r;
}

void ApplyTileEffect(Game& game, GridPos p, MoveResult& r) {
    Tile& tile = game.map.At(p);
    switch (tile.type) {
        case TileType::Key:
            if (tile.id >= 0 && tile.id < MAX_KEYS) game.player.keys.set(tile.id);
            r.keyId   = tile.id;
            tile.type = TileType::Path;
            tile.id   = -1;
            r.events.push_back(GameEvent::KeyPicked);
            break;
        case TileType::Door:
            // 열쇠는 유지한다. (같은 번호의 문이 여러 개여도 열 수 있음)
            r.keyId   = tile.id;
            tile.type = TileType::Path;
            tile.id   = -1;
            r.events.push_back(GameEvent::DoorOpened);
            break;
        case TileType::TrapReset:
            game.player.pos = game.map.start;
            AddTrail(game.trail, game.map, game.map.start);
            r.teleported = true;
            r.events.push_back(GameEvent::TrapTriggered);
            break;
        case TileType::TrapPenalty:
            game.player.moveCount += game.map.trapPenalty;
            r.events.push_back(GameEvent::TrapTriggered);
            break;
        case TileType::Exit:
            game.session.screen = Screen::Cleared;
            r.events.push_back(GameEvent::ReachedExit);
            break;
        default:
            break;
    }
}

bool UpdateTimer(GameSession& s, float dt) {
    if (s.screen != Screen::Playing) return false;
    s.elapsed += dt;
    if (s.mode != GameMode::TimeLimit) return false;

    s.timeRemaining -= dt;
    if (s.timeRemaining <= 0.0f) {
        s.timeRemaining = 0.0f;
        s.screen = Screen::GameOver;
        return true;
    }
    return false;
}

bool UpdateBestRecord(std::map<std::string, int>& records, const std::string& mazeName, int moves) {
    auto it = records.find(mazeName);
    if (it == records.end() || moves < it->second) {
        records[mazeName] = moves;
        return true;
    }
    return false;
}

SaveData MakeSnapshot(const Game& game) {
    SaveData s;
    s.version       = SAVE_VERSION;
    s.hasSave       = true;
    s.map           = game.map;
    s.original      = game.original.CellCount() > 0 ? game.original : game.map;
    s.player        = game.player;
    s.trail         = game.trail;
    s.mode          = game.session.mode;
    s.timeLimit     = game.session.timeLimit;
    s.timeRemaining = game.session.timeRemaining;
    return s;
}

bool RestoreSnapshot(const SaveData& save, Game& game) {
    if (!save.hasSave || save.map.CellCount() <= 0) return false;

    game.map      = save.map;
    game.original = save.original.CellCount() > 0 ? save.original : save.map;
    game.player   = save.player;
    game.trail    = save.trail;
    if ((int)game.trail.visited.size() != game.map.CellCount()) {
        game.trail.visited.assign(game.map.CellCount(), false);
        for (GridPos p : game.trail.order)
            if (IsInside(game.map, p)) game.trail.visited[game.map.Index(p)] = true;
    }

    GameSession& s  = game.session;
    s.mode          = save.mode;
    s.timeLimit     = save.timeLimit;
    s.timeRemaining = save.timeRemaining;
    s.elapsed       = 0.0f;
    s.newRecord     = false;
    s.showGuide     = false;
    s.guidePath.clear();
    s.menuIndex     = 0;
    s.resumedFromSave = true;
    s.screen        = Screen::Playing;
    return true;
}
