# MAZE (C++ / raylib 6.0)

20220086 길상준

## 폴더 구조

```
maze_game/
├── build.bat              Windows(MinGW g++) 빌드 스크립트
├── src/
│   ├── types.h            공통 자료구조 (보고서 5장)
│   ├── input.h/.cpp       입력 처리 모듈
│   ├── logic.h/.cpp       게임 로직 모듈 (raylib 미사용)
│   ├── pathfind.h/.cpp    경로 탐색(BFS) 및 미로 생성(DFS) 모듈 (raylib 미사용)
│   ├── fileio.h/.cpp      파일 입출력 모듈 (raylib 미사용)
│   ├── view.h/.cpp        렌더링 및 사운드 출력 모듈
│   ├── sprites.h          32x32 픽셀 아트 데이터 (문자 1개 = 픽셀 1개)
│   └── main.cpp           메인 게임 루프
├── tests/logic_test.cpp   raylib 없이 로직만 시험하는 콘솔 프로그램
├── tools/sprites.py       sprites.h 를 만든 픽셀 아트 작성 스크립트 (선택, 빌드에는 불필요)
├── assets_template/       코드가 그리는 그림을 PNG로 뽑아 둔 것 (수정용 원본)
├── maps/                  미로 파일 (.txt)
├── assets/                직접 만든 스프라이트/효과음을 넣는 곳 (없으면 코드가 자동 생성)
└── data/                  records.txt, save.txt (실행하면 자동 생성)
```

## 빌드와 실행

1. `build.bat`을 메모장으로 열어 `set RAYLIB=...` 줄을 본인 raylib 설치 경로로 바꿉니다.
   - 공식 릴리스 zip: `raylib-6.0_win64_mingw-w64` 폴더 (안에 `include`, `lib`가 있는 폴더)
   - raylib 설치 프로그램: `C:\raylib\raylib` (안에 `src\libraylib.a`가 있는 폴더)
2. VS Code 터미널에서 프로젝트 폴더로 이동한 뒤 `.\build.bat`를 실행합니다. (또는 Ctrl+Shift+B)
3. **반드시 프로젝트 폴더에서** `.\maze.exe`를 실행합니다. `maps`, `assets`, `data` 폴더를 상대 경로로 찾기 때문입니다.

## 조작

| 키 | 기능 |
|---|---|
| W A S D | 상/좌/하/우 이동 (길게 누르면 연속 이동) |
| F | 벽 부수기 준비/취소 (게임당 1회, 캐릭터가 빛나는 동안 벽에 부딪히면 부서짐) |
| H | BFS 출구 경로 안내 켜기/끄기 |
| T | 지나온 경로 표시 켜기/끄기 |
| F5 | 현재 상태 저장 |
| P / ESC | 일시정지 |
| 메뉴 | W/S 또는 방향키로 선택, Enter/Space 확인, ESC 뒤로 |

## 미로 파일 작성 규칙 (maps 폴더의 .txt)

| 문자 | 의미 |
|---|---|
| `#` | 벽 |
| `.` | 길 |
| `S` | 시작 위치 (정확히 1개) |
| `E` | 출구 (정확히 1개) |
| `a`~`z` | 열쇠 (s, e 제외) |
| `A`~`Z` | 잠긴 문 (같은 알파벳 열쇠로 열림, S, E 제외) |
| `^` | 함정: 시작 위치로 되돌아감 |
| `*` | 함정: 이동 횟수 +5 |

- 10x10 이상, 모든 줄 길이가 같아야 합니다. 열쇠 종류는 미로당 최대 4개입니다.
- 미로 선택 화면에서 모든 파일을 자동으로 검사하며, 잘못된 파일은 빨간 글씨로 원인과 줄 번호를 보여 줍니다.
  (`zz_invalid_example.txt`는 검사 기능을 보여 주기 위한 일부러 잘못된 예제입니다.)

## 확장 기능: 벽 부수기

- 게임당 한 번, F를 누르면 캐릭터가 빛나며 부수기 준비 상태가 됩니다. F를 다시 누르면 취소됩니다.
- 준비 상태에서 벽에 부딪히면 그 벽이 무너져 길이 됩니다. 캐릭터는 제자리에 있고, 부수기 자체는 이동 횟수에 들어가지 않습니다.
- 바깥 테두리 벽과 잠긴 문은 부술 수 없습니다. (부딪혀도 준비 상태가 유지됩니다.)
- 부순 직후 BFS로 최단 거리를 다시 계산하여 "Shortest path 54 -> 31" 처럼 변화량을 보여 줍니다.
  경로 안내(H)를 켜 두면 부순 벽을 지나는 새 최단 경로가 바로 표시됩니다.
- 부수기를 사용한 판도 최고 기록에 그대로 반영됩니다.

## 직접 만든 픽셀 그래픽/효과음으로 교체하기

파일이 없으면 코드가 만든 기본 그래픽/효과음을 사용합니다. 아래 이름으로 파일을 넣으면 자동으로 교체됩니다.

- `assets/tiles.png`: 32x32 칸 13개를 가로로 배치 (416x32).
  순서: 바닥, 벽1~벽5, 시작, 출구, 열쇠, 문, 복귀 함정, 페널티 함정, 벽 잔해.
  벽은 칸마다 벽1~벽5 중 하나가 무작위로(같은 칸은 항상 같은 모양) 선택됩니다.
  열쇠와 문은 **흰색/회색으로** 그리면 열쇠 번호별 색이 자동으로 입혀집니다. 바닥 외의 칸은 배경을 투명하게 그리세요.
- `assets/player.png`: 가로 3칸(정지, 왼발, 오른발) x 세로 4줄(아래, 위, 왼쪽, 오른쪽) = 96x128.
- `assets/sfx/`: `step.wav bump.wav key.wav door.wav locked.wav trap.wav clear.wav timeup.wav cursor.wav confirm.wav save.wav`
  `break.wav arm.wav disarm.wav` (한 개만 넣어도 되고, 없는 것은 기본 효과음을 사용)

`assets_template` 폴더의 PNG가 현재 그림이므로, 이를 고쳐서 `assets` 폴더에 넣으면 됩니다.
타이틀의 한글 이름은 Windows의 맑은 고딕(`C:/Windows/Fonts/malgun.ttf`)으로 출력하며, 글꼴이 없으면 영문으로 표시합니다.

## 로직 시험

`build.bat` 실행 시 `logic_test.exe`도 함께 만들어집니다. 프로젝트 폴더에서 실행하면
미로 로드/검사, BFS 도달 가능성, 열쇠 필요 여부, 충돌 규칙, 저장/불러오기, 최고 기록, 자동 생성 미로 200개를 검사합니다.

## 자주 나는 오류

- `raylib.h: No such file or directory` → `build.bat`의 RAYLIB 경로가 잘못됨
- `undefined reference to 'InitWindow'` 등 → `-L` 경로에 `libraylib.a`가 없음 (경로 확인)
- `undefined reference to std::filesystem...` → g++ 8 이하 버전. g++ 9 이상을 쓰거나 명령 끝에 `-lstdc++fs` 추가
- 실행하면 "No mazes found" → 프로젝트 폴더가 아닌 곳에서 exe를 실행함
- 게임 화면 글자는 영어입니다. raylib 기본 글꼴에 한글이 없기 때문입니다. (타이틀의 이름만 맑은 고딕 사용)
- 이전 버전에서 저장한 세이브는 형식이 달라 자동으로 무시됩니다.
