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

conda 환경에서 Hershey-Fonts import가 되는지 확인합니다.

```bash
python -c "from HersheyFonts import HersheyFonts; print('Hershey-Fonts OK')"
```

그 다음 glyph library를 생성합니다.

```bash
cd robot_arm
python scripts/generate_hershey_glyph_library.py --font futural --output assets/fonts/hershey_futural_glyphs.json
```

생성된 파일은 앱의 `Hershey Glyph Library Text` 모드에서 기본 경로로 사용할 수 있습니다.

```text
../assets/fonts/hershey_futural_glyphs.json
```

앱 실행 중에는 Python script를 자동 호출하지 않습니다. CMake build 과정에서도 이 script는 실행되지 않으므로, C++ 빌드는 Hershey-Fonts Python package 설치 여부와 분리되어 있습니다.

ImGui에서 loaded source가 `fallback-no-hershey-fonts` 또는 fallback warning으로 표시되면, 현재 JSON이 실제 Hershey-Fonts 기반이 아니라 sample fallback 데이터라는 뜻입니다. 이 경우 위 명령을 Hershey-Fonts가 설치된 conda 환경에서 다시 실행하면 됩니다.

## ambientCG Paper Texture 배치

Paper preset은 ambientCG 스타일 texture를 아래 구조에서 찾습니다. 앱은 `robot_arm/build`에서 실행되므로 코드에서는 `../assets/...` 상대 경로로 로드합니다.

```text
robot_arm/assets/papers/smooth/albedo.png
robot_arm/assets/papers/smooth/normal.png
robot_arm/assets/papers/smooth/roughness.png

robot_arm/assets/papers/rough/albedo.png
robot_arm/assets/papers/rough/normal.png
robot_arm/assets/papers/rough/roughness.png

robot_arm/assets/papers/recycled/albedo.png
robot_arm/assets/papers/recycled/normal.png
robot_arm/assets/papers/recycled/roughness.png
```

- `albedo.png`: ambientCG `Color` 또는 `Albedo` map
- `normal.png`: ambientCG `NormalGL` map을 rename한 파일
- `roughness.png`: ambientCG `Roughness` map

현재 단계에서는 paper plane rendering에 albedo texture만 사용합니다. `normal.png`와 `roughness.png`는 ImGui에서 path/existence를 확인하고, 이후 PBR 확장을 위해 보관합니다. `NormalDX`를 `normal.png`로 사용하면 OpenGL normal map 기준과 green channel 방향이 달라질 수 있습니다.

structured albedo가 없으면 기존 placeholder texture로 fallback합니다.

```text
robot_arm/assets/papers/smooth_paper.png
robot_arm/assets/papers/rough_paper.png
robot_arm/assets/papers/recycled_paper.png
```

## Brush Texture 교체

Pen Preset은 stroke parameter를 결정하고, Brush Image selector는 실제 stamp에 사용할 PNG texture를 결정합니다. Pen Preset을 새로 선택하면 기본 brush image로 돌아가고, Brush Image combo에서 다른 PNG를 고르면 brush texture만 override됩니다.

기본 mapping은 아래와 같습니다.

```text
Pencil:        robot_arm/assets/brushes/chalk.png
Ballpoint Pen: robot_arm/assets/brushes/round.png
Marker:        robot_arm/assets/brushes/blob.png
Fallback:      robot_arm/assets/brushes/basic_circle.png
```

`robot_arm/assets/brushes/` 안의 PNG 파일은 Brush Image selector에서 사용할 수 있습니다. 다른 기본 brush를 쓰려면 [pen_preset.cpp](robot_arm/src/pen_preset.cpp)의 `brushTexturePath`를 바꾸면 됩니다. Brush image는 PNG, RGBA, transparent background를 권장합니다. 256x256 또는 512x512 정방형 brush tip이 가장 예측 가능하게 stamp됩니다. `square.png`, `rock.png`, `rakchalk.png`처럼 aspect ratio가 큰 이미지는 paper 위 quad에 찍힐 때 늘어나 보일 수 있습니다.

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
