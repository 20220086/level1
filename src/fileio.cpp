// fileio.cpp
#include "fileio.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

LoadResult Fail(const std::string& msg, int line = -1) {
    LoadResult r;
    r.ok = false;
    r.error = msg;
    r.line = line;
    return r;
}

LoadResult Ok() {
    LoadResult r;
    r.ok = true;
    return r;
}

void WriteMapRows(std::ostream& out, const Map& m) {
    for (int y = 0; y < m.height; ++y) {
        for (int x = 0; x < m.width; ++x) out << TileToChar(m.At({x, y}));
        out << '\n';
    }
}

// 세이브 파일의 맵 행들을 읽는다. (map.width/height는 미리 설정되어 있어야 함)
bool ReadMapRows(std::istream& in, Map& m) {
    m.tiles.assign(m.CellCount(), Tile{});
    int starts = 0, exits = 0;
    for (int y = 0; y < m.height; ++y) {
        std::string row;
        if (!(in >> row) || static_cast<int>(row.size()) != m.width) return false;
        for (int x = 0; x < m.width; ++x) {
            bool ok = false;
            Tile t = CharToTile(row[x], ok);
            if (!ok) return false;
            m.tiles[y * m.width + x] = t;
            if (t.type == TileType::Start) { m.start = {x, y}; ++starts; }
            if (t.type == TileType::Exit)  { m.exit  = {x, y}; ++exits; }
        }
    }
    return starts == 1 && exits == 1;
}

std::string ModeToString(GameMode m) { return m == GameMode::TimeLimit ? "TimeLimit" : "Normal"; }

}  // namespace

// ---------------------------------------------------------------------------
// 미로 파일
// ---------------------------------------------------------------------------
std::vector<std::string> ListMazeFiles(const std::string& dir) {
    std::vector<std::string> files;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return files;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".txt")
            files.push_back(entry.path().filename().string());
    }
    std::sort(files.begin(), files.end());
    return files;
}

Tile CharToTile(char c, bool& ok) {
    ok = true;
    switch (c) {
        case '#': return {TileType::Wall, -1};
        case '.': return {TileType::Path, -1};
        case 'S': return {TileType::Start, -1};
        case 'E': return {TileType::Exit, -1};
        case '^': return {TileType::TrapReset, -1};
        case '*': return {TileType::TrapPenalty, -1};
        default: break;
    }
    // s, e / S, E 는 시작/출구로 예약되어 열쇠와 문으로 사용하지 않는다.
    if (c >= 'a' && c <= 'z' && c != 's' && c != 'e') return {TileType::Key, c - 'a'};
    if (c >= 'A' && c <= 'Z' && c != 'S' && c != 'E') return {TileType::Door, c - 'A'};
    ok = false;
    return {TileType::Wall, -1};
}

char TileToChar(const Tile& t) {
    switch (t.type) {
        case TileType::Wall:        return '#';
        case TileType::Path:        return '.';
        case TileType::Start:       return 'S';
        case TileType::Exit:        return 'E';
        case TileType::TrapReset:   return '^';
        case TileType::TrapPenalty: return '*';
        case TileType::Key:         return static_cast<char>('a' + t.id);
        case TileType::Door:        return static_cast<char>('A' + t.id);
    }
    return '#';
}

LoadResult ParseMapLines(const std::vector<std::string>& lines, Map& outMap) {
    if (lines.empty()) return Fail("File is empty");

    const int H = static_cast<int>(lines.size());
    const int W = static_cast<int>(lines[0].size());
    if (W < MIN_MAP_SIZE || H < MIN_MAP_SIZE)
        return Fail("Maze must be at least 10x10 (now " + std::to_string(W) + "x" + std::to_string(H) + ")");

    Map m;
    m.width  = W;
    m.height = H;
    m.tiles.resize(W * H);

    for (int y = 0; y < H; ++y) {
        const std::string& row = lines[y];
        if (static_cast<int>(row.size()) != W)
            return Fail("Line length mismatch: expected " + std::to_string(W) + ", got " +
                        std::to_string(row.size()), y + 1);
        for (int x = 0; x < W; ++x) {
            bool ok = false;
            Tile t = CharToTile(row[x], ok);
            if (!ok)
                return Fail(std::string("Invalid character '") + row[x] + "' at column " + std::to_string(x + 1),
                            y + 1);
            m.tiles[y * W + x] = t;
            if (t.type == TileType::Start) m.start = {x, y};
            if (t.type == TileType::Exit)  m.exit  = {x, y};
        }
    }
    m.name        = outMap.name;
    m.trapPenalty = outMap.trapPenalty;
    outMap = m;
    return Ok();
}

