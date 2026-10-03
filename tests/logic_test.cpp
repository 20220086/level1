// logic_test.cpp
// Raylib 없이 게임 로직, 경로 탐색, 파일 입출력을 콘솔에서 시험한다. (보고서 7장 시험/평가)
// 빌드: g++ tests/logic_test.cpp src/logic.cpp src/pathfind.cpp src/fileio.cpp -Isrc -std=c++17 -o logic_test
#include <cstdio>
#include <string>

#include "fileio.h"
#include "logic.h"
#include "pathfind.h"

static int g_fail = 0;
#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (cond) std::printf("  [PASS] %s\n", msg);                  \
        else { std::printf("  [FAIL] %s\n", msg); ++g_fail; }         \
    } while (0)

static Direction StepDir(GridPos a, GridPos b) {
    if (b.x > a.x) return Direction::Right;
    if (b.x < a.x) return Direction::Left;
    if (b.y > a.y) return Direction::Down;
    return Direction::Up;
}

// BFS 안내 경로를 따라 실제로 TryMove를 호출하여 출구까지 도달하는지 확인
static bool PlayByGuide(Game& g, int maxSteps = 5000) {
    for (int step = 0; step < maxSteps && g.session.screen == Screen::Playing; ++step) {
        auto path = FindPath(g.map, g.player.pos, g.player.keys, true);
        if (path.empty()) path = FindPath(g.map, g.player.pos, g.player.keys, false);
        if (path.size() < 2) return false;
        TryMove(g, StepDir(path[0], path[1]));
    }
    return g.session.screen == Screen::Cleared;
}

