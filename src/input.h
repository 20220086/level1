// input.h
// 입력 처리 모듈: 키보드 입력을 이동 방향 / 게임 명령 / 메뉴 입력으로 변환한다.
#pragma once

#include <optional>

#include "types.h"

std::optional<Direction> ReadMoveInput();   // W/A/S/D (길게 누르면 연속 이동)
GameCommand              ReadGameCommand(); // P/ESC 일시정지, F5 저장, H 경로 안내, T 경로 표시, F 벽 부수기
MenuInput                ReadMenuInput();   // W/S/방향키 이동, Enter/Space 확인, ESC/Backspace 뒤로
