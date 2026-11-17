cmake_minimum_required(VERSION 3.20)
project(CDModMenu LANGUAGES CXX RC)

set(MOD_NAME "CDModMenu" CACHE STRING "Output name of the mod (.asi, .ini, console, menu title)")
string(TOUPPER "${MOD_NAME}" MOD_NAME_UPPER)

# ---------------------------------------------------------------------------
# CDModMenu - internal ImGui mod menu for Crimson Desert (DirectX 12).
# Builds a CDModMenu.asi that an ASI loader injects into the game process.
# ---------------------------------------------------------------------------

if(NOT WIN32)
    message(FATAL_ERROR "CDModMenu targets Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # static CRT -> no VC++ redist needed in-game

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# --- Dear ImGui (docking branch, classic DX12 backend API) -----------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5-docking
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# --- MinHook (function detours) --------------------------------------------
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(minhook)

# --- CDModMenu ASI -----------------------------------------------------------
add_library(CDModMenu SHARED
    src/dllmain.cpp
    src/core/mod.cpp
    src/core/settings.cpp
    src/mem/scanner.cpp
    src/game/player.cpp
    src/game/teleport.cpp
    src/game/inventory.cpp
    src/game/storage_slots.cpp
    src/game/world.cpp
    src/game/dye.cpp
    src/game/equipment.cpp
    src/game/friendly.cpp
    src/game/pak.cpp
    src/hooks/dx12_hook.cpp
    src/hooks/input.cpp
    src/hooks/xinput_hook.cpp
    src/gui/framework.cpp
    src/gui/widgets.cpp
    src/gui/icons.cpp
    src/gui/menu.cpp
    src/gui/storage_sizes_page.cpp
    src/gui/stat_tuning_page.cpp
    src/game/stat_tuning.cpp
    src/game/intro_skip.cpp
    src/game/stat_drain.cpp
    src/CDModMenu.rc               # VERSIONINFO -> legitimate PE metadata for AV/ML scoring
)

target_include_directories(CDModMenu PRIVATE src)
# Resolve #include "core/version.h" from the .rc via an explicit /I for the RC compiler.
target_include_directories(CDModMenu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(CDModMenu PRIVATE imgui minhook d3d12 dxgi dwmapi imm32 xinput9_1_0)
target_compile_definitions(CDModMenu PRIVATE
    MOD_NAME="${MOD_NAME}"
    MOD_NAME_UPPER="${MOD_NAME_UPPER}"
    MOD_FILENAME="${MOD_NAME}.asi"
    MOD_DESCRIPTION="${MOD_NAME} - Crimson Desert mod menu"
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

# .asi is just a DLL with a different extension; strip the lib prefix too.
set_target_properties(CDModMenu PROPERTIES
    PREFIX ""
    SUFFIX ".asi"
    OUTPUT_NAME "${MOD_NAME}"
)

# --- Binary hardening ------------------------------------------------------
# ASLR (/DYNAMICBASE + /HIGHENTROPYVA), DEP (/NXCOMPAT) and Control Flow Guard
# make the PE look like a modern, legitimately-compiled binary. Combined with a
# code signature these markers materially improve AV/ML reputation scoring; they
# cost nothing at runtime for a mod of this size.
if(MSVC)
    target_compile_options(CDModMenu PRIVATE /guard:cf)
    target_link_options(CDModMenu PRIVATE
        /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /guard:cf)
endif()
