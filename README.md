# GLFW 게임 프로젝트

창·입력·게임 루프는 GLFW 3.4, 그래픽은 OpenGL 3.3 기반이다. GLFW·GLEW 2.3.1·글꼴 래스터라이저 소스와 배포 가능한 한글 폰트를 저장소에 포함한다. 빌드 중 인터넷 다운로드나 기존 `freeglut.dll`·`glew32.dll`은 필요하지 않다.

## Windows

Visual Studio의 **C++ 데스크톱 개발**과 **C++ CMake 도구**, Windows SDK가 필요하다. 독립 CMake를 쓰면 3.24 이상이며 설치된 Visual Studio 버전을 지원해야 한다. Visual Studio 2026에서는 함께 설치된 CMake를 권장한다.

```powershell
./Tools/build.ps1 -Configuration Release -Architecture x64 -Test -Package
```

- 실행: `build/vs-x64/bin/Release/SimpleGame.exe`
- 배포: `build/vs-x64/GSWGame-1.0-Windows-x64.zip`
- Debug: `-Configuration Debug`
- 32비트: `-Architecture Win32`
- 기존 `SimpleGame.sln`도 사용할 수 있다. 프로젝트의 빌드 명령이 같은 CMake 빌드를 호출하며 F5는 새 실행 파일을 실행한다.

ZIP 전체를 풀어 실행한다. `Data`, `Shaders`를 실행 파일 옆에 유지한다. GLFW·GLEW는 정적 링크한다. Release의 MSVC 런타임은 기존과 같은 DLL 연결을 유지하며, CMake가 설치된 Visual Studio의 재배포 DLL을 찾아 실행 폴더와 ZIP에 자동으로 포함한다. 수동 DLL 복사나 별도 런타임 설치가 필요 없다. Debug 런타임은 정적으로 연결한다. 그래픽 드라이버와 운영체제의 기본 라이브러리는 필요하다. Windows의 기본 배율 처리는 기존 게임과 같다. Native DPI는 CMake의 `GAME_WINDOWS_NATIVE_DPI=ON`으로 별도 선택할 수 있다.

## Linux / macOS 및 다른 CMake 환경

```sh
cmake --preset release
cmake --build --preset release --parallel
ctest --preset release
cmake --build build/cmake-release --config Release --target package
```

Linux는 기본 X11 백엔드를 사용한다. Debian/Ubuntu의 시스템 개발 패키지는 다음과 같다.

```sh
sudo apt-get install build-essential cmake libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

Wayland가 필요하면 해당 개발 패키지를 설치하고 `-DGLFW_BUILD_WAYLAND=ON`으로 구성한다. macOS는 Xcode Command Line Tools와 CMake가 필요하며 OpenGL 4.1 core 컨텍스트를 요청한다. 창이 필요한 테스트는 디스플레이가 있는 환경에서 `-DGAME_GRAPHICS_TESTS=ON`으로 추가한다. Linux CI에서는 Xvfb 등을 사용할 수 있다.

현재 로컬 실행 검증 범위는 Windows다. Linux/macOS용 조건부 코드와 CI 빌드를 포함하지만 해당 OS에서의 실제 실행 결과와 픽셀 일치까지 보장한 것은 아니다.

## 폰트와 조작

Windows는 기존 맑은 고딕/바탕 GDI 출력을 유지한다. 해당 글꼴이 없으면 포함된 나눔고딕/나눔명조를 프로세스에만 등록한다. 다른 OS는 같은 배포 폰트를 `stb_truetype`으로 그린다. `GAME_PORTABLE_FONTS=1`로 Windows에서도 배포 폰트 경로를 시험할 수 있다. 대체 폰트는 원래 시스템 폰트와 모양·글자 폭이 다를 수 있다.

WASD 이동, Shift 달리기, Space 공격, 1/2 무기 선택, E 대화, Tab 단서, K 제1레벨, C 능력치, Esc 종료를 유지한다. 포커스를 잃으면 눌린 키를 해제해 이동이 고정되지 않게 한다.

전환 내용과 검증 범위는 [GLFW 전환 기록](docs/GLFW_MIGRATION.md), 의존성 원본과 라이선스는 [ThirdParty 안내](ThirdParty/README.md)를 참조한다.


## 지도

마을과 제1레벨에서 우측 상단 미니맵이 현재 주변 지형·건물과 플레이어(흰 마름모)를 표시한다.
M으로 전체 지도를 열고 M 또는 Esc로 닫는다. 지도 위에서 마우스 휠로 1~16배 확대·축소하고,
왼쪽 버튼 드래그로 이동한다. F는 현재 확대율에서 내 위치로, R은 전체 지도 보기로 돌아간다.
2배 이상에서 NPC(청록)와 사냥터의 적(빨강), 작은 시설이 표시되며 4배 이상에서 인물 이름과 시설명이 표시된다.
인물 표시는 실제 Actor 위치를 사용한다. 겹치는 이름은 생략한다.
전체 지도에서는 시간·이동·전투가 정지하고, 닫으면 재개한다. 대화·단서·능력치 화면은 먼저 닫아야 한다.

지도 좌표·줌 한계·화면비·입력 상태 회귀 테스트는 CTest의 `map` 항목에 포함되어 있다.
