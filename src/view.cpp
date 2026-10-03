// view.cpp
// 렌더링 및 사운드 출력 모듈
// - 픽셀 그래픽: sprites.h의 문자 배열(32x32)을 한 점씩 찍어 텍스처로 만든다.
//   assets/tiles.png, assets/player.png 가 있으면 그 파일을 대신 사용한다.
// - 효과음: 파형을 코드로 합성한다. assets/sfx/*.wav 가 있으면 그 파일을 대신 사용한다.
#include "view.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "raylib.h"
#include "sprites.h"

namespace {

// ---------------------------------------------------------------------------
// 상수
// ---------------------------------------------------------------------------
constexpr int   HUD_H      = 72;     // 상단 정보 표시줄 높이
constexpr int   FOOTER_H   = 32;     // 하단 도움말 높이
constexpr float MOVE_TIME  = 0.12f;  // 한 칸 이동 애니메이션 시간
constexpr float BUMP_TIME  = 0.10f;  // 벽 충돌 흔들림 애니메이션 시간
constexpr float SHAKE_TIME = 0.15f;
constexpr float FLASH_TIME = 0.35f;

const char* GAME_TITLE  = "MAZE";
const char* AUTHOR_TEXT = "20220086 길상준";        // 한글 글꼴이 있으면 이 문구를 출력
const char* AUTHOR_FALLBACK = "20220086 Kil Sang-jun";  // 한글 글꼴이 없을 때

const Color BG_COLOR    = {16, 18, 20, 255};
const Color PANEL_COLOR = {24, 28, 26, 238};
const Color ACCENT      = {236, 196, 80, 255};
const Color MOSS_GREEN  = {120, 180, 90, 255};
const Color DIM_TEXT    = {150, 156, 146, 255};
const Color GLOW_COLOR  = {255, 214, 110, 255};

// 효과음 순서
enum Sfx { SFX_STEP, SFX_BUMP, SFX_KEY, SFX_DOOR, SFX_LOCKED, SFX_TRAP, SFX_CLEAR, SFX_TIMEUP,
           SFX_CURSOR, SFX_CONFIRM, SFX_SAVE, SFX_BREAK, SFX_ARM, SFX_DISARM, SFX_COUNT };
const char* SFX_FILES[SFX_COUNT] = {
    "assets/sfx/step.wav",   "assets/sfx/bump.wav",    "assets/sfx/key.wav",   "assets/sfx/door.wav",
    "assets/sfx/locked.wav", "assets/sfx/trap.wav",    "assets/sfx/clear.wav", "assets/sfx/timeup.wav",
    "assets/sfx/cursor.wav", "assets/sfx/confirm.wav", "assets/sfx/save.wav",  "assets/sfx/break.wav",
    "assets/sfx/arm.wav",    "assets/sfx/disarm.wav"};

const Color KEY_COLORS[6] = {{250, 210, 60, 255}, {80, 200, 240, 255},  {240, 100, 160, 255},
                             {140, 220, 80, 255}, {245, 140, 50, 255}, {170, 120, 250, 255}};

// ---------------------------------------------------------------------------
// 상태 (이 모듈 내부에서만 관리)
// ---------------------------------------------------------------------------
struct PlayerAnim {
    Vector2   pos{0, 0};   // 현재 그리는 위치 (격자 단위, 소수)
    Vector2   from{0, 0};
    Vector2   to{0, 0};
    float     t = 1.0f;    // 진행도 0~1 (1이면 정지)
    bool      bump = false;
    bool      leftFoot = false;
    Direction facing = Direction::Down;
};

struct Particle {          // 벽이 부서질 때 튀는 돌 조각
    Vector2 pos;           // 격자 단위
    Vector2 vel;
    float   life;
    float   maxLife;
    float   size;          // 칸 크기 대비 비율
    Color   color;
};

Texture2D  gTiles{};
Texture2D  gPlayer{};
Font       gNameFont{};
bool       gHasNameFont = false;
Sound      gSfx[SFX_COUNT]{};
bool       gSfxLoaded[SFX_COUNT]{};
PlayerAnim gAnim;
float      gShake = 0.0f;
float      gShakePower = 4.0f;
float      gFlash = 0.0f;
Color      gFlashColor = WHITE;
std::vector<Particle> gParticles;
unsigned int gRand = 2463534242u;

float Rand01() {  // 화면 효과용 간단한 난수 (xorshift)
    gRand ^= gRand << 13; gRand ^= gRand >> 17; gRand ^= gRand << 5;
    return (gRand & 0xFFFFFF) / static_cast<float>(0x1000000);
}

Color KeyColor(int id) { return id < 0 ? WHITE : KEY_COLORS[id % 6]; }

// ---------------------------------------------------------------------------
// 효과음 합성 (wav 파일이 없을 때 사용)
// ---------------------------------------------------------------------------
enum WaveShape { W_SQUARE, W_TRIANGLE, W_SAW, W_NOISE };
struct Tone {
    float f0, f1, dur, vol;
    WaveShape shape;
};

Sound Synth(const std::vector<Tone>& tones) {
    const int RATE = 44100;
    std::vector<short> buf;
    unsigned int seed = 12345u;
    for (const Tone& t : tones) {
        int n = static_cast<int>(t.dur * RATE);
        double phase = 0.0;
        for (int i = 0; i < n; ++i) {
            float k = static_cast<float>(i) / n;
            float f = t.f0 + (t.f1 - t.f0) * k;
            phase += f / RATE;
            phase -= std::floor(phase);
            float v = 0.0f;
            switch (t.shape) {
                case W_SQUARE:   v = phase < 0.5 ? 1.0f : -1.0f; break;
                case W_TRIANGLE: v = 4.0f * std::fabs(static_cast<float>(phase) - 0.5f) - 1.0f; break;
                case W_SAW:      v = 2.0f * static_cast<float>(phase) - 1.0f; break;
                case W_NOISE:
                    seed = seed * 1103515245u + 12345u;
                    v = ((seed >> 16) & 0x7FFF) / 16383.5f - 1.0f;
                    break;
            }
            float attack = std::min(1.0f, i / (0.004f * RATE));
            float env = attack * (1.0f - k);
            buf.push_back(static_cast<short>(v * t.vol * env * 12000.0f));
        }
    }
    Wave w{};
    w.frameCount = static_cast<unsigned int>(buf.size());
    w.sampleRate = RATE;
    w.sampleSize = 16;
    w.channels   = 1;
    w.data       = buf.data();
    return LoadSoundFromWave(w);  // 데이터를 복사하므로 buf는 함수 종료 시 해제되어도 된다
}

Sound MakeDefaultSfx(int id) {
    switch (id) {
        case SFX_STEP:    return Synth({{190, 140, 0.045f, 0.35f, W_TRIANGLE}});
        case SFX_BUMP:    return Synth({{95, 55, 0.08f, 0.55f, W_SQUARE}, {1, 1, 0.05f, 0.25f, W_NOISE}});
        case SFX_KEY:     return Synth({{880, 880, 0.06f, 0.35f, W_SQUARE}, {1320, 1320, 0.12f, 0.35f, W_SQUARE}});
        case SFX_DOOR:    return Synth({{320, 190, 0.20f, 0.45f, W_SAW}});
        case SFX_LOCKED:  return Synth({{160, 160, 0.07f, 0.5f, W_SQUARE}, {1, 1, 0.04f, 0.0f, W_SQUARE},
                                        {150, 150, 0.08f, 0.5f, W_SQUARE}});
        case SFX_TRAP:    return Synth({{700, 90, 0.35f, 0.45f, W_SQUARE}});
        case SFX_CLEAR:   return Synth({{523, 523, 0.10f, 0.35f, W_SQUARE}, {659, 659, 0.10f, 0.35f, W_SQUARE},
                                        {784, 784, 0.10f, 0.35f, W_SQUARE}, {1047, 1047, 0.30f, 0.35f, W_SQUARE}});
        case SFX_TIMEUP:  return Synth({{240, 110, 0.60f, 0.45f, W_SAW}});
        case SFX_CURSOR:  return Synth({{620, 620, 0.03f, 0.22f, W_SQUARE}});
        case SFX_CONFIRM: return Synth({{700, 1050, 0.08f, 0.28f, W_SQUARE}});
        case SFX_SAVE:    return Synth({{660, 660, 0.06f, 0.35f, W_TRIANGLE}, {990, 990, 0.10f, 0.35f, W_TRIANGLE}});
        case SFX_BREAK:   return Synth({{1, 1, 0.10f, 0.70f, W_NOISE}, {130, 40, 0.28f, 0.60f, W_SAW},
                                        {1, 1, 0.15f, 0.25f, W_NOISE}});
        case SFX_ARM:     return Synth({{440, 1320, 0.18f, 0.30f, W_TRIANGLE}, {1320, 1320, 0.06f, 0.20f, W_TRIANGLE}});
        case SFX_DISARM:  return Synth({{1100, 420, 0.14f, 0.25f, W_TRIANGLE}});
    }
    return Sound{};
}

void PlaySfx(int id) {
    if (id >= 0 && id < SFX_COUNT && gSfxLoaded[id]) PlaySound(gSfx[id]);
}

// ---------------------------------------------------------------------------
// 픽셀 그래픽: sprites.h 의 문자 배열을 이미지로 변환
// ---------------------------------------------------------------------------
Color PaletteColor(char c) {
    for (const PaletteEntry& e : SPRITE_PALETTE)
        if (e.ch == c) return {e.r, e.g, e.b, 255};
    return BLANK;
}

void PaintPixels(Image& img, int ox, int oy, const char* const rows[SPRITE_PX]) {
    for (int y = 0; y < SPRITE_PX; ++y)
        for (int x = 0; x < SPRITE_PX; ++x) {
            char c = rows[y][x];
            if (c != '.') ImageDrawPixel(&img, ox + x, oy + y, PaletteColor(c));
        }
}

// tiles.png: 32x32 칸 T_COUNT개를 가로로 배치
Image BuildTileSheet() {
    Image img = GenImageColor(SPRITE_PX * T_COUNT, SPRITE_PX, BLANK);
    for (int i = 0; i < T_COUNT; ++i) PaintPixels(img, i * SPRITE_PX, 0, TILE_PIXELS[i]);
    return img;
}

// player.png: 가로 3칸(정지, 왼발, 오른발) x 세로 4줄(아래, 위, 왼쪽, 오른쪽)
Image BuildPlayerSheet() {
    Image img = GenImageColor(SPRITE_PX * 3, SPRITE_PX * 4, BLANK);
    for (int i = 0; i < 12; ++i) PaintPixels(img, (i % 3) * SPRITE_PX, (i / 3) * SPRITE_PX, PLAYER_PIXELS[i]);
    return img;
}

Texture2D LoadOrBuild(const char* file, Image (*builder)()) {
    Texture2D tex{};
    if (FileExists(file)) tex = LoadTexture(file);
    if (tex.id == 0) {
        Image img = builder();
        tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);  // 픽셀 아트가 흐려지지 않도록
    return tex;
}

void LoadNameFont() {
    const char* candidates[] = {"C:/Windows/Fonts/malgun.ttf", "C:/Windows/Fonts/malgunbd.ttf",
                                "C:/Windows/Fonts/NanumGothic.ttf", "assets/font.ttf"};
    int count = 0;
    int* codepoints = LoadCodepoints(AUTHOR_TEXT, &count);
    for (const char* path : candidates) {
        if (!FileExists(path)) continue;
        gNameFont = LoadFontEx(path, 40, codepoints, count);
        if (IsFontValid(gNameFont) && gNameFont.texture.id != 0) {
            SetTextureFilter(gNameFont.texture, TEXTURE_FILTER_BILINEAR);
            gHasNameFont = true;
            break;
        }
    }
    UnloadCodepoints(codepoints);
}

// ---------------------------------------------------------------------------
// 그리기 보조
// ---------------------------------------------------------------------------
struct Layout {
    float cell = 32.0f;
    float ox = 0.0f, oy = 0.0f;
};

Layout ComputeLayout(const Map& m) {
    const float areaX = 16.0f, areaY = HUD_H + 10.0f;
    const float areaW = SCREEN_WIDTH - 32.0f;
    const float areaH = SCREEN_HEIGHT - HUD_H - FOOTER_H - 20.0f;
    float cell = std::min(areaW / m.width, areaH / m.height);
    // 32픽셀 스프라이트가 고르게 보이도록 32의 배수(작으면 16의 배수)로 맞춘다
    if (cell >= 32.0f) cell = std::floor(cell / 32.0f) * 32.0f;
    else if (cell >= 16.0f) cell = 16.0f;
    else cell = std::floor(cell);
    cell = std::clamp(cell, 4.0f, 64.0f);
    Layout L;
    L.cell = cell;
    L.ox = std::floor(areaX + (areaW - cell * m.width) / 2.0f);
    L.oy = std::floor(areaY + (areaH - cell * m.height) / 2.0f);
    return L;
}

void DrawTileSprite(int index, Rectangle dest, Color tint) {
    float fw = gTiles.width / static_cast<float>(T_COUNT);
    float fh = static_cast<float>(gTiles.height);
    DrawTexturePro(gTiles, {index * fw, 0, fw, fh}, dest, {0, 0}, 0.0f, tint);
}

// 벽마다 1~5번 블록 중 하나를 고른다. 같은 칸은 항상 같은 블록이 되도록 좌표와 미로 이름으로 해시한다.
int WallVariant(int x, int y, unsigned int seed) {
    unsigned int h = seed ^ (static_cast<unsigned int>(x) * 73856093u) ^ (static_cast<unsigned int>(y) * 19349663u);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return T_WALL1 + static_cast<int>(h % WALL_VARIANTS);
}

void DrawPlayerSprite(Direction facing, int col, Rectangle dest, Color tint) {
    int row = 0;
    switch (facing) {
        case Direction::Down:  row = 0; break;
        case Direction::Up:    row = 1; break;
        case Direction::Left:  row = 2; break;
        case Direction::Right: row = 3; break;
    }
    float fw = gPlayer.width / 3.0f, fh = gPlayer.height / 4.0f;
    DrawTexturePro(gPlayer, {col * fw, row * fh, fw, fh}, dest, {0, 0}, 0.0f, tint);
}

void DrawCentered(const char* text, int y, int size, Color c) {
    DrawText(text, (SCREEN_WIDTH - MeasureText(text, size)) / 2, y, size, c);
}

void DrawShadowText(const char* text, int x, int y, int size, Color c) {
    DrawText(text, x + 4, y + 4, size, {0, 0, 0, 170});
    DrawText(text, x, y, size, c);
}

void DrawMenuList(const std::vector<std::string>& labels, const std::vector<bool>& enabled, int selected, int y,
                  int spacing, int size) {
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        const char* text = labels[i].c_str();
        int w = MeasureText(text, size);
        int x = (SCREEN_WIDTH - w) / 2;
        int yy = y + i * spacing;
        bool on = enabled.empty() || enabled[i];
        if (i == selected) {
            DrawRectangle(x - 40, yy - 6, w + 80, size + 12, Fade(ACCENT, 0.15f));
            DrawText(">", x - 30, yy, size, ACCENT);
            DrawText("<", x + w + 16, yy, size, ACCENT);
        }
        Color c = !on ? Color{86, 92, 84, 255} : (i == selected ? ACCENT : RAYWHITE);
        DrawText(text, x, yy, size, c);
    }
}

