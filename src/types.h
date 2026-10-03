// types.h
// 모든 모듈이 공유하는 자료구조 정의 (보고서 5장 자료구조 설계)
// 이 파일은 Raylib을 포함하지 않는다. 게임 로직을 화면 없이 시험하기 위함이다.
#pragma once

#include <bitset>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 상수
// ---------------------------------------------------------------------------
constexpr int MAX_KEYS      = 26;  // 열쇠 번호 범위 (a~z)
constexpr int MIN_MAP_SIZE  = 10;  // 미로 최소 크기 (10x10)
constexpr int MAX_KEY_TYPES = 4;   // 한 미로의 열쇠 종류 상한 (BFS 상태 수 N x 2^K 제한)
constexpr int SAVE_VERSION  = 2;   // 세이브 파일 형식 버전 (2: 벽 부수기, 원본 미로 추가)
inline const std::string GENERATED_MAZE_NAME = "(random maze)";

// ---------------------------------------------------------------------------
// 좌표와 타일
// ---------------------------------------------------------------------------
struct GridPos {
    int x = 0;  // 열 번호 (0부터)
    int y = 0;  // 행 번호 (0부터)
};
inline bool operator==(GridPos a, GridPos b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(GridPos a, GridPos b) { return !(a == b); }

enum class Direction { Up, Down, Left, Right };

enum class TileType : std::uint8_t {
    Wall,         // # 벽
    Path,         // . 길
    Start,        // S 시작 위치
    Exit,         // E 출구
    Key,          // a~z 열쇠
    Door,         // A~Z 잠긴 문
    TrapReset,    // ^ 시작 위치로 되돌리는 함정
    TrapPenalty   // * 이동 횟수 페널티 함정
};

struct Tile {
    TileType type = TileType::Wall;
    int      id   = -1;  // 열쇠/문 짝 번호 (a/A = 0). 그 외 -1
};

// ---------------------------------------------------------------------------
// 미로
// ---------------------------------------------------------------------------
struct Map {
    std::string       name;              // 미로 파일 이름 (기록/세이브 구분용)
    int               width  = 0;        // 가로 칸 수
    int               height = 0;        // 세로 칸 수
    std::vector<Tile> tiles;             // width*height, (x,y) = tiles[y*width + x]
    GridPos           start;             // 시작 위치
    GridPos           exit;              // 출구 위치
    int               trapPenalty = 5;   // 페널티 함정의 추가 이동 횟수
    bool              generated = false; // 자동 생성 미로 여부

    int         CellCount() const { return width * height; }
    int         Index(GridPos p) const { return p.y * width + p.x; }
    Tile&       At(GridPos p) { return tiles[Index(p)]; }
    const Tile& At(GridPos p) const { return tiles[Index(p)]; }
};

// ---------------------------------------------------------------------------
// 플레이어와 경로
// ---------------------------------------------------------------------------
struct Player {
    GridPos               pos;            // 현재 격자 좌표
    int                   moveCount = 0;  // 이동 횟수 (페널티 포함)
    std::bitset<MAX_KEYS> keys;           // keys[i] == true 이면 i번 열쇠 소지

    // 확장 기능: 벽 부수기 (게임당 1회)
    bool    breakArmed = false;           // F로 부수기 대기 중인가 (캐릭터가 빛남)
    bool    breakUsed  = false;           // 이번 게임에서 이미 부쉈는가
    GridPos brokenAt{-1, -1};             // 부순 벽의 좌표 (잔해 표시용)
};

struct Trail {
    std::vector<GridPos> order;    // 지나온 좌표 (순서 보존)
    std::vector<bool>    visited;  // 칸별 경유 여부 (O(1) 확인용)
};

// ---------------------------------------------------------------------------
// 모듈 간 인터페이스 자료형 (보고서 6.1)
// ---------------------------------------------------------------------------
enum class GameEvent {
    Moved, BumpWall, KeyPicked, DoorOpened, DoorLocked, TrapTriggered, ReachedExit, TimeUp,
    WallBroken,    // 확장: 부수기 대기 상태로 벽에 부딪혀 벽을 부숨
    BreakBlocked   // 확장: 부수기 대기 상태로 바깥 테두리 벽에 부딪힘 (부술 수 없음)
};

struct MoveResult {
    bool                   moved      = false;  // 실제로 위치가 바뀌었는가
    bool                   teleported = false;  // 복귀형 함정으로 시작 위치로 이동했는가
    GridPos                from;                // 출발 좌표
    GridPos                to;                  // 최종 좌표
    Direction              dir = Direction::Down;
    int                    keyId = -1;          // 관련 열쇠/문 번호 (메시지용)
    bool                   brokeWall = false;   // 이번 입력으로 벽을 부쉈는가
    GridPos                breakPos{-1, -1};    // 부순(또는 부수려던) 벽의 좌표
    std::vector<GameEvent> events;              // 이번 이동에서 발생한 이벤트
};

struct LoadResult {
    bool        ok = false;
    std::string error;     // 오류 원인
    int         line = -1; // 오류 줄 번호 (없으면 -1)
};

enum class MenuInput   { None, Up, Down, Confirm, Back };
enum class GameCommand { None, Pause, Save, ToggleGuide, ToggleTrail, ToggleBreak };
enum class BreakToggle { Armed, Disarmed, AlreadyUsed };

// ---------------------------------------------------------------------------
// 게임 진행 상태
// ---------------------------------------------------------------------------
enum class Screen   { Title, MazeSelect, Records, Playing, Paused, Cleared, GameOver, Error };
enum class GameMode { Normal, TimeLimit };

// 메뉴 항목 번호 (main과 view가 같은 번호를 사용)
enum TitleItem  { TITLE_START, TITLE_TIME_ATTACK, TITLE_RANDOM, TITLE_CONTINUE, TITLE_RECORDS, TITLE_QUIT, TITLE_COUNT };
enum PauseItem  { PAUSE_RESUME, PAUSE_SAVE, PAUSE_TITLE, PAUSE_COUNT };
enum ResultItem { RESULT_RETRY, RESULT_TITLE, RESULT_COUNT };

struct GameSession {
    Screen   screen        = Screen::Title;
    GameMode mode          = GameMode::Normal;
    float    timeLimit     = 0.0f;  // 제한 시간(초)
    float    timeRemaining = 0.0f;  // 남은 시간(초)
    float    elapsed       = 0.0f;  // 경과 시간(초)

    std::vector<std::string> mazeFiles;   // 메뉴에 표시할 미로 파일 목록
    std::vector<std::string> mazeStatus;  // 각 미로 파일의 유효성 검사 결과
    int  menuIndex  = 0;                  // 현재 메뉴에서 선택된 항목

    bool newRecord  = false;              // 이번 클리어가 신기록인가
    bool showTrail  = true;               // 지나온 경로 표시 여부
    bool showGuide  = false;              // BFS 안내 경로 표시 여부
    std::vector<GridPos> guidePath;       // BFS 안내 경로

    std::string message;                  // 화면 하단 알림 문구
    float       messageTimer = 0.0f;      // 알림 표시 남은 시간

    bool resumedFromSave = false;         // 이어하기로 시작한 게임인가 (세이브 정리 판단용)

    LoadResult lastError;                 // 오류 화면에 표시할 내용
    Screen     errorReturn = Screen::Title;
};

// ---------------------------------------------------------------------------
// 세이브 데이터와 최상위 게임 상태
// ---------------------------------------------------------------------------
struct SaveData {
    int      version  = SAVE_VERSION;
    bool     hasSave  = false;
    Map      map;                 // 저장 시점의 미로 (열쇠/문/부순 벽 변경 포함)
    Map      original;            // 처음 상태의 미로 (이어하기 후 다시하기용)
    Player   player;
    Trail    trail;
    GameMode mode          = GameMode::Normal;
    float    timeLimit     = 0.0f;
    float    timeRemaining = 0.0f;
};

struct Game {
    Map         map;          // 현재 플레이 중인 미로
    Map         original;     // 다시하기(Retry)용 시작 시점 미로
    Player      player;
    Trail       trail;
    GameSession session;
    std::map<std::string, int> bestRecords;  // 미로 이름 -> 최소 이동 횟수
    SaveData    save;
};
