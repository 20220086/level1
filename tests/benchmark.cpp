// benchmark.cpp
// 보고서 9장 실험 2~4 측정 프로그램 (Raylib 불필요)
// 빌드: g++ tests/benchmark.cpp src/logic.cpp src/pathfind.cpp src/fileio.cpp -Isrc -std=c++17 -O2 -o benchmark
// 실행: 프로젝트 폴더에서 ./benchmark
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "fileio.h"
#include "logic.h"
#include "pathfind.h"

using Clock = std::chrono::steady_clock;

static double Ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// 경로의 실제 이동 비용: 이동 칸 수 + 경로 위 페널티 함정 수 x trapPenalty
struct Route {
    bool found = false;
    int  steps = 0;     // 실제로 걸은 칸 수 (애니메이션 시간 계산용)
    int  moves = 0;     // 이동 횟수 (페널티 포함, 기록 기준)
};

static Route Evaluate(const Map& m, const std::vector<GridPos>& path) {
    Route r;
    if (path.empty()) return r;
    r.found = true;
    r.steps = static_cast<int>(path.size()) - 1;
    r.moves = r.steps;
    for (size_t i = 1; i < path.size(); ++i)
        if (m.At(path[i]).type == TileType::TrapPenalty) r.moves += m.trapPenalty;
    return r;
}

// 시작 위치에서 출구까지의 최소 이동 횟수 (함정 회피 경로와 함정 허용 경로 중 작은 쪽)
static Route MinRoute(const Map& m) {
    Route a = Evaluate(m, FindPath(m, m.start, {}, true));
    Route b = Evaluate(m, FindPath(m, m.start, {}, false));
    if (!a.found) return b;
    if (!b.found) return a;
    return (b.moves < a.moves) ? b : a;
}

static int OpenCells(const Map& m) {
    int n = 0;
    for (const Tile& t : m.tiles)
        if (t.type != TileType::Wall) ++n;
    return n;
}

int main() {
    // ------------------------------------------------------------- 실험 2
    std::printf("=== EXPERIMENT 2: processing time (average of 100 runs) ===\n");
    std::printf("size,cells,gen_ms,bfs_ms,bfs_path_cells\n");
    const int sizes[] = {11, 21, 41, 81, 161};
    const int RUNS = 100;
    for (int s : sizes) {
        double genTotal = 0.0, bfsTotal = 0.0;
        size_t pathLen = 0;
        volatile size_t sink = 0;
        for (int i = 0; i < RUNS; ++i) {
            auto t0 = Clock::now();
            Map m = GenerateMaze(s, s, 1000u + i);
            auto t1 = Clock::now();
            std::vector<GridPos> p = FindPath(m, m.start, {}, false);
            auto t2 = Clock::now();
            genTotal += Ms(t0, t1);
            bfsTotal += Ms(t1, t2);
            pathLen = p.size();
            sink = sink + p.size();
        }
        std::printf("%dx%d,%d,%.4f,%.4f,%zu\n", s, s, s * s, genTotal / RUNS, bfsTotal / RUNS, pathLen);
    }

    // ------------------------------------------------------------- 실험 3, 4
    const char* mazes[] = {"maps/level1_basic.txt", "maps/level2_keys.txt", "maps/level3_advanced.txt"};
    std::printf("\n=== EXPERIMENT 3: shortest path and wall-break effect ===\n");
    std::vector<Route> baseRoutes;
    std::vector<Map> maps;
    for (const char* f : mazes) {
        Map m;
        LoadResult lr = LoadMap(f, m);
        if (!lr.ok) { std::printf("%s load error: %s\n", f, lr.error.c_str()); return 1; }
        maps.push_back(m);

        Route base = MinRoute(m);
        baseRoutes.push_back(base);

        int breakable = 0, shortcut = 0, best = 0, sumGain = 0;
        std::vector<GridPos> bestWalls;
        auto t0 = Clock::now();
        for (int y = 0; y < m.height; ++y) {
            for (int x = 0; x < m.width; ++x) {
                GridPos p{x, y};
                if (m.At(p).type != TileType::Wall || IsBorder(m, p)) continue;
                ++breakable;
                Map broken = m;
                broken.At(p) = Tile{TileType::Path, -1};
                Route r = MinRoute(broken);
                int gain = (r.found && base.found) ? base.moves - r.moves : 0;
                if (gain > 0) {
                    ++shortcut;
                    sumGain += gain;
                }
                if (gain > best) { best = gain; bestWalls.clear(); }
                if (gain == best && gain > 0) bestWalls.push_back(p);
            }
        }
        auto t1 = Clock::now();

        std::printf("\n[%s] size %dx%d\n", m.name.c_str(), m.width, m.height);
        std::printf("  min moves (no break): %d  (cells walked %d)\n", base.moves, base.steps);
        std::printf("  breakable inner walls: %d\n", breakable);
        std::printf("  walls that create a shortcut: %d (%.1f%%)\n", shortcut,
                    breakable ? 100.0 * shortcut / breakable : 0.0);
        std::printf("  max reduction: %d moves -> best min moves %d\n", best, base.moves - best);
        std::printf("  best wall position(s) (x,y):");
        for (GridPos p : bestWalls) std::printf(" (%d,%d)", p.x, p.y);
        std::printf("\n");
        std::printf("  average reduction over all breakable walls: %.2f moves\n",
                    breakable ? static_cast<double>(sumGain) / breakable : 0.0);
        std::printf("  average reduction over shortcut walls only: %.2f moves\n",
                    shortcut ? static_cast<double>(sumGain) / shortcut : 0.0);
        std::printf("  exhaustive search time: %.2f ms\n", Ms(t0, t1));
    }

    std::printf("\n=== EXPERIMENT 4: time limit margin ===\n");
    for (size_t i = 0; i < maps.size(); ++i) {
        const Map& m = maps[i];
        int open = OpenCells(m);
        float limit = std::max(30.0f, 20.0f + open * 0.5f);
        double minTime = baseRoutes[i].steps * 0.12;
        std::printf("[%s] open cells %d, time limit %.1f s, min walking time %.2f s (%d cells x 0.12 s), "
                    "margin %.1f s, ratio %.1fx\n",
                    m.name.c_str(), open, limit, minTime, baseRoutes[i].steps, limit - minTime, limit / minTime);
    }
    return 0;
}