void DrawTileBackground() {
    ClearBackground(BG_COLOR);
    const float cell = 64.0f;
    int cols = SCREEN_WIDTH / static_cast<int>(cell) + 1;
    int rows = SCREEN_HEIGHT / static_cast<int>(cell) + 1;
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x) {
            bool wall = (x * 31 + y * 17) % 5 == 0 || y == 0 || y == rows - 1;
            Rectangle r = {x * cell, y * cell, cell, cell};
            DrawTileSprite(T_FLOOR, r, WHITE);
            if (wall) DrawTileSprite(WallVariant(x, y, 99u), r, WHITE);
        }
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.62f));
}

void DrawPanel(int w, int h, int y) {
    int x = (SCREEN_WIDTH - w) / 2;
    DrawRectangle(x, y, w, h, PANEL_COLOR);
    DrawRectangleLinesEx({(float)x, (float)y, (float)w, (float)h}, 3.0f, {86, 120, 70, 255});
}

void DrawFooter(const char* text) {
    DrawRectangle(0, SCREEN_HEIGHT - FOOTER_H, SCREEN_WIDTH, FOOTER_H, {12, 14, 14, 255});
    DrawText(text, 16, SCREEN_HEIGHT - FOOTER_H + 8, 18, DIM_TEXT);
}

