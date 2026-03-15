cmake_minimum_required(VERSION 3.20)
project(EmpireTotalStrategy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Dependencies ──────────────────────────────────────────────
# You'll need to install these via vcpkg or manually:
#   vcpkg install glfw3 glm glad stb
# 
# If using vcpkg, pass: -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake

find_package(glfw3 3.3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# GLM: newer vcpkg versions use CONFIG mode and the target is glm::glm-header-only
find_package(glm CONFIG REQUIRED)

# glad - if installed via vcpkg it should be findable, otherwise we bundle it
find_package(glad CONFIG QUIET)

# ─── Source files ──────────────────────────────────────────────
set(CORE_SOURCES
    src/core/Game.cpp
    src/core/InputManager.cpp
    src/core/ResourceManager.cpp
)

set(CAMPAIGN_SOURCES
    src/campaign/CampaignMap.cpp
    src/campaign/Province.cpp
    src/campaign/Faction.cpp
    src/campaign/Army.cpp
    src/campaign/Economy.cpp
    src/campaign/TurnManager.cpp
)

set(BATTLE_SOURCES
    src/battle/BattleScene.cpp
    src/battle/Unit.cpp
    src/battle/BattleResolver.cpp
)

set(RENDERING_SOURCES
    src/rendering/Renderer.cpp
    src/rendering/Camera.cpp
    src/rendering/Shader.cpp
    src/rendering/MapRenderer.cpp
    src/rendering/UIRenderer.cpp
)

set(UI_SOURCES
    src/ui/UIManager.cpp
    src/ui/ProvincePanel.cpp
    src/ui/ArmyPanel.cpp
    src/ui/TopBar.cpp
)

set(UTIL_SOURCES
    src/utils/Logger.cpp
    src/utils/MathUtils.cpp
)

# ─── Executable ────────────────────────────────────────────────
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${CORE_SOURCES}
    ${CAMPAIGN_SOURCES}
    ${BATTLE_SOURCES}
    ${RENDERING_SOURCES}
    ${UI_SOURCES}
    ${UTIL_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    OpenGL::GL
)

# GLM: vcpkg may expose glm::glm or glm::glm-header-only depending on version
if(TARGET glm::glm-header-only)
    target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm-header-only)
elseif(TARGET glm::glm)
    target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
else()
    message(FATAL_ERROR "GLM target not found. Make sure glm is installed via vcpkg.")
endif()

if(glad_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE glad::glad)
else()
    message(WARNING "glad not found via find_package. You may need to add it manually.")
endif()

# ─── Copy assets to build directory ───────────────────────────
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:${PROJECT_NAME}>/data
)
