# freeglut → GLFW 전환

## 빌드와 의존성

`SimpleGame.cpp`의 freeglut 호출을 GLFW로 교체했다. OpenGL 로더·렌더링·데이터 기반 게임 시스템은 유지한다. GLFW 3.4, GLEW 2.3.1의 소스와 라이선스를 `ThirdParty`에 포함하고 정적으로 빌드한다. MSVC Release 런타임은 기존 `/MD`를 유지하고, CMake의 `InstallRequiredSystemLibraries`로 해당 Visual Studio/아키텍처에 맞는 재배포 DLL을 자동 복사·패키징한다. 정적 `/MT` 비교 빌드에서 이 장면의 CPU 비용과 초기화 시간이 증가했기 때문이다. Debug는 `/MTd`로 연결한다. `GAME_STATIC_RUNTIME=ON`으로 정적 Release도 선택할 수 있다. 게임을 빌드할 때 별도 라이브러리 다운로드나 사전 생성된 GLFW/GLEW `.lib`를 요구하지 않는다.

루트 CMake가 기준 빌드다. 기존 솔루션은 NMake 진입점으로 CMake를 호출하고 새 실행 경로를 디버거에 전달한다. `v143`·`v145` 중 하나를 소스에 고정하지 않는다. 컴파일러와 운영체제 SDK, OpenGL 드라이버는 개발 환경에서 제공해야 한다. 실행 파일 옆에 데이터·셰이더·폰트·라이선스를 자동 복사하며 설치·ZIP 생성에서 로그와 생성 캐시는 제외한다. 임의 작업 폴더에서 실행해도 실행 파일 옆의 자산을 찾는다.

## 루프·입력·해상도

| 이전 | 변경 후 |
|---|---|
| glutMainLoop / Idle / Display | glfwPollEvents → Update → Render → glfwSwapBuffers |
| glutGet elapsed milliseconds | glfwGetTime의 단조 시간, dt 상한 0.05초 유지 |
| glutIgnoreKeyRepeat | GLFW_REPEAT 무시, 연속 공격은 기존 게임 상태로 처리 |
| ASCII 키와 modifiers | 물리 키를 기존 소문자/숫자/제어 키로 매핑, Shift 양쪽 지원 |
| 키 해제 이벤트 | 키 해제와 포커스 상실 시 held 상태 초기화 |
| glutLeaveMainLoop | close flag 후 게임·GL 자원 해제, 창 파괴, glfwTerminate |
| 가상화된 논리 창 크기 | 논리 해상도와 실제 framebuffer 크기 분리 |

OpenGL 3.3 이상을 요구하고 Windows/Linux는 종전처럼 드라이버 기본 프로필을 유지한다. macOS만 4.1 core/forward-compatible로 구성한다. VSync를 새로 강제하지 않도록 swap interval 0을 명시한다. 기존 depth 정렬·메시 인스턴스 형식·색·블렌딩·월드 배치·대사·전투 수치를 바꾸지 않는다.

Windows 기본 빌드는 실행 파일 manifest에 기존 freeglut 게임과 같은 DPI-unaware 방식을 명시한다. 따라서 200% 화면에서도 OpenGL은 종전의 1024×640을 그리고 Windows가 표시 크기를 확대한다. 네 배의 셰이더 작업이나 별도 확대 복사 비용을 추가하지 않는다. GLFW 원본 코드는 수정하지 않는다. 이 동작은 기존의 화면 선명도와 창 크기를 보존하기 위한 선택이다.

`-DGAME_WINDOWS_NATIVE_DPI=ON`으로 Windows의 native DPI를 선택하거나 Retina 등의 framebuffer가 논리 크기보다 큰 환경에서는 논리 크기의 RGBA8 framebuffer에 렌더링한 뒤 선형 blit한다. 월드 시야와 HUD 크기를 유지하며 과도한 셰이더 작업을 피하지만 최종 확대 복사 비용은 추가된다. 두 크기가 같으면 별도 framebuffer/blit은 사용하지 않는다. 최소화 중에는 표시를 생략하고 짧게 이벤트를 기다린다. 게임 갱신은 유지한다.