void DrawMessage(const GameSession& s) {
    if (s.messageTimer <= 0.0f || s.message.empty()) return;
    float a = std::min(1.0f, s.messageTimer / 0.3f);
    int w = MeasureText(s.message.c_str(), 22) + 40;
    int x = (SCREEN_WIDTH - w) / 2;
    int y = SCREEN_HEIGHT - FOOTER_H - 60;
    DrawRectangle(x, y, w, 40, Fade({8, 10, 10, 255}, 0.88f * a));
    DrawRectangleLines(x, y, w, 40, Fade(ACCENT, a));
    DrawText(s.message.c_str(), x + 20, y + 9, 22, Fade(RAYWHITE, a));
}

std::string ModeLabel(GameMode m) { return m == GameMode::TimeLimit ? "TIME ATTACK" : "NORMAL"; }

void DrawHud(const Game& game) {
    const GameSession& s = game.session;
    const Player& pl = game.player;
    DrawRectangle(0, 0, SCREEN_WIDTH, HUD_H, {20, 24, 22, 255});
    DrawRectangle(0, HUD_H - 2, SCREEN_WIDTH, 2, {70, 100, 60, 255});

    // 미로 이름과 모드
    DrawText(game.map.name.c_str(), 16, 12, 20, RAYWHITE);
    DrawText(ModeLabel(s.mode).c_str(), 16, 42, 18, s.mode == GameMode::TimeLimit ? Color{240, 110, 90, 255} : DIM_TEXT);

    // 이동 횟수와 최고 기록
    DrawText(TextFormat("MOVES %d", pl.moveCount), 250, 10, 28, ACCENT);
    if (game.map.generated) {
        DrawText("BEST (random: no record)", 250, 46, 16, DIM_TEXT);
    } else {
        auto it = game.bestRecords.find(game.map.name);
        if (it != game.bestRecords.end()) DrawText(TextFormat("BEST  %d", it->second), 250, 44, 18, DIM_TEXT);
        else DrawText("BEST  --", 250, 44, 18, DIM_TEXT);
    }

    // 소지 열쇠
    DrawText("KEYS", 470, 10, 18, DIM_TEXT);
    int shown = 0;
    for (int id = 0; id < MAX_KEYS; ++id) {
        if (!pl.keys.test(id)) continue;
        DrawTileSprite(T_KEY, {466.0f + shown * 30.0f, 30.0f, 32.0f, 32.0f}, KeyColor(id));
        ++shown;
    }
    if (shown == 0) DrawText("-", 474, 38, 22, DIM_TEXT);

    // 벽 부수기 상태
    DrawText("BREAK [F]", 630, 10, 18, DIM_TEXT);
    if (pl.breakUsed) {
        DrawText("USED", 630, 36, 24, {110, 112, 108, 255});
    } else if (pl.breakArmed) {
        Color c = std::fmod(GetTime(), 0.5) < 0.25 ? GLOW_COLOR : RAYWHITE;
        DrawText("ARMED!", 630, 36, 24, c);
    } else {
        DrawText("READY", 630, 36, 24, MOSS_GREEN);
    }

    // 시간
    if (s.mode == GameMode::TimeLimit) {
        bool danger = s.timeRemaining <= 10.0f;
        Color c = danger ? Color{240, 80, 70, 255} : Color{110, 220, 140, 255};
        if (danger && std::fmod(GetTime(), 0.5) < 0.25) c = RAYWHITE;
        DrawText(TextFormat("TIME %.1f", s.timeRemaining), 790, 10, 24, c);
        float ratio = s.timeLimit > 0 ? s.timeRemaining / s.timeLimit : 0.0f;
        DrawRectangle(790, 42, 210, 14, {44, 50, 46, 255});
        DrawRectangle(790, 42, static_cast<int>(210 * ratio), 14, c);
    } else {
        DrawText(TextFormat("TIME %.1f", s.elapsed), 790, 10, 24, DIM_TEXT);
    }
}