LoadResult ValidateMap(const Map& map) {
    if (map.width < MIN_MAP_SIZE || map.height < MIN_MAP_SIZE) return Fail("Maze must be at least 10x10");
    if (static_cast<int>(map.tiles.size()) != map.CellCount()) return Fail("Tile data size mismatch");

    int starts = 0, exits = 0, extraStartLine = -1, extraExitLine = -1;
    std::bitset<MAX_KEYS> keyIds, doorIds;
    int doorLine[MAX_KEYS];
    std::fill(doorLine, doorLine + MAX_KEYS, -1);

    for (int y = 0; y < map.height; ++y) {
        for (int x = 0; x < map.width; ++x) {
            const Tile& t = map.At({x, y});
            if (t.type == TileType::Start && ++starts == 2) extraStartLine = y + 1;
            if (t.type == TileType::Exit  && ++exits  == 2) extraExitLine  = y + 1;
            if (t.type == TileType::Key  && t.id >= 0 && t.id < MAX_KEYS) keyIds.set(t.id);
            if (t.type == TileType::Door && t.id >= 0 && t.id < MAX_KEYS) {
                doorIds.set(t.id);
                if (doorLine[t.id] < 0) doorLine[t.id] = y + 1;
            }
        }
    }
    if (starts != 1) return Fail("Maze needs exactly one start 'S' (found " + std::to_string(starts) + ")", extraStartLine);
    if (exits  != 1) return Fail("Maze needs exactly one exit 'E' (found " + std::to_string(exits) + ")", extraExitLine);

    for (int id = 0; id < MAX_KEYS; ++id) {
        if (doorIds.test(id) && !keyIds.test(id))
            return Fail(std::string("Door '") + char('A' + id) + "' has no matching key '" + char('a' + id) + "'",
                        doorLine[id]);
    }
    if (static_cast<int>(keyIds.count()) > MAX_KEY_TYPES)
        return Fail("Too many key types (max " + std::to_string(MAX_KEY_TYPES) + ")");
    return Ok();
}

LoadResult LoadMap(const std::string& path, Map& outMap) {
    std::ifstream in(path);
    if (!in.is_open()) return Fail("Cannot open file: " + path);

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();          // Windows 줄바꿈
        if (lines.empty() && line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3); // 메모장 UTF-8 BOM
        lines.push_back(line);
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();         // 끝의 빈 줄 무시

    Map m;
    m.name = fs::path(path).filename().string();
    LoadResult r = ParseMapLines(lines, m);
    if (!r.ok) return r;
    r = ValidateMap(m);
    if (!r.ok) return r;

    outMap = m;
    return Ok();
}

// ---------------------------------------------------------------------------
// 기록 파일: "미로이름|이동횟수|모드|클리어여부" 한 줄씩 추가
// ---------------------------------------------------------------------------
bool AppendPlayRecord(const std::string& path, const std::string& mazeName, int moves, GameMode mode,
                      bool cleared) {
    std::error_code ec;
    fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);

    std::string name = mazeName;
    std::replace(name.begin(), name.end(), '|', '_');

    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) return false;
    out << name << '|' << moves << '|' << ModeToString(mode) << '|' << (cleared ? 1 : 0) << '\n';
    return static_cast<bool>(out);
}

std::map<std::string, int> LoadRecords(const std::string& path) {
    std::map<std::string, int> best;
    std::ifstream in(path);
    if (!in.is_open()) return best;  // 첫 실행

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '|')) f.push_back(part);
        if (f.size() < 4) continue;  // 손상된 줄은 건너뜀

        int moves = 0;
        try { moves = std::stoi(f[1]); } catch (...) { continue; }
        if (f[3] != "1" || f[0] == GENERATED_MAZE_NAME) continue;

        auto it = best.find(f[0]);
        if (it == best.end() || moves < it->second) best[f[0]] = moves;
    }
    return best;
}

