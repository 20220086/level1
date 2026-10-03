// main.cpp
// 메인 게임 루프: 각 모듈을 정해진 순서로 호출하고 모듈 사이의 데이터를 전달한다.
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "fileio.h"
#include "input.h"
#include "logic.h"
#include "pathfind.h"
#include "raylib.h"
#include "types.h"
#include "view.h"

namespace {

const std::string MAZE_DIR    = "maps";
const std::string RECORD_FILE = "data/records.txt";
const std::string SAVE_FILE   = "data/save.txt";
constexpr int RANDOM_MAZE_W = 23;
constexpr int RANDOM_MAZE_H = 15;

bool gQuit = false;
std::optional<Direction> gQueuedMove;  // 애니메이션 중 입력된 이동 (1개만 보관)

void SetScreen(Game& g, Screen s) {
    g.session.screen = s;
    g.session.menuIndex = 0;
}

void ShowMessage(GameSession& s, const std::string& text, float seconds = 2.0f) {
    s.message = text;
    s.messageTimer = seconds;
}

// 시간 제한: 걸을 수 있는 칸 수에 비례 (최소 30초)
float TimeLimitFor(const Map& m) {
    int open = 0;
    for (const Tile& t : m.tiles)
        if (t.type != TileType::Wall) ++open;
    return std::max(30.0f, 20.0f + open * 0.5f);
}

std::string FormatError(const LoadResult& r) {
    std::string s = "ERROR: " + r.error;
    if (r.line > 0) s += " (line " + std::to_string(r.line) + ")";
    return s;
}

// 미로 선택 화면용: 모든 미로 파일의 유효성을 미리 검사
void RefreshMazeStatus(Game& g) {
    GameSession& s = g.session;
    s.mazeFiles = ListMazeFiles(MAZE_DIR);
    s.mazeStatus.clear();
    for (const std::string& f : s.mazeFiles) {
        Map m;
        LoadResult r = LoadMap(MAZE_DIR + "/" + f, m);
        if (r.ok && !IsReachable(m)) { r.ok = false; r.error = "Exit is not reachable from start"; }
        s.mazeStatus.push_back(r.ok ? "OK" : FormatError(r));
    }
}

// 현재 위치에서 출구까지의 최단 경로 (함정 회피 우선, 없으면 함정 포함)
std::vector<GridPos> ShortestPath(const Game& g) {
    std::vector<GridPos> p = FindPath(g.map, g.player.pos, g.player.keys, true);
    if (p.empty()) p = FindPath(g.map, g.player.pos, g.player.keys, false);
    return p;
}

void RecomputeGuide(Game& g) { g.session.guidePath = ShortestPath(g); }

bool StartNewGame(Game& g, const std::string& path, bool generate, GameMode mode) {
    GameSession& s = g.session;
    Map m;
    LoadResult r;
    if (generate) {
        std::random_device rd;
        unsigned int seed = rd() ^ static_cast<unsigned int>(std::time(nullptr));
        for (int attempt = 0; attempt < 10; ++attempt) {
            m = GenerateMaze(RANDOM_MAZE_W, RANDOM_MAZE_H, seed + attempt);
            r = ValidateMap(m);
            if (r.ok && IsReachable(m)) break;
            r.ok = false;
            if (r.error.empty()) r.error = "Generated maze is not solvable";
        }
    } else {
        r = LoadMap(path, m);
        if (r.ok && !IsReachable(m)) { r.ok = false; r.error = "Exit is not reachable from start"; }
    }

    if (!r.ok) {
        s.lastError   = r;
        s.errorReturn = generate ? Screen::Title : Screen::MazeSelect;
        SetScreen(g, Screen::Error);
        return false;
    }
    g.original = m;
    StartLevel(g, m, mode, TimeLimitFor(m));
    SyncPlayer(g.player.pos);
    return true;
}

void DoSave(Game& g) {
    g.save = MakeSnapshot(g);
    if (SaveGame(SAVE_FILE, g.save)) {
        ShowMessage(g.session, "Game saved");
        PlayUiSound(UiSound::Save);
    } else {
        ShowMessage(g.session, "Save failed!");
    }
}

void HandleGameEnd(Game& g) {
    GameSession& s = g.session;
    bool cleared = s.screen == Screen::Cleared;

    AppendPlayRecord(RECORD_FILE, g.map.name, g.player.moveCount, s.mode, cleared);
    s.newRecord = false;
    if (cleared && !g.map.generated) s.newRecord = UpdateBestRecord(g.bestRecords, g.map.name, g.player.moveCount);

    // 이어하기로 시작한 게임을 끝냈을 때만 세이브를 비운다.
    // (같은 미로를 새로 시작해 끝낸 경우에는 기존 세이브를 유지)
    if (s.resumedFromSave && g.save.hasSave) {
        g.save.hasSave = false;
        SaveGame(SAVE_FILE, g.save);
    }
    s.menuIndex = 0;
    s.showGuide = false;
}

void AnnounceMove(Game& g, const MoveResult& r) {
    GameSession& s = g.session;
    for (GameEvent e : r.events) {
        switch (e) {
            case GameEvent::KeyPicked:
                ShowMessage(s, std::string("Got key '") + char('a' + r.keyId) + "'!");
                break;
            case GameEvent::DoorOpened:
                ShowMessage(s, std::string("Door '") + char('A' + r.keyId) + "' opened");
                break;
            case GameEvent::DoorLocked:
                ShowMessage(s, std::string(g.player.breakArmed ? "Doors can't be broken! " : "Locked! ") +
                                   "Find key '" + char('a' + r.keyId) + "'");
                break;
            case GameEvent::BreakBlocked:
                ShowMessage(s, "The outer wall can't be broken");
                break;
            case GameEvent::TrapTriggered:
                ShowMessage(s, r.teleported ? "Trap! Back to the start"
                                            : "Trap! +" + std::to_string(g.map.trapPenalty) + " moves");
                break;
            default:
                break;
        }
    }
}

void UpdatePlaying(Game& g, float dt) {
    GameSession& s = g.session;

    // 1) 게임 명령
    switch (ReadGameCommand()) {
        case GameCommand::Pause:
            SetScreen(g, Screen::Paused);
            PlayUiSound(UiSound::Confirm);
            return;
        case GameCommand::Save:
            DoSave(g);
            break;
        case GameCommand::ToggleGuide:
            s.showGuide = !s.showGuide;
            if (s.showGuide) {
                RecomputeGuide(g);
                if (s.guidePath.empty()) ShowMessage(s, "No path to the exit");
            }
            break;
        case GameCommand::ToggleTrail:
            s.showTrail = !s.showTrail;
            break;
        case GameCommand::ToggleBreak:
            switch (ToggleBreakMode(g.player)) {
                case BreakToggle::Armed:
                    ShowMessage(s, "Wall break ready: bump into a wall (F to cancel)");
                    PlayUiSound(UiSound::BreakArm);
                    break;
                case BreakToggle::Disarmed:
                    ShowMessage(s, "Wall break canceled");
                    PlayUiSound(UiSound::BreakCancel);
                    break;
                case BreakToggle::AlreadyUsed:
                    ShowMessage(s, "Wall break already used in this game");
                    break;
            }
            break;
        default:
            break;
    }

    // 2) 이동: 애니메이션 중에 누른 키는 버퍼에 저장했다가 애니메이션이 끝나면 처리한다.
    //    (연타해도 칸을 건너뛰지 않으면서 입력이 사라지지 않게 함)
    if (auto dir = ReadMoveInput()) gQueuedMove = dir;
    if (!IsAnimating() && gQueuedMove) {
        // 부수기 대기 중이면, 부수기 전의 최단 거리를 미리 구해 둔다.
        int before = -1;
        if (g.player.breakArmed) before = static_cast<int>(ShortestPath(g).size()) - 1;

        MoveResult r = TryMove(g, *gQueuedMove);
        gQueuedMove.reset();
        OnMoveResult(r);
        AnnounceMove(g, r);

        if (r.brokeWall) {
            // 벽을 부순 뒤의 최단 거리를 다시 탐색하여 변화량을 알려 준다.
            int after = static_cast<int>(ShortestPath(g).size()) - 1;
            std::string msg = "Wall broken!";
            if (before >= 0 && after >= 0) {
                msg += " Shortest path " + std::to_string(before) + " -> " + std::to_string(after);
                if (after < before) msg += " (" + std::to_string(before - after) + " shorter)";
                else msg += " (no shortcut)";
            }
            ShowMessage(s, msg, 3.5f);
        }
        if ((r.moved || r.brokeWall) && s.showGuide) RecomputeGuide(g);
    }

    // 3) 제한 시간
    if (UpdateTimer(s, dt)) OnEvent(GameEvent::TimeUp);

    // 4) 게임 종료 처리
    if (s.screen == Screen::Cleared || s.screen == Screen::GameOver) HandleGameEnd(g);
}

void MoveCursor(GameSession& s, int count, MenuInput in) {
    if (count <= 0) return;
    if (in == MenuInput::Up)   { s.menuIndex = (s.menuIndex + count - 1) % count; PlayUiSound(UiSound::Cursor); }
    if (in == MenuInput::Down) { s.menuIndex = (s.menuIndex + 1) % count; PlayUiSound(UiSound::Cursor); }
}

void RetryLevel(Game& g) {
    StartLevel(g, g.original, g.session.mode, TimeLimitFor(g.original));
    SyncPlayer(g.player.pos);
}

void UpdateMenu(Game& g, MenuInput in) {
    GameSession& s = g.session;
    switch (s.screen) {
        case Screen::Title:
            MoveCursor(s, TITLE_COUNT, in);
            if (in != MenuInput::Confirm) break;
            PlayUiSound(UiSound::Confirm);
            switch (s.menuIndex) {
                case TITLE_START:
                case TITLE_TIME_ATTACK:
                    s.mode = (s.menuIndex == TITLE_START) ? GameMode::Normal : GameMode::TimeLimit;
                    RefreshMazeStatus(g);
                    SetScreen(g, Screen::MazeSelect);
                    break;
                case TITLE_RANDOM:
                    StartNewGame(g, "", true, GameMode::Normal);
                    break;
                case TITLE_CONTINUE:
                    if (g.save.hasSave && RestoreSnapshot(g.save, g)) {
                        SyncPlayer(g.player.pos);
                        ShowMessage(s, "Game loaded");
                    } else {
                        ShowMessage(s, "No saved game");
                    }
                    break;
                case TITLE_RECORDS:
                    SetScreen(g, Screen::Records);
                    break;
                case TITLE_QUIT:
                    gQuit = true;
                    break;
            }
            break;

        case Screen::MazeSelect: {
            int count = static_cast<int>(s.mazeFiles.size());
            MoveCursor(s, count + 1, in);
            if (in == MenuInput::Back) { SetScreen(g, Screen::Title); break; }
            if (in != MenuInput::Confirm) break;
            PlayUiSound(UiSound::Confirm);
            if (s.menuIndex == count) { SetScreen(g, Screen::Title); break; }
            StartNewGame(g, MAZE_DIR + "/" + s.mazeFiles[s.menuIndex], false, s.mode);
            break;
        }

        case Screen::Records:
            if (in == MenuInput::Confirm || in == MenuInput::Back) SetScreen(g, Screen::Title);
            break;

        case Screen::Paused:
            MoveCursor(s, PAUSE_COUNT, in);
            if (in == MenuInput::Back || ReadGameCommand() == GameCommand::Pause) {
                s.screen = Screen::Playing;
                break;
            }
            if (in != MenuInput::Confirm) break;
            PlayUiSound(UiSound::Confirm);
            if (s.menuIndex == PAUSE_RESUME) s.screen = Screen::Playing;
            else if (s.menuIndex == PAUSE_SAVE) { DoSave(g); s.screen = Screen::Playing; }
            else if (s.menuIndex == PAUSE_TITLE) SetScreen(g, Screen::Title);
            break;

        case Screen::Cleared:
        case Screen::GameOver:
            MoveCursor(s, RESULT_COUNT, in);
            if (in != MenuInput::Confirm) break;
            PlayUiSound(UiSound::Confirm);
            if (s.menuIndex == RESULT_RETRY) RetryLevel(g);
            else SetScreen(g, Screen::Title);
            break;

        case Screen::Error:
            if (in == MenuInput::Confirm || in == MenuInput::Back) {
                if (s.errorReturn == Screen::MazeSelect) RefreshMazeStatus(g);
                SetScreen(g, s.errorReturn);
            }
            break;

        default:
            break;
    }
}

}  // namespace