void SpawnBreakParticles(GridPos p) {
    const Color stones[4] = {{92, 98, 92, 255}, {118, 124, 114, 255}, {146, 150, 136, 255}, {70, 116, 52, 255}};
    for (int i = 0; i < 26; ++i) {
        Particle pt;
        pt.pos = {p.x + 0.5f + (Rand01() - 0.5f) * 0.6f, p.y + 0.5f + (Rand01() - 0.5f) * 0.6f};
        float ang = Rand01() * 6.2832f;
        float spd = 1.5f + Rand01() * 4.0f;
        pt.vel = {std::cos(ang) * spd, std::sin(ang) * spd - 2.5f};
        pt.maxLife = pt.life = 0.45f + Rand01() * 0.45f;
        pt.size = 0.06f + Rand01() * 0.10f;
        pt.color = stones[static_cast<int>(Rand01() * 4) % 4];
        gParticles.push_back(pt);
    }
}

}  // namespace

// ===========================================================================
// 공개 함수
// ===========================================================================
bool InitView() {
    gTiles  = LoadOrBuild("assets/tiles.png", BuildTileSheet);
    gPlayer = LoadOrBuild("assets/player.png", BuildPlayerSheet);
    LoadNameFont();

    if (IsAudioDeviceReady()) {
        for (int i = 0; i < SFX_COUNT; ++i) {
            gSfx[i] = FileExists(SFX_FILES[i]) ? LoadSound(SFX_FILES[i]) : MakeDefaultSfx(i);
            if (!IsSoundValid(gSfx[i])) gSfx[i] = MakeDefaultSfx(i);
            gSfxLoaded[i] = IsSoundValid(gSfx[i]);
        }
    }
    return gTiles.id != 0 && gPlayer.id != 0;
}

