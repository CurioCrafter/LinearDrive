cmake_minimum_required(VERSION 3.24)

project(LinearDriveNative LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_MP3 ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_OGG ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_GLTF ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip
)
FetchContent_MakeAvailable(raylib)

add_executable(LinearDrive
  src/main.cpp
  src/engine/Engine.cpp
  src/engine/Assets.cpp
  src/engine/AudioSystem.cpp
  src/engine/Renderer.cpp
  src/game/Simulation.cpp
  src/game/WorldGenerator.cpp
  src/game/SmokeTest.cpp
)

target_include_directories(LinearDrive PRIVATE src)
target_compile_definitions(LinearDrive PRIVATE NOMINMAX _USE_MATH_DEFINES)
target_link_libraries(LinearDrive PRIVATE raylib)

if (MSVC)
  target_compile_options(LinearDrive PRIVATE /W4 /permissive-)
else()
  target_compile_options(LinearDrive PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (MINGW)
  target_link_options(LinearDrive PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_custom_command(TARGET LinearDrive POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:LinearDrive>/assets"
)

install(TARGETS LinearDrive RUNTIME DESTINATION .)
install(DIRECTORY assets DESTINATION .)