int main() {
    // ---------------- 초기화 ----------------
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "MAZE");
    SetExitKey(KEY_NULL);  // ESC는 일시정지/뒤로가기에 사용
    SetTargetFPS(60);
    InitAudioDevice();
    InitView();

    std::error_code ec;
    std::filesystem::create_directories("data", ec);

    Game game;
    game.bestRecords = LoadRecords(RECORD_FILE);
    if (!LoadGame(SAVE_FILE, game.save)) ShowMessage(game.session, "Save file was damaged and ignored", 3.0f);
    game.session.mazeFiles = ListMazeFiles(MAZE_DIR);
    if (game.session.mazeFiles.empty())
        ShowMessage(game.session, "No mazes found: run the game from the project folder", 5.0f);
    SetScreen(game, Screen::Title);

    // ---------------- 게임 루프 ----------------
    while (!gQuit && !WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.1f);  // 창 이동 등으로 인한 시간 급증 방지
        GameSession& s = game.session;
        if (s.messageTimer > 0.0f) s.messageTimer -= dt;

        if (s.screen == Screen::Playing) {
            UpdatePlaying(game, dt);
        } else {
            gQueuedMove.reset();
            UpdateMenu(game, ReadMenuInput());
        }

        UpdateAnimation(dt);

        BeginDrawing();
        switch (s.screen) {
            case Screen::Playing:
            case Screen::Paused:
            case Screen::Cleared:
            case Screen::GameOver:
                DrawGame(game);
                DrawMenu(game);  // 일시정지/결과 화면은 게임 화면 위에 겹쳐 그림
                break;
            case Screen::Error:
                DrawError(s.lastError);
                break;
            default:
                DrawMenu(game);
                break;
        }
        EndDrawing();
    }

    // ---------------- 종료 ----------------
    ShutdownView();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
