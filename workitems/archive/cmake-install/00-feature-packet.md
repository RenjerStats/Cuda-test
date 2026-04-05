# Feature Packet

## Task

- Name: Post-MVP Phase 5 — CMake Install & FetchContent
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/cmake-install`
- Status: spec-ready
- Vision link: `workitems/active/post-mvp-vision/00-vision.md` — решает проблему P5

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §5 (практичность: простая интеграция в существующий C++/CUDA проект), §10 (технический стек: CMake)

## Objective

Добавить CMake install target и config-package, чтобы библиотеку можно было подключить через `find_package(cuda_test)`, `FetchContent`, или `add_subdirectory` без ручного копирования заголовков. Это базовое требование для практического использования внешними разработчиками.

## Scope

- In scope:
  - CMake install rules: headers из `include/`, config package file
  - `cmake/cuda_testConfig.cmake.in` — config template для `find_package`
  - Экспорт targets: `cuda_test::cuda_test` (основная библиотека), `cuda_test::testing` (GTest-зависимый модуль, опциональный)
  - `install(TARGETS ...)` + `install(DIRECTORY include/ ...)` + `install(EXPORT ...)`
  - Version file через `CMakePackageConfigHelpers`
  - Тест: пример внешнего проекта, подключающего установленный пакет через `find_package`
  - Тест: пример внешнего проекта, подключающего библиотеку через `FetchContent` (скрипт `tests/integration/cmake/fetch_content_test/`)
  - Тест: пример подключения через `add_subdirectory` (скрипт `tests/integration/cmake/subdirectory_test/`)
  - Документация в `README` секции (если README существует) или в `docs/integration.md`

- Out of scope:
  - vcpkg/Conan порты (можно добавить позже, config package — достаточная база)
  - Создание `.deb`/`.rpm`/`.msi` пакетов
  - Поддержка `pkg-config`
  - CI/CD pipeline

## Affected Areas

- Build system: `CMakeLists.txt` (root), `src/CMakeLists.txt`
- New files:
  - `cmake/cuda_testConfig.cmake.in`
  - `cmake/cuda_testConfigVersion.cmake` (генерируется)
  - `tests/integration/cmake/find_package_test/CMakeLists.txt`
  - `tests/integration/cmake/find_package_test/main.cpp`
  - `tests/integration/cmake/fetch_content_test/CMakeLists.txt`
  - `tests/integration/cmake/fetch_content_test/main.cpp`
  - `tests/integration/cmake/subdirectory_test/CMakeLists.txt`
  - `tests/integration/cmake/subdirectory_test/main.cpp`
  - `tests/integration/cmake/common/FindGTest.cmake`
- Existing files: минимальные изменения в root `CMakeLists.txt` для добавления install rules

## Interface Notes

### Target layout

```cmake
# Основная библиотека (всё кроме testing)
cuda_test::cuda_test
  - INTERFACE_INCLUDE_DIRECTORIES: include/
  - Зависимости: CUDA::cudart (если найден)

# Тестовый модуль (GTest-зависимый)
cuda_test::testing
  - INTERFACE_INCLUDE_DIRECTORIES: include/
  - Зависимости: cuda_test::cuda_test, GTest::gtest
```

### Пример подключения через FetchContent

```cmake
cmake_minimum_required(VERSION 3.26)
project(my_cuda_project LANGUAGES CXX CUDA)

include(FetchContent)
FetchContent_Declare(cuda_test
    GIT_REPOSITORY <repo_url>
    GIT_TAG v0.2.0
)
FetchContent_MakeAvailable(cuda_test)

add_executable(my_app main.cu)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

### Пример подключения через add_subdirectory

```cmake
add_subdirectory(extern/cuda_test)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

### Пример подключения через find_package (после install)

```cmake
find_package(cuda_test 0.2.0 REQUIRED)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

## Risks

- **Header-only vs compiled targets.** Библиотека header-only, но CMake target должен быть `INTERFACE` library. Если в будущем появятся `.cpp` — потребуется перевод на `STATIC`/`SHARED`. Mitigation: использовать `INTERFACE` сейчас, перевод — backward-compatible change.
- **CUDA dependency.** Не все пользователи могут иметь CUDA на этапе configure. Mitigation: `cuda_testConfig.cmake` проверяет `find_package(CUDAToolkit)` и выставляет `CUDA_TEST_CUDA_FOUND`.
- **GTest dependency для testing target.** Пользователь может не хотеть GTest. Mitigation: `cuda_test::testing` — отдельный опциональный target, не включается в `cuda_test::cuda_test`.
- **Installed package не должен навязывать GTest всем потребителям.** Mitigation: `cuda_testConfig.cmake` подгружает testing export и вызывает `find_dependency(GTest)` только при `find_package(cuda_test COMPONENTS testing)`. Базовый `find_package(cuda_test)` работает без GTest.

## Acceptance Criteria

- [ ] `cmake --install` копирует заголовки и config package в указанный prefix
- [ ] `find_package(cuda_test)` работает после install
- [ ] `target_link_libraries(app PRIVATE cuda_test::cuda_test)` предоставляет include path
- [ ] FetchContent тест: внешний проект конфигурируется и компилирует `#include <cuda_test/cuda_test.hpp>`
- [ ] add_subdirectory тест: внешний проект конфигурируется и компилирует `#include <cuda_test/cuda_test.hpp>`
- [ ] `cuda_test::testing` target доступен только при `CUDA_TEST_BUILD_TESTS=ON` или когда GTest найден
- [ ] Version check: `find_package(cuda_test 0.2.0)` проходит, `find_package(cuda_test 999.0)` — нет
- [ ] Existing build presets (`CMakePresets.json`) продолжают работать без изменений
