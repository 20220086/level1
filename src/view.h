// view.h
// 렌더링 및 사운드 출력 모듈: 스프라이트, 애니메이션, 효과음, 화면 출력
// 게임 상태는 읽기 전용(const)으로만 참조한다.
#pragma once

#include "types.h"

constexpr int SCREEN_WIDTH  = 1024;
constexpr int SCREEN_HEIGHT = 768;

enum class UiSound { Cursor, Confirm, Save, BreakArm, BreakCancel };

bool InitView();      // InitWindow / InitAudioDevice 이후 호출
void ShutdownView();  // CloseAudioDevice / CloseWindow 이전 호출

void SyncPlayer(GridPos pos);                 // 레벨 시작/이어하기 시 캐릭터 위치 맞춤
void OnMoveResult(const MoveResult& result);  // 이동 애니메이션 + 효과음
void OnEvent(GameEvent e);                    // 이동과 무관한 이벤트 (시간 초과)
void PlayEventSound(GameEvent e);
void PlayUiSound(UiSound s);

void UpdateAnimation(float dt);
bool IsAnimating();

void DrawGame(const Game& game);              // 맵, 경로, 캐릭터, HUD
void DrawMenu(const Game& game);              // 타이틀, 미로 선택, 기록, 일시정지, 결과 화면
void DrawError(const LoadResult& error);      // 미로 파일 오류 화면
