// fileio.h
// 파일 입출력 모듈: 미로 파일 읽기/검사, 기록 파일, 세이브 파일
// Raylib에 의존하지 않는다.
#pragma once

#include "types.h"

std::vector<std::string>   ListMazeFiles(const std::string& dir);
LoadResult                 LoadMap(const std::string& path, Map& outMap);
LoadResult                 ParseMapLines(const std::vector<std::string>& lines, Map& outMap);
LoadResult                 ValidateMap(const Map& map);
Tile                       CharToTile(char c, bool& ok);
char                       TileToChar(const Tile& t);

bool                       AppendPlayRecord(const std::string& path, const std::string& mazeName,
                                            int moves, GameMode mode, bool cleared);
std::map<std::string, int> LoadRecords(const std::string& path);

bool                       SaveGame(const std::string& path, const SaveData& data);
bool                       LoadGame(const std::string& path, SaveData& outData);
