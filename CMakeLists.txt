cmake_minimum_required(VERSION 3.16)
project(tgui VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # GNU extensions, for the POSIX API (fork, strtok_r, ...)

# MSVC: build every target (CLI, GUI, fetched SDL3/SDL3_ttf/FreeType) against the
# static CRT (/MT). Mixing CRTs produces link errors (__imp_strtol and friends), and
# the dynamic CRT will not run on a server without the VC++ redistributable.
# FreeType's CMake declares an old policy version (3.10), so CMP0091 has to be forced
# to NEW for it to honour this setting.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Shared code plus the platform layer (src/win or src/posix)
file(GLOB COMMON_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

if(WIN32)
  enable_language(RC)
  file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/win/*.c)
  # tgui.rc embeds tgui.manifest (requireAdministrator) into the exe.
  add_executable(tgui ${COMMON_SOURCES} ${PLATFORM_SOURCES} tgui.rc)
  set_source_files_properties(tgui.rc PROPERTIES OBJECT_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tgui.manifest)
  target_compile_definitions(tgui PRIVATE _WIN32_WINNT=0x0601)
  target_link_libraries(tgui PRIVATE
    winhttp ws2_32 iphlpapi wintrust crypt32 advapi32 shell32)
elseif(UNIX)
  file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/posix/*.c)
  add_executable(tgui ${COMMON_SOURCES} ${PLATFORM_SOURCES})
else()
  message(FATAL_ERROR "Unsupported platform (Windows and Linux only).")
endif()

target_compile_definitions(tgui PRIVATE TGUI_VERSION="${PROJECT_VERSION}")
target_include_directories(tgui PRIVATE src)
if(NOT WIN32)
  target_include_directories(tgui PRIVATE src/posix)
endif()

if(MSVC)
  target_compile_options(tgui PRIVATE /W4 /utf-8)
  target_compile_definitions(tgui PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_link_options(tgui PRIVATE /MANIFEST:NO)   # avoid duplicating our own manifest
else()
  target_compile_options(tgui PRIVATE -Wall -Wextra)
  if(WIN32)
    target_link_options(tgui PRIVATE -static $<$<CONFIG:Release,MinSizeRel>:-s>)
  elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(tgui PRIVATE -s)
  endif()
endif()

install(TARGETS tgui RUNTIME DESTINATION bin)

# ---------------------------------------------------------------- GUI (optional)
# cmake -DTGUI_BUILD_GUI=ON ...  ->  tgui-gui (SDL3 + SDL3_ttf)
# External code is fetched only from the official libsdl-org repositories, at pinned
# release tags (SDL_ttf vendors FreeType through the same mirror).
# To link against an already-installed SDL3 / SDL3_ttf, use -DTGUI_USE_SYSTEM_SDL=ON
option(TGUI_BUILD_GUI "Build the SDL3 GUI (tgui-gui)" OFF)
option(TGUI_USE_SYSTEM_SDL "Use installed SDL3/SDL3_ttf instead of fetching them" OFF)

if(TGUI_BUILD_GUI)
  if(TGUI_USE_SYSTEM_SDL)
    find_package(SDL3 REQUIRED CONFIG)
    find_package(SDL3_ttf REQUIRED CONFIG)
  else()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG        release-3.4.18
      GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL3)

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)   # precomposed Hangul renders fine without HarfBuzz
    set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3_ttf
      GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
      GIT_TAG        release-3.2.2
      GIT_SHALLOW    TRUE
      GIT_SUBMODULES external/freetype)
    FetchContent_MakeAvailable(SDL3_ttf)
  endif()

  file(GLOB GUI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gui/*.c)
  if(WIN32)
    # The GUI carries the requireAdministrator manifest too, so the CLI it launches inherits the privilege.
    add_executable(tgui-gui WIN32 ${GUI_SOURCES} tgui.rc)
  else()
    add_executable(tgui-gui ${GUI_SOURCES})
  endif()
  target_compile_definitions(tgui-gui PRIVATE TGUI_VERSION="${PROJECT_VERSION}")
  target_link_libraries(tgui-gui PRIVATE SDL3_ttf::SDL3_ttf SDL3::SDL3)
  if(MSVC)
    target_compile_options(tgui-gui PRIVATE /W4 /utf-8)
    target_compile_definitions(tgui-gui PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_options(tgui-gui PRIVATE /MANIFEST:NO)
  else()
    target_compile_options(tgui-gui PRIVATE -Wall -Wextra)
    if(WIN32)
      target_link_options(tgui-gui PRIVATE -static)
    endif()
  endif()
  # The GUI runs the CLI from its own folder, so always build them together.
  add_dependencies(tgui-gui tgui)

  # UI strings live in lang/*.json next to the executable. English is compiled in,
  # so the program still runs if this folder is missing.
  add_custom_command(TARGET tgui-gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/lang" "$<TARGET_FILE_DIR:tgui-gui>/lang"
    COMMENT "Copying lang/ next to tgui-gui")

  install(TARGETS tgui-gui RUNTIME DESTINATION bin)
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lang/" DESTINATION bin/lang)
endif()
