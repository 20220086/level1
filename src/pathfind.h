// pathfind.h
// 경로 탐색 및 미로 생성 모듈: BFS 최단 경로, 도달 가능성 검사, DFS 미로 생성
// Raylib에 의존하지 않는다.
#pragma once

#include "types.h"

struct SearchState {
    GridPos      pos;          // 탐색 중인 칸
    unsigned int keyMask = 0;  // 이 상태에서 소지한 열쇠 조합 (압축된 비트)
};

// from에서 출구까지의 최단 경로 (from 포함). 도달 불가면 빈 벡터.
std::vector<GridPos> FindPath(const Map& map, GridPos from,
                              const std::bitset<MAX_KEYS>& heldKeys, bool avoidTraps);

// 현재 상태에서 이동 가능한 다음 상태들
std::vector<SearchState> GetNextStates(const Map& map, const SearchState& state,
                                       bool avoidTraps, const int bitOfKey[MAX_KEYS]);

// 시작 위치에서 열쇠 없이 출발해 출구까지 갈 수 있는가 (함정은 통과 가능으로 취급)
bool IsReachable(const Map& map);

// 명시적 스택 DFS로 미로 생성 (짝수 크기는 홀수로 보정, 최소 11x11)
Map GenerateMaze(int width, int height, unsigned int seed);
