// pathfind.cpp
#include "pathfind.h"

#include <algorithm>
#include <queue>
#include <random>
#include <stack>

namespace {

const GridPos DIRS[4] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

bool Inside(const Map& m, GridPos p) {
    return p.x >= 0 && p.x < m.width && p.y >= 0 && p.y < m.height;
}

bool IsTrap(TileType t) { return t == TileType::TrapReset || t == TileType::TrapPenalty; }

}  // namespace

std::vector<SearchState> GetNextStates(const Map& map, const SearchState& st,
                                       bool avoidTraps, const int bitOfKey[MAX_KEYS]) {
    std::vector<SearchState> next;
    next.reserve(4);
    for (const GridPos& d : DIRS) {
        GridPos t{st.pos.x + d.x, st.pos.y + d.y};
        if (!Inside(map, t)) continue;
        const Tile& tile = map.At(t);
        if (tile.type == TileType::Wall) continue;
        if (tile.type == TileType::Door) {
            int b = (tile.id >= 0 && tile.id < MAX_KEYS) ? bitOfKey[tile.id] : -1;
            if (b < 0 || !((st.keyMask >> b) & 1u)) continue;
        }
        if (avoidTraps && IsTrap(tile.type)) continue;

        SearchState n{t, st.keyMask};
        if (tile.type == TileType::Key && tile.id >= 0 && tile.id < MAX_KEYS) {
            int b = bitOfKey[tile.id];
            if (b >= 0) n.keyMask |= (1u << b);
        }
        if (tile.type == TileType::TrapReset) n.pos = map.start;  // 실제 게임과 같이 시작 위치로
        next.push_back(n);
    }
    return next;
}

std::vector<GridPos> FindPath(const Map& map, GridPos from,
                              const std::bitset<MAX_KEYS>& heldKeys, bool avoidTraps) {
    if (map.CellCount() <= 0 || !Inside(map, from)) return {};

    // 1) 미로에 등장하는 열쇠/문 번호를 0..K-1로 압축
    int bitOfKey[MAX_KEYS];
    std::fill(bitOfKey, bitOfKey + MAX_KEYS, -1);
    int K = 0;
    for (const Tile& t : map.tiles) {
        if ((t.type == TileType::Key || t.type == TileType::Door) && t.id >= 0 && t.id < MAX_KEYS &&
            bitOfKey[t.id] < 0)
            bitOfKey[t.id] = K++;
    }
    if (K > 8) return {};  // 상태 수 폭증 방지 (설계 상한은 4)

    unsigned int startMask = 0;
    for (int id = 0; id < MAX_KEYS; ++id)
        if (bitOfKey[id] >= 0 && heldKeys.test(id)) startMask |= (1u << bitOfKey[id]);

    // 2) 상태 번호 = mask * N + 칸 위치
    const int    N = map.CellCount();
    const size_t S = static_cast<size_t>(N) << K;
    std::vector<char> visited(S, 0);
    std::vector<int>  parent(S, -1);
    std::queue<int>   frontier;

    auto encode = [&](const SearchState& s) { return static_cast<int>(s.keyMask) * N + map.Index(s.pos); };

    int s0 = encode({from, startMask});
    visited[s0] = 1;
    frontier.push(s0);

    int goal = -1;
    while (!frontier.empty()) {
        int cur = frontier.front();
        frontier.pop();
        SearchState st;
        st.keyMask = static_cast<unsigned int>(cur / N);
        int cell   = cur % N;
        st.pos     = {cell % map.width, cell / map.width};

        if (st.pos == map.exit) { goal = cur; break; }

        for (const SearchState& ns : GetNextStates(map, st, avoidTraps, bitOfKey)) {
            int id = encode(ns);
            if (!visited[id]) {
                visited[id] = 1;
                parent[id]  = cur;
                frontier.push(id);
            }
        }
    }
    if (goal < 0) return {};

    // 3) 경로 복원
    std::vector<GridPos> path;
    for (int s = goal; s != -1; s = parent[s]) {
        int cell = s % N;
        path.push_back({cell % map.width, cell / map.width});
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool IsReachable(const Map& map) {
    if (map.CellCount() <= 0) return false;
    return !FindPath(map, map.start, std::bitset<MAX_KEYS>{}, false).empty();
}

Map GenerateMaze(int width, int height, unsigned int seed) {
    width  = std::max(width,  MIN_MAP_SIZE + 1);
    height = std::max(height, MIN_MAP_SIZE + 1);
    if (width  % 2 == 0) ++width;
    if (height % 2 == 0) ++height;

    Map m;
    m.name      = GENERATED_MAZE_NAME;
    m.generated = true;
    m.width     = width;
    m.height    = height;
    m.tiles.assign(width * height, Tile{TileType::Wall, -1});

    std::mt19937        rng(seed);
    std::stack<GridPos> genStack;
    std::vector<bool>   genVisited(width * height, false);
    std::vector<GridPos> candidates;

    GridPos first{1, 1};
    m.At(first).type = TileType::Path;
    genVisited[m.Index(first)] = true;
    genStack.push(first);

    while (!genStack.empty()) {
        GridPos cur = genStack.top();
        candidates.clear();
        for (const GridPos& d : DIRS) {
            GridPos n{cur.x + d.x * 2, cur.y + d.y * 2};
            if (n.x > 0 && n.x < width - 1 && n.y > 0 && n.y < height - 1 && !genVisited[m.Index(n)])
                candidates.push_back(n);
        }
        if (candidates.empty()) { genStack.pop(); continue; }

        std::uniform_int_distribution<int> pick(0, (int)candidates.size() - 1);
        GridPos n = candidates[pick(rng)];
        GridPos between{(cur.x + n.x) / 2, (cur.y + n.y) / 2};
        m.At(between).type = TileType::Path;
        m.At(n).type       = TileType::Path;
        genVisited[m.Index(n)] = true;
        genStack.push(n);
    }

    // 시작 위치 지정 후, BFS로 가장 먼 칸을 출구로 지정
    m.start = first;
    m.At(first).type = TileType::Start;

    std::vector<int> dist(width * height, -1);
    std::queue<GridPos> q;
    dist[m.Index(first)] = 0;
    q.push(first);
    GridPos far = first;
    while (!q.empty()) {
        GridPos c = q.front(); q.pop();
        if (dist[m.Index(c)] > dist[m.Index(far)]) far = c;
        for (const GridPos& d : DIRS) {
            GridPos n{c.x + d.x, c.y + d.y};
            if (!Inside(m, n) || m.At(n).type == TileType::Wall || dist[m.Index(n)] >= 0) continue;
            dist[m.Index(n)] = dist[m.Index(c)] + 1;
            q.push(n);
        }
    }
    m.exit = far;
    m.At(far).type = TileType::Exit;
    return m;
}