void ShutdownView() {
    for (int i = 0; i < SFX_COUNT; ++i)
        if (gSfxLoaded[i]) UnloadSound(gSfx[i]);
    if (gHasNameFont) UnloadFont(gNameFont);
    if (gTiles.id != 0) UnloadTexture(gTiles);
    if (gPlayer.id != 0) UnloadTexture(gPlayer);
}

void SyncPlayer(GridPos pos) {
    Vector2 p = {static_cast<float>(pos.x), static_cast<float>(pos.y)};
    gAnim.pos = gAnim.from = gAnim.to = p;
    gAnim.t = 1.0f;
    gAnim.bump = false;
    gAnim.facing = Direction::Down;
    gParticles.clear();
}

void OnMoveResult(const MoveResult& r) {
    gAnim.facing = r.dir;
    if (r.moved && !r.teleported) {
        gAnim.from = {static_cast<float>(r.from.x), static_cast<float>(r.from.y)};
        gAnim.to   = {static_cast<float>(r.to.x), static_cast<float>(r.to.y)};
        gAnim.pos  = gAnim.from;
        gAnim.t    = 0.0f;
        gAnim.bump = false;
        gAnim.leftFoot = !gAnim.leftFoot;
    } else if (r.moved && r.teleported) {  // 복귀형 함정: 애니메이션 없이 즉시 이동
        Vector2 p = {static_cast<float>(r.to.x), static_cast<float>(r.to.y)};
        gAnim.pos = gAnim.from = gAnim.to = p;
        gAnim.t = 1.0f;
        gFlash = FLASH_TIME;
        gFlashColor = {175, 95, 235, 255};
    } else {  // 벽, 잠긴 문, 부수기: 제자리에서 박치기 동작
        gAnim.from = gAnim.to = gAnim.pos;
        gAnim.t = 0.0f;
        gAnim.bump = true;
        gShake = SHAKE_TIME;
        gShakePower = r.brokeWall ? 9.0f : 4.0f;
    }

    for (GameEvent e : r.events) {
        PlayEventSound(e);
        if (e == GameEvent::TrapTriggered && !r.teleported) { gFlash = FLASH_TIME; gFlashColor = {240, 70, 60, 255}; }
        if (e == GameEvent::ReachedExit) { gFlash = FLASH_TIME; gFlashColor = ACCENT; }
        if (e == GameEvent::WallBroken) {
            SpawnBreakParticles(r.breakPos);
            gFlash = FLASH_TIME * 0.6f;
            gFlashColor = GLOW_COLOR;
        }
    }
}

