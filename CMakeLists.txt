cmake_minimum_required(VERSION 3.21)

project(ConflictLongFront LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ConflictLongFront
    src/main.cpp
    src/core/Application.cpp
    src/core/Time.cpp
    src/core/Camera2D.cpp
    src/sim/World.cpp
    src/sim/Systems.cpp
    src/render/Renderer2D.cpp
    src/ui/DebugUI.cpp
)

target_include_directories(ConflictLongFront PRIVATE src)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(ConflictLongFront PRIVATE
    SDL3::SDL3
    imgui::imgui
    EnTT::EnTT
    glm::glm
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

if (MSVC)
    target_compile_options(ConflictLongFront PRIVATE /W4 /permissive-)
else()
    target_compile_options(ConflictLongFront PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET ConflictLongFront POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/data
            $<TARGET_FILE_DIR:ConflictLongFront>/data
)

if (WIN32)
    add_custom_command(TARGET ConflictLongFront POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ConflictLongFront>
                $<TARGET_FILE_DIR:ConflictLongFront>
        COMMAND_EXPAND_LISTS
    )
endif()