## 폰트

문자열 캐시, R8 배열 아틀라스, 1px gutter와 텍스트 배치는 유지한다. GDI 래스터라이징을 `FontRasterizer`로 분리했으며 Windows의 기존 시스템 글꼴·굵기·앤티앨리어싱·2px 패딩 설정을 그대로 쓴다. 시스템 글꼴이 없을 때만 함께 배포한 나눔 글꼴을 `FR_PRIVATE`로 등록하며 종료 시 해제한다. 운영체제 폰트를 설치하거나 시스템 설정을 바꾸지 않는다.

Linux/macOS와 강제 portable 모드는 포함된 나눔고딕/나눔명조 Regular·Bold를 stb_truetype으로 래스터라이징한다. UTF-8→wide 변환은 UTF-16/UTF-32와 잘못된 입력의 대체 문자를 처리한다. 폰트 자체가 달라지는 fallback/portable 경로의 외형과 글자 폭은 맑은 고딕·바탕과 동일하다고 보장하지 않는다. Windows 기본 경로는 샘플 한글·영문·숫자의 4개 face/bold 조합에서 이전 GDI 코드와 메트릭·coverage 픽셀을 비교한다.

## 플랫폼 코드와 진단

환경 변수·프로세스 ID·UTC 로그 경로·작업 디렉터리·메모리 조회를 `Platform`으로 모았다. Windows 모니터링은 기존 WinHTTP를 유지하고 POSIX는 백그라운드 스레드에서 타임아웃이 있는 loopback HTTP를 사용한다. 요청은 계속 127.0.0.1로만 보내며 리다이렉트를 따라가지 않는다. Linux의 private 메모리 값은 RSS에서 공유 페이지를 뺀 근사치이며 Windows private commit과 같은 의미는 아니다.

`window.backend`, `window.resize`의 논리 크기/framebuffer 크기, `fonts.backend`를 이벤트로 기록한다. 다음 진단 환경 변수는 기본 실행에는 영향을 주지 않는다.

| 변수 | 용도 |
|---|---|
| GAME_SMOKE_SECONDS | 0 초과 120 이하일 때 자동 종료 실행 검사 |
| GAME_SMOKE_SCENE=level1 | 실행 검사용 제1레벨 진입 |
| GAME_SMOKE_CAPTURE | 8초 이후 GL backbuffer를 PPM 파일로 한 번 저장 |
| GAME_PORTABLE_FONTS=1 | Windows에서도 배포 폰트 경로 사용 |

## 검증 범위

CTest는 플레이어 공격·보행, 40명 주민 시뮬레이션/소문/사냥, 정렬·조명 일치, 폰트, 키 매핑·반복·Shift·포커스 해제를 검사한다. `GAME_GRAPHICS_TESTS=ON`은 실제 튜토리얼·제1레벨 창을 띄워 GL 오류 0, 도형·텍스트 제출과 정상 종료를 확인한다. GUI 입력 전체·퀘스트 완주·장시간 전투 검사를 대체하지 않는다.

Windows x64 Release/Debug와 Win32 Release를 로컬에서 빌드·검사한다. Linux/macOS는 조건부 구현과 CI 정의를 포함하지만 이 Windows PC에서 해당 OS의 실제 실행까지 검증한 것은 아니다. 성능 비교는 같은 Release·기본 위치·논리 창 크기·40명 주민·동일 로깅 조건으로 진행하며 첫 초기화·상태 전환·캡처 프레임을 구분한다. 다음 실측은 Windows 11 / AMD Radeon 780M / x64 Release / 200% 화면 배율 / 논리 1024×640 / VSync off / 40명 주민 / 상세 프레임 로그·GPU timer ON / 모니터링 OFF 조건이다. 두 실행을 연속으로 수행하고 튜토리얼 PLAY 진입 후 2~12초의 정지 장면을 비교했다. 캡처 없는 실행이며 최소화·대화 전환이 없는 구간이다.