void OnEvent(GameEvent e) {
    PlayEventSound(e);
    if (e == GameEvent::TimeUp) { gFlash = FLASH_TIME; gFlashColor = {240, 70, 60, 255}; }
}

void PlayEventSound(GameEvent e) {
    switch (e) {
        case GameEvent::Moved:         PlaySfx(SFX_STEP); break;
        case GameEvent::BumpWall:      PlaySfx(SFX_BUMP); break;
        case GameEvent::KeyPicked:     PlaySfx(SFX_KEY); break;
        case GameEvent::DoorOpened:    PlaySfx(SFX_DOOR); break;
        case GameEvent::DoorLocked:    PlaySfx(SFX_LOCKED); break;
        case GameEvent::TrapTriggered: PlaySfx(SFX_TRAP); break;
        case GameEvent::ReachedExit:   PlaySfx(SFX_CLEAR); break;
        case GameEvent::TimeUp:        PlaySfx(SFX_TIMEUP); break;
        case GameEvent::WallBroken:    PlaySfx(SFX_BREAK); break;
        case GameEvent::BreakBlocked:  PlaySfx(SFX_BUMP); break;
    }
}

void PlayUiSound(UiSound s) {
    switch (s) {
        case UiSound::Cursor:      PlaySfx(SFX_CURSOR); break;
        case UiSound::Confirm:     PlaySfx(SFX_CONFIRM); break;
        case UiSound::Save:        PlaySfx(SFX_SAVE); break;
        case UiSound::BreakArm:    PlaySfx(SFX_ARM); break;
        case UiSound::BreakCancel: PlaySfx(SFX_DISARM); break;
    }
}

void UpdateAnimation(float dt) {
    gShake = std::max(0.0f, gShake - dt);
    gFlash = std::max(0.0f, gFlash - dt);
    if (gAnim.t < 1.0f) {
        gAnim.t = std::min(1.0f, gAnim.t + dt / (gAnim.bump ? BUMP_TIME : MOVE_TIME));
        gAnim.pos.x = gAnim.from.x + (gAnim.to.x - gAnim.from.x) * gAnim.t;
        gAnim.pos.y = gAnim.from.y + (gAnim.to.y - gAnim.from.y) * gAnim.t;
    }
    for (Particle& p : gParticles) {
        p.life -= dt;
        p.vel.y += 9.0f * dt;  // 중력
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
    }
    gParticles.erase(std::remove_if(gParticles.begin(), gParticles.end(),
                                    [](const Particle& p) { return p.life <= 0.0f; }),
                     gParticles.end());
}

bool IsAnimating() { return gAnim.t < 1.0f; }