int main() {
    const char* mazes[] = {"maps/level1_basic.txt", "maps/level2_keys.txt", "maps/level3_advanced.txt"};
    for (const char* f : mazes) {
        std::printf("== %s\n", f);
        Map m;
        LoadResult r = LoadMap(f, m);
        CHECK(r.ok, ("load/validate " + r.error).c_str());
        if (!r.ok) continue;
        CHECK(IsReachable(m), "exit reachable (BFS)");

        auto path = FindPath(m, m.start, {}, false);
        std::printf("  size %dx%d, shortest path %zu cells\n", m.width, m.height, path.size());

        // 문을 벽으로 바꾸면 도달 불가해야 열쇠가 실제로 필요한 미로
        bool hasDoor = false;
        Map noDoor = m;
        for (Tile& t : noDoor.tiles)
            if (t.type == TileType::Door) { t.type = TileType::Wall; hasDoor = true; }
        if (hasDoor) CHECK(!IsReachable(noDoor), "key is required to reach exit");

        Game g;
        StartLevel(g, m, GameMode::Normal, 60.0f);
        CHECK(PlayByGuide(g), "player reaches exit by TryMove");
        std::printf("  moves: %d\n", g.player.moveCount);
    }

    std::printf("== invalid example\n");
    {
        Map m;
        LoadResult r = LoadMap("maps/zz_invalid_example.txt", m);
        CHECK(!r.ok, "invalid maze is rejected");
        std::printf("  error: %s (line %d)\n", r.error.c_str(), r.line);
    }

    std::printf("== collision rules\n");
    {
        Map m;
        LoadMap("maps/level1_basic.txt", m);
        Game g;
        StartLevel(g, m, GameMode::Normal, 60.0f);
        MoveResult r = TryMove(g, Direction::Up);  // S 위는 벽
        CHECK(!r.moved && r.events[0] == GameEvent::BumpWall, "wall blocks movement");
        CHECK(g.player.moveCount == 0, "bump does not count as a move");
        r = TryMove(g, Direction::Right);
        CHECK(r.moved && g.player.moveCount == 1, "valid move counts");
        CHECK(IsInside(m, {0, 0}) && !IsInside(m, {-1, 0}) && !IsInside(m, {m.width, 0}), "range check");
    }

    std::printf("== wall break (extension)\n");
    {
        Map m;
        LoadMap("maps/level1_basic.txt", m);
        Game g;
        StartLevel(g, m, GameMode::Normal, 60.0f);
        CHECK(ToggleBreakMode(g.player) == BreakToggle::Armed, "F arms wall break");
        CHECK(ToggleBreakMode(g.player) == BreakToggle::Disarmed, "F again cancels");
        ToggleBreakMode(g.player);
        MoveResult r = TryMove(g, Direction::Up);  // (1,0) 은 바깥 테두리
        CHECK(!r.brokeWall && r.events[0] == GameEvent::BreakBlocked && g.player.breakArmed,
              "outer wall cannot be broken (stays armed)");
        TryMove(g, Direction::Down);                // (1,2)
        int movesBefore = g.player.moveCount;
        auto pathBefore = FindPath(g.map, g.player.pos, g.player.keys, false);
        r = TryMove(g, Direction::Right);          // (2,2) 안쪽 벽
        CHECK(r.brokeWall && g.map.At({2, 2}).type == TileType::Path, "inner wall is broken into a path");
        CHECK(!r.moved && (g.player.pos == GridPos{1, 2}), "player stays in place when breaking");
        CHECK(g.player.moveCount == movesBefore, "breaking itself is not counted as a move");
        CHECK(g.player.breakUsed && !g.player.breakArmed, "break is used up");
        CHECK(ToggleBreakMode(g.player) == BreakToggle::AlreadyUsed, "only one break per game");
        auto pathAfter = FindPath(g.map, g.player.pos, g.player.keys, false);
        std::printf("  shortest path %zu -> %zu cells after break\n", pathBefore.size(), pathAfter.size());
        CHECK(!pathAfter.empty() && pathAfter.size() <= pathBefore.size(), "BFS uses the broken wall");
        r = TryMove(g, Direction::Left);           // (0,2) 테두리, 대기 아님
        CHECK(r.events[0] == GameEvent::BumpWall, "after use, walls block again");

        Map m2;
        LoadMap("maps/level2_keys.txt", m2);
        Game g2;
        StartLevel(g2, m2, GameMode::Normal, 60.0f);
        g2.player.pos = {15, 5};                    // 문 A (15,6) 바로 위
        ToggleBreakMode(g2.player);
        r = TryMove(g2, Direction::Down);
        CHECK(r.events[0] == GameEvent::DoorLocked && g2.map.At({15, 6}).type == TileType::Door &&
                  g2.player.breakArmed, "doors cannot be broken");

        StartLevel(g, m, GameMode::Normal, 60.0f);
        CHECK(!g.player.breakUsed && !g.player.breakArmed, "new game resets the break");
    }

    std::printf("== save / load round trip\n");
    {
        Map m;
        LoadMap("maps/level2_keys.txt", m);
        Game g;
        g.original = m;
        StartLevel(g, m, GameMode::TimeLimit, 90.0f);
        for (int i = 0; i < 25 && g.session.screen == Screen::Playing; ++i) {
            auto p = FindPath(g.map, g.player.pos, g.player.keys, true);
            if (p.size() >= 2) TryMove(g, StepDir(p[0], p[1]));
        }
        SaveData s = MakeSnapshot(g);
        CHECK(SaveGame("data/test_save.txt", s), "save file written");
        SaveData loaded;
        CHECK(LoadGame("data/test_save.txt", loaded) && loaded.hasSave, "save file loaded");
        Game g2;
        CHECK(RestoreSnapshot(loaded, g2), "snapshot restored");
        bool same = g2.player.pos == g.player.pos && g2.player.moveCount == g.player.moveCount &&
                    g2.player.keys == g.player.keys && g2.trail.order.size() == g.trail.order.size();
        for (int i = 0; same && i < g.map.CellCount(); ++i)
            same = g.map.tiles[i].type == g2.map.tiles[i].type && g.map.tiles[i].id == g2.map.tiles[i].id;
        CHECK(same, "restored state equals saved state");
        CHECK(g2.session.resumedFromSave, "continue marks the game as resumed");
        bool origOk = g2.original.CellCount() == m.CellCount();
        for (int i = 0; origOk && i < m.CellCount(); ++i) origOk = g2.original.tiles[i].type == m.tiles[i].type;
        CHECK(origOk, "retry after continue uses the original maze");

        // 부수기 상태도 저장/복원되는가
        g.player.breakUsed = true; g.player.brokenAt = {3, 3};
        SaveGame("data/test_save.txt", MakeSnapshot(g));
        SaveData l2; LoadGame("data/test_save.txt", l2);
        CHECK(l2.player.breakUsed && (l2.player.brokenAt == GridPos{3, 3}), "wall-break state is saved");
    }

    std::printf("== records\n");
    {
        std::remove("data/test_records.txt");
        AppendPlayRecord("data/test_records.txt", "a.txt", 40, GameMode::Normal, true);
        AppendPlayRecord("data/test_records.txt", "a.txt", 30, GameMode::Normal, true);
        AppendPlayRecord("data/test_records.txt", "a.txt", 10, GameMode::TimeLimit, false);
        auto best = LoadRecords("data/test_records.txt");
        CHECK(best["a.txt"] == 30, "best = minimum cleared moves");
        CHECK(!UpdateBestRecord(best, "a.txt", 35), "worse score is not a record");
        CHECK(UpdateBestRecord(best, "a.txt", 25), "better score is a record");
    }

    std::printf("== maze generation\n");
    {
        int okCount = 0;
        for (unsigned seed = 1; seed <= 200; ++seed) {
            Map m = GenerateMaze(21, 15, seed);
            if (ValidateMap(m).ok && IsReachable(m)) ++okCount;
        }
        CHECK(okCount == 200, "200 generated mazes are valid and solvable");
    }

    std::printf("\n%s (%d failure%s)\n", g_fail ? "SOME TESTS FAILED" : "ALL TESTS PASSED", g_fail,
                g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