참조: [GLFW 소스 빌드](https://www.glfw.org/docs/3.4/compile_guide.html), [키 반복·포커스 입력](https://www.glfw.org/docs/3.4/input_guide.html), [창·framebuffer·배율](https://www.glfw.org/docs/3.4/window_guide.html), [CMake 연결](https://www.glfw.org/docs/3.4/build_guide.html).

Windows DPI 정책: [Microsoft 프로세스 DPI 설정](https://learn.microsoft.com/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process).

## 최종 실행 결과 (2026-11-04)

| 항목 | 기존 freeglut | 최종 GLFW 배포본 |
|---|---:|---:|
| 평균 FPS (1000 / 평균 프레임 간격) | 220.15 | 215.70 |
| 평균 프레임 간격 | 4.542 ms | 4.636 ms |
| 프레임 간격 p95 | 5.342 ms | 5.640 ms |
| Render CPU | 4.184 ms | 4.215 ms |
| GPU elapsed | 0.631 ms | 0.606 ms |
| 도형 / 텍스트 draw calls | 1 / 1 | 1 / 1 |
| 방문 노드 / 제출 Actor | 3685 / 1152 | 3685 / 1152 |

FPS는 약 2.0% 낮고 평균 프레임 간격은 약 0.094ms 길었다. GPU·도형 수·배치 수는 같은 수준이며 별도 반복 실행에서는 CPU 부하에 따라 편차가 있었다. 따라서 이 측정을 성능이 모든 환경에서 완전히 같다는 보장으로 해석하지 않는다. 장시간 이동·전투·서로 다른 GPU의 프레임 지연은 추가 실기 검증 범위다. 이 비교를 이유로 주민 수나 그래픽 품질을 낮추지 않았다.

초기 정적 CRT + native DPI 비교에서 늘어난 비용은 기본 Windows 배율 정책을 복원하고 기존 Release CRT 연결을 유지해 줄였다. 정적 CRT 비교 빌드에서는 캐시가 있는 튜토리얼 초기화가 약 4.45초였고, 기존/최종 DLL CRT 빌드에서는 약 2.2~2.6초였다. 이는 이 PC와 해당 실행에 대한 결과이며 일반적인 CRT 성능 우열을 뜻하지 않는다.

- x64 Release: CTest 7/7 통과 (실제 튜토리얼·제1레벨 OpenGL 포함).
- x64 Debug: CTest 7/7 통과. Win32 Release: CTest 5/5 통과.
- 기존 `SimpleGame.sln`의 Release/x64 MSBuild 진입점 빌드 성공.
- ZIP을 한글·공백 폴더에 풀어 다른 작업 폴더에서 실행. PATH에 기존 게임 DLL 경로를 넣지 않고 정상 종료, GL 오류 0, 도형·텍스트 제출 확인.
- 배포본에서 `GAME_PORTABLE_FONTS=1`로 제1레벨 실행 성공. 기본 GDI와 portable 모두 한글을 래스터라이징하고 그린다.
- ZIP에는 GLFW/GLEW DLL·개발 테스트 실행 파일·로그·생성 메시 캐시를 넣지 않는다. 필요한 MSVC 재배포 DLL, 네 개 TTF와 OFL, 셰이더·데이터·의존성 라이선스를 포함한다.
- GL backbuffer 실제 출력에서 성채·계단·나무·플레이어·한글 HUD를 확인했다. 전체 게임 화면의 픽셀 동일성 검사는 수행하지 않았다. GDI 폰트의 픽셀 일치는 별도 샘플 단위 테스트로 검사했다.

로컬 검증 로그와 원본 수치는 `SimpleGame/Data/Logs/visual-qa/glfw-final-*.log`, `glfw-final-performance.json`에 남긴다. 게임 실행 로그는 각 실행 폴더의 `Data/Logs`에 기록한다.
