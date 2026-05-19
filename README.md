# Robot Arm Graphics Project

OpenGL, GLFW, Assimp, GLM, Dear ImGui를 사용한 로봇 팔 그래픽스 프로젝트입니다.

## 프로젝트 구조

```text
.
├── glad.c
├── robot_arm/
│   ├── CMakeLists.txt
│   ├── resources/
│   ├── shaders/
│   └── src/
└── README.md
```

## 필요 환경

- CMake 3.10 이상
- C/C++ 컴파일러
- GLFW 3.x
- GLM
- Assimp
- FreeType

Ubuntu 계열에서는 보통 아래 패키지가 필요합니다.

```bash
sudo apt install build-essential cmake pkg-config libglfw3-dev libglm-dev libassimp-dev libfreetype6-dev
```

## 빌드 방법

레포 루트에서 CMake build 디렉터리를 생성하고 빌드합니다.

```bash
cd /data/Graphics_2026/course2601-graphics-project
cmake -S robot_arm -B robot_arm/build
cmake --build robot_arm/build -j
```

`make`를 직접 사용하고 싶다면 CMake가 생성한 build 디렉터리에서 실행합니다.

```bash
cd /data/Graphics_2026/course2601-graphics-project/robot_arm/build
make -j
```

빌드가 성공하면 실행 파일은 아래 위치에 생성됩니다.

```text
robot_arm/build/main
```

## 실행 방법

셰이더와 리소스 경로가 `robot_arm/build` 기준 상대 경로로 잡혀 있으므로, build 디렉터리에서 실행합니다.

```bash
cd /data/Graphics_2026/course2601-graphics-project/robot_arm/build
./main
```

## Hershey Glyph Library 생성

Hershey-Fonts 기반 텍스트 경로를 앱에서 조합하려면, 먼저 glyph library JSON을 생성합니다.

```bash
cd robot_arm
python scripts/generate_hershey_glyph_library.py --font futural --output assets/fonts/hershey_futural_glyphs.json
```

생성된 파일은 앱의 `Hershey Glyph Library Text` 모드에서 기본 경로로 사용할 수 있습니다.

```text
../assets/fonts/hershey_futural_glyphs.json
```

## 빌드 파일 정리

빌드 산출물은 `robot_arm/build/` 아래에 생성됩니다. 이 디렉터리는 `.gitignore`에 포함되어 있으므로 새로 생성되는 빌드 파일은 git에 추가되지 않습니다.

빌드를 처음부터 다시 하고 싶으면 build 디렉터리를 지운 뒤 다시 configure/build 하면 됩니다.

```bash
rm -rf robot_arm/build
cmake -S robot_arm -B robot_arm/build
cmake --build robot_arm/build -j
```

## 참고

이 프로젝트는 수업 배포 폴더의 공용 헤더와 라이브러리를 참조합니다. Linux에서는 시스템 GLFW 헤더를 먼저 사용하고, 수업 배포 `includes`는 보조 include 경로로 사용하도록 CMake가 설정되어 있습니다.