void DrawGame(const Game& game) {
    const Map& m = game.map;
    const GameSession& s = game.session;
    const Player& pl = game.player;
    ClearBackground(BG_COLOR);
    if (m.CellCount() <= 0) return;

    Layout L = ComputeLayout(m);
    if (gShake > 0.0f) {
        float k = gShakePower * (gShake / SHAKE_TIME);
        L.ox += std::sin(GetTime() * 90.0) * k;
        L.oy += std::cos(GetTime() * 70.0) * k * 0.5f;
    }
    const unsigned int seed = static_cast<unsigned int>(std::hash<std::string>{}(m.name));

    DrawRectangle((int)L.ox - 6, (int)L.oy - 6, (int)(m.width * L.cell) + 12, (int)(m.height * L.cell) + 12,
                  {8, 10, 10, 255});

    // 1) 타일
    for (int y = 0; y < m.height; ++y) {
        for (int x = 0; x < m.width; ++x) {
            const Tile& t = m.At({x, y});
            Rectangle r = {L.ox + x * L.cell, L.oy + y * L.cell, L.cell, L.cell};
            DrawTileSprite(T_FLOOR, r, WHITE);
            if (t.type == TileType::Wall) { DrawTileSprite(WallVariant(x, y, seed), r, WHITE); continue; }
            if (pl.brokenAt == GridPos{x, y}) DrawTileSprite(T_RUBBLE, r, WHITE);  // 부순 벽의 잔해
            switch (t.type) {
                case TileType::Start:       DrawTileSprite(T_START, r, WHITE); break;
                case TileType::Exit:        DrawTileSprite(T_EXIT, r, WHITE); break;
                case TileType::Key:         DrawTileSprite(T_KEY, r, KeyColor(t.id)); break;
                case TileType::Door:        DrawTileSprite(T_DOOR, r, KeyColor(t.id)); break;
                case TileType::TrapReset:   DrawTileSprite(T_TRAP_RESET, r, WHITE); break;
                case TileType::TrapPenalty: DrawTileSprite(T_TRAP_PENALTY, r, WHITE); break;
                default: break;
            }
            // 2) 지나온 경로
            if (s.showTrail && !game.trail.visited.empty() && game.trail.visited[m.Index({x, y})]) {
                float d = L.cell * 0.22f;
                DrawRectangleRec({r.x + (L.cell - d) / 2, r.y + (L.cell - d) / 2, d, d}, Fade(SKYBLUE, 0.40f));
            }
        }
    }

    // 3) BFS 안내 경로
    if (s.showGuide) {
        for (size_t i = 1; i < s.guidePath.size(); ++i) {
            GridPos p = s.guidePath[i];
            float d = L.cell * 0.30f;
            float a = 0.55f + 0.3f * std::sin(static_cast<float>(GetTime()) * 6.0f - i * 0.5f);
            DrawRectangleRec({L.ox + p.x * L.cell + (L.cell - d) / 2, L.oy + p.y * L.cell + (L.cell - d) / 2, d, d},
                             Fade(YELLOW, a));
        }
    }

    // 4) 캐릭터
    Vector2 pos = gAnim.pos;
    if (gAnim.bump && gAnim.t < 1.0f) {
        GridPos d = {0, 0};
        switch (gAnim.facing) {
            case Direction::Up: d = {0, -1}; break;
            case Direction::Down: d = {0, 1}; break;
            case Direction::Left: d = {-1, 0}; break;
            case Direction::Right: d = {1, 0}; break;
        }
        float k = std::sin(gAnim.t * PI) * 0.22f;
        pos.x += d.x * k;
        pos.y += d.y * k;
    }
    Rectangle pr = {L.ox + pos.x * L.cell, L.oy + pos.y * L.cell, L.cell, L.cell};
    Vector2 center = {pr.x + L.cell / 2, pr.y + L.cell / 2};
    float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 7.0f);

    if (pl.breakArmed) {  // 부수기 대기: 캐릭터 주변이 빛남
        DrawCircleGradient(center, L.cell * (0.85f + 0.12f * pulse), Fade(GLOW_COLOR, 0.35f + 0.25f * pulse),
                           Fade(GLOW_COLOR, 0.0f));
    }
    DrawEllipse((int)center.x, (int)(pr.y + L.cell * 0.94f), L.cell * 0.26f, L.cell * 0.07f, Fade(BLACK, 0.4f));
    int col = (gAnim.t < 1.0f && !gAnim.bump) ? (gAnim.leftFoot ? 1 : 2) : 0;
    DrawPlayerSprite(gAnim.facing, col, pr, WHITE);
    if (pl.breakArmed) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawPlayerSprite(gAnim.facing, col, pr, Fade(GLOW_COLOR, 0.30f + 0.30f * pulse));
        EndBlendMode();
        for (int i = 0; i < 4; ++i) {  // 주위를 도는 반짝이
            float a = static_cast<float>(GetTime()) * 3.0f + i * 1.5708f;
            float rr = L.cell * 0.55f;
            float sz = std::max(2.0f, L.cell / 16.0f);
            DrawRectangleRec({center.x + std::cos(a) * rr - sz / 2, center.y + std::sin(a) * rr * 0.8f - sz / 2, sz, sz},
                             Fade(WHITE, 0.6f + 0.4f * pulse));
        }
    }

    // 5) 벽 파편
    for (const Particle& p : gParticles) {
        float a = std::clamp(p.life / p.maxLife, 0.0f, 1.0f);
        float sz = std::max(2.0f, L.cell * p.size);
        DrawRectangleRec({L.ox + p.pos.x * L.cell - sz / 2, L.oy + p.pos.y * L.cell - sz / 2, sz, sz}, Fade(p.color, a));
    }

    // 6) 화면 효과, HUD, 도움말, 알림
    if (gFlash > 0.0f) DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(gFlashColor, 0.40f * gFlash / FLASH_TIME));
    DrawHud(game);
    DrawFooter(TextFormat("WASD Move   F Break   H Guide %s   T Trail %s   F5 Save   P/ESC Pause",
                          s.showGuide ? "[ON]" : "[OFF]", s.showTrail ? "[ON]" : "[OFF]"));
    DrawMessage(s);
}