// ---------------------------------------------------------------------------
// 세이브 파일 (텍스트)
// ---------------------------------------------------------------------------
bool SaveGame(const std::string& path, const SaveData& d) {
    std::error_code ec;
    fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);

    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out.is_open()) return false;

        out << "MAZESAVE " << SAVE_VERSION << '\n';
        out << "HASSAVE " << (d.hasSave ? 1 : 0) << '\n';
        if (d.hasSave) {
            out << "NAME " << d.map.name << '\n';
            out << "GENERATED " << (d.map.generated ? 1 : 0) << '\n';
            out << "MODE " << (d.mode == GameMode::TimeLimit ? 1 : 0) << '\n';
            out << "TIME " << d.timeLimit << ' ' << d.timeRemaining << '\n';
            out << "SIZE " << d.map.width << ' ' << d.map.height << '\n';
            out << "PENALTY " << d.map.trapPenalty << '\n';
            const Map& orig = d.original.CellCount() == d.map.CellCount() ? d.original : d.map;
            out << "MAP\n";
            WriteMapRows(out, d.map);
            out << "ORIGINAL\n";
            WriteMapRows(out, orig);
            out << "PLAYER " << d.player.pos.x << ' ' << d.player.pos.y << ' ' << d.player.moveCount << ' '
                << d.player.keys.to_ulong() << '\n';
            out << "BREAK " << (d.player.breakUsed ? 1 : 0) << ' ' << (d.player.breakArmed ? 1 : 0) << ' '
                << d.player.brokenAt.x << ' ' << d.player.brokenAt.y << '\n';
            out << "TRAIL " << d.trail.order.size() << '\n';
            for (GridPos p : d.trail.order) out << p.x << ' ' << p.y << '\n';
        }
        out << "END\n";
        if (!out) return false;
    }
    // 임시 파일에 모두 쓴 뒤 교체하여, 저장 도중 오류가 나도 기존 파일이 손상되지 않게 한다.
    fs::remove(path, ec);
    ec.clear();
    fs::rename(tmp, path, ec);
    return !ec;
}

bool LoadGame(const std::string& path, SaveData& out) {
    out = SaveData{};
    std::ifstream in(path);
    if (!in.is_open()) return true;  // 세이브 없음 (오류 아님)

    auto fail = [&]() { out = SaveData{}; return false; };
    std::string tag;
    int version = 0, has = 0;
    if (!(in >> tag >> version) || tag != "MAZESAVE") return fail();
    if (version != SAVE_VERSION) { out = SaveData{}; return true; }  // 이전 버전 세이브는 조용히 무시
    if (!(in >> tag >> has) || tag != "HASSAVE") return fail();
    if (!has) return true;

    SaveData d;
    d.hasSave = true;
    int gen = 0, mode = 0;
    if (!(in >> tag) || tag != "NAME") return fail();
    in.get();  // 공백 하나
    std::getline(in, d.map.name);
    if (!d.map.name.empty() && d.map.name.back() == '\r') d.map.name.pop_back();
    if (!(in >> tag >> gen) || tag != "GENERATED") return fail();
    if (!(in >> tag >> mode) || tag != "MODE") return fail();
    if (!(in >> tag >> d.timeLimit >> d.timeRemaining) || tag != "TIME") return fail();
    if (!(in >> tag >> d.map.width >> d.map.height) || tag != "SIZE") return fail();
    if (!(in >> tag >> d.map.trapPenalty) || tag != "PENALTY") return fail();
    if (!(in >> tag) || tag != "MAP") return fail();
    if (d.map.width < MIN_MAP_SIZE || d.map.height < MIN_MAP_SIZE || d.map.width > 200 || d.map.height > 200)
        return fail();

    d.map.generated = (gen != 0);
    d.mode = mode ? GameMode::TimeLimit : GameMode::Normal;
    if (!ReadMapRows(in, d.map)) return fail();
    d.original = d.map;  // 크기, 이름, 페널티 복사 후 타일을 다시 읽음
    if (!(in >> tag) || tag != "ORIGINAL" || !ReadMapRows(in, d.original)) return fail();

    unsigned long keys = 0;
    if (!(in >> tag >> d.player.pos.x >> d.player.pos.y >> d.player.moveCount >> keys) || tag != "PLAYER")
        return fail();
    d.player.keys = std::bitset<MAX_KEYS>(keys);
    int used = 0, armed = 0;
    if (!(in >> tag >> used >> armed >> d.player.brokenAt.x >> d.player.brokenAt.y) || tag != "BREAK") return fail();
    d.player.breakUsed  = used != 0;
    d.player.breakArmed = armed != 0 && !d.player.breakUsed;
    GridPos p = d.player.pos;
    if (p.x < 0 || p.x >= d.map.width || p.y < 0 || p.y >= d.map.height) return fail();
    if (d.map.At(p).type == TileType::Wall) return fail();

    size_t count = 0;
    if (!(in >> tag >> count) || tag != "TRAIL" || count > 1000000) return fail();
    d.trail.visited.assign(d.map.CellCount(), false);
    for (size_t i = 0; i < count; ++i) {
        GridPos t;
        if (!(in >> t.x >> t.y)) return fail();
        if (t.x < 0 || t.x >= d.map.width || t.y < 0 || t.y >= d.map.height) return fail();
        d.trail.order.push_back(t);
        d.trail.visited[d.map.Index(t)] = true;
    }
    out = d;
    return true;
}