void DrawMenu(const Game& game) {
    const GameSession& s = game.session;
    switch (s.screen) {
        case Screen::Title: {
            DrawTileBackground();
            DrawShadowText(GAME_TITLE, (SCREEN_WIDTH - MeasureText(GAME_TITLE, 120)) / 2, 70, 120, ACCENT);
            if (gHasNameFont) {
                Vector2 sz = MeasureTextEx(gNameFont, AUTHOR_TEXT, 26, 1);
                DrawTextEx(gNameFont, AUTHOR_TEXT, {(SCREEN_WIDTH - sz.x) / 2, 205}, 26, 1, DIM_TEXT);
            } else {
                DrawCentered(AUTHOR_FALLBACK, 208, 20, DIM_TEXT);
            }
            DrawMenuList({"START GAME", "TIME ATTACK", "RANDOM MAZE", "CONTINUE", "RECORDS", "QUIT"},
                         {true, true, true, game.save.hasSave, true, true}, s.menuIndex, 280, 52, 30);
            DrawMessage(s);
            DrawFooter("W/S : select    ENTER : confirm");
            break;
        }
        case Screen::MazeSelect: {
            DrawTileBackground();
            DrawShadowText("SELECT MAZE", 60, 40, 40, ACCENT);
            DrawText(ModeLabel(s.mode).c_str(), 64, 90, 20, s.mode == GameMode::TimeLimit ? Color{240, 110, 90, 255} : DIM_TEXT);
            int count = static_cast<int>(s.mazeFiles.size());
            if (count == 0) DrawText("No maze files (.txt) in the maps folder.", 64, 150, 22, RAYWHITE);
            for (int i = 0; i <= count; ++i) {
                int y = 140 + i * 44;
                bool sel = (i == s.menuIndex);
                if (sel) DrawRectangle(48, y - 8, SCREEN_WIDTH - 96, 40, Fade(ACCENT, 0.15f));
                if (i == count) { DrawText("< BACK", 64, y, 24, sel ? ACCENT : RAYWHITE); break; }
                DrawText(s.mazeFiles[i].c_str(), 64, y, 24, sel ? ACCENT : RAYWHITE);
                const std::string& st = i < (int)s.mazeStatus.size() ? s.mazeStatus[i] : std::string("?");
                bool ok = st == "OK";
                std::string right = st;
                if (ok) {
                    auto it = game.bestRecords.find(s.mazeFiles[i]);
                    right = it != game.bestRecords.end() ? TextFormat("OK   best %d", it->second) : "OK   best --";
                }
                if (right.size() > 52) right = right.substr(0, 49) + "...";
                DrawText(right.c_str(), 420, y + 4, 18, ok ? Color{110, 220, 140, 255} : Color{240, 90, 80, 255});
            }
            DrawFooter("W/S : select    ENTER : play    ESC : back    (add your own .txt mazes to the maps folder)");
            break;
        }
        case Screen::Records: {
            DrawTileBackground();
            DrawShadowText("BEST RECORDS", 60, 40, 40, ACCENT);
            DrawText("fewest moves per maze", 64, 90, 20, DIM_TEXT);
            int i = 0;
            for (const auto& kv : game.bestRecords) {
                DrawText(kv.first.c_str(), 64, 150 + i * 40, 24, RAYWHITE);
                DrawText(TextFormat("%d moves", kv.second), 600, 150 + i * 40, 24, ACCENT);
                ++i;
            }
            if (i == 0) DrawText("No records yet. Clear a maze first!", 64, 150, 24, RAYWHITE);
            DrawFooter("ENTER / ESC : back");
            break;
        }
        case Screen::Paused: {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.6f));
            DrawPanel(420, 300, 220);
            DrawCentered("PAUSED", 250, 40, ACCENT);
            DrawMenuList({"RESUME", "SAVE GAME", "QUIT TO TITLE"}, {}, s.menuIndex, 330, 52, 28);
            break;
        }
        case Screen::Cleared:
        case Screen::GameOver: {
            bool cleared = s.screen == Screen::Cleared;
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.6f));
            DrawPanel(520, 380, 180);
            DrawCentered(cleared ? "MAZE CLEARED!" : "TIME UP!", 205, 44, cleared ? ACCENT : Color{240, 90, 80, 255});
            DrawCentered(TextFormat("Moves : %d", game.player.moveCount), 275, 26, RAYWHITE);
            DrawCentered(game.player.breakUsed ? "Wall break : used" : "Wall break : not used", 310, 20, DIM_TEXT);
            if (game.map.generated) {
                DrawCentered("(random maze is not recorded)", 340, 20, DIM_TEXT);
            } else {
                auto it = game.bestRecords.find(game.map.name);
                DrawCentered(it != game.bestRecords.end() ? TextFormat("Best : %d", it->second) : "Best : --", 340, 22,
                             DIM_TEXT);
            }
            if (cleared && s.newRecord && std::fmod(GetTime(), 0.6) < 0.4) DrawCentered("NEW RECORD!", 373, 28, {110, 220, 140, 255});
            DrawMenuList({"RETRY", "TITLE"}, {}, s.menuIndex, 425, 50, 28);
            break;
        }
        default:
            break;
    }
}

void DrawError(const LoadResult& err) {
    DrawTileBackground();
    DrawPanel(760, 320, 200);
    DrawCentered("MAZE ERROR", 230, 40, {240, 90, 80, 255});

    // 긴 오류 문구는 단어 단위로 줄바꿈
    std::vector<std::string> lines;
    std::string cur, word;
    auto flush = [&]() { if (!cur.empty()) { lines.push_back(cur); cur.clear(); } };
    for (size_t i = 0; i <= err.error.size(); ++i) {
        char c = i < err.error.size() ? err.error[i] : ' ';
        if (c == ' ') {
            std::string test = cur.empty() ? word : cur + " " + word;
            if (MeasureText(test.c_str(), 22) > 680) { flush(); cur = word; } else cur = test;
            word.clear();
        } else {
            word += c;
        }
    }
    flush();
    int y = 300;
    for (const std::string& l : lines) { DrawCentered(l.c_str(), y, 22, RAYWHITE); y += 30; }
    if (err.line > 0) DrawCentered(TextFormat("Line %d", err.line), y + 6, 22, ACCENT);
    DrawCentered("Press ENTER to go back", 470, 20, DIM_TEXT);
}
