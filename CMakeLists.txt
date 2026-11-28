
cmake_minimum_required(VERSION 3.24)

project(veinbash C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
include(ExternalProject)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.2
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(raylib)

set(FLECS_SHARED OFF CACHE BOOL "" FORCE)
set(FLECS_STATIC ON CACHE BOOL "" FORCE)
set(FLECS_TESTS OFF CACHE BOOL "" FORCE)
set(FLECS_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG v4.2.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(flecs)

set(LUAJIT_REVISION "3bfb8fa4ab970b36d42d108ead04e92341c49998")
set(LUAJIT_PREFIX "${CMAKE_BINARY_DIR}/_deps/luajit-prefix")
set(LUAJIT_SOURCE_DIR "${LUAJIT_PREFIX}/src/luajit_external")
set(LUAJIT_SRC_DIR "${LUAJIT_SOURCE_DIR}/src")

if(MSVC)
    set(LUAJIT_LIBRARY "${LUAJIT_SRC_DIR}/lua51.lib")
    set(LUAJIT_DLL "${LUAJIT_SRC_DIR}/lua51.dll")

    find_program(VSWHERE_EXECUTABLE
        NAMES vswhere.exe vswhere
        HINTS
            "$ENV{ProgramFiles}/../Program Files (x86)/Microsoft Visual Studio/Installer"
            "$ENV{ProgramFiles}/Microsoft Visual Studio/Installer"
    )

    if(NOT VSWHERE_EXECUTABLE)
        message(FATAL_ERROR
            "Visual Studio vswhere.exe not found"
        )
    endif()

    execute_process(
        COMMAND "${VSWHERE_EXECUTABLE}"
            -latest
            -products "*"
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
            -property installationPath

        OUTPUT_VARIABLE VS_INSTALL_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE VSWHERE_RESULT
    )

    if(NOT VSWHERE_RESULT EQUAL 0 OR NOT VS_INSTALL_DIR)
        message(FATAL_ERROR
            "Visual Studio C++ toolchain not found"
        )
    endif()

    set(VSDEVCMD
        "${VS_INSTALL_DIR}/Common7/Tools/VsDevCmd.bat"
    )

    if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64" OR CMAKE_VS_PLATFORM_NAME MATCHES "ARM64")
        set(LUAJIT_ARCH arm64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(LUAJIT_ARCH x86)
    else()
        set(LUAJIT_ARCH x64)
    endif()

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
    set(LUAJIT_BUILD_SCRIPT "${CMAKE_BINARY_DIR}/_deps/build_luajit.cmd")

    file(TO_NATIVE_PATH "${VSDEVCMD}" VSDEVCMD_NATIVE)
    file(TO_NATIVE_PATH "${LUAJIT_SRC_DIR}" LUAJIT_SRC_DIR_NATIVE)
    file(TO_NATIVE_PATH "${LUAJIT_BUILD_SCRIPT}" LUAJIT_BUILD_SCRIPT_NATIVE)
    file(WRITE "${LUAJIT_BUILD_SCRIPT}"
        "@echo off\r\n"
        "call \"${VSDEVCMD_NATIVE}\" -no_logo -arch=${LUAJIT_ARCH} -host_arch=x64 || exit /b 1\r\n"
        "cd /d \"${LUAJIT_SRC_DIR_NATIVE}\" || exit /b 1\r\n"
        "call msvcbuild.bat || exit /b 1\r\n"
    )

    ExternalProject_Add(luajit_external
        PREFIX "${LUAJIT_PREFIX}"
        GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
        GIT_TAG "${LUAJIT_REVISION}"
        GIT_SHALLOW FALSE
        UPDATE_DISCONNECTED TRUE
        CONFIGURE_COMMAND ""
        BUILD_COMMAND "${LUAJIT_BUILD_SCRIPT}"
        BUILD_BYPRODUCTS
            "${LUAJIT_LIBRARY}"
            "${LUAJIT_DLL}"
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE TRUE
        USES_TERMINAL_DOWNLOAD TRUE
        USES_TERMINAL_BUILD TRUE
    )
elseif(WIN32)
    find_program(LUAJIT_MAKE
        NAMES mingw32-make make
        REQUIRED
    )
    set(LUAJIT_LIBRARY "${LUAJIT_SRC_DIR}/libluajit.a")

    ExternalProject_Add(luajit_external
        PREFIX "${LUAJIT_PREFIX}"

        GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
        GIT_TAG "${LUAJIT_REVISION}"
        GIT_SHALLOW FALSE
        UPDATE_DISCONNECTED TRUE
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            "${CMAKE_COMMAND}" -E chdir
            "${LUAJIT_SRC_DIR}"
            "${LUAJIT_MAKE}" BUILDMODE=static
        BUILD_BYPRODUCTS "${LUAJIT_LIBRARY}"
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE TRUE
        USES_TERMINAL_DOWNLOAD TRUE
        USES_TERMINAL_BUILD TRUE
    )
elseif(UNIX)
    find_program(LUAJIT_MAKE
        NAMES gmake make
        REQUIRED
    )
    set(LUAJIT_LIBRARY "${LUAJIT_SRC_DIR}/libluajit.a")

    ExternalProject_Add(luajit_external
        PREFIX "${LUAJIT_PREFIX}"
        GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
        GIT_TAG "${LUAJIT_REVISION}"
        GIT_SHALLOW FALSE
        UPDATE_DISCONNECTED TRUE
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            "${CMAKE_COMMAND}" -E chdir
            "${LUAJIT_SRC_DIR}"
            "${LUAJIT_MAKE}" BUILDMODE=static
        BUILD_BYPRODUCTS "${LUAJIT_LIBRARY}"
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE TRUE
        USES_TERMINAL_DOWNLOAD TRUE
        USES_TERMINAL_BUILD TRUE
    )
else()
    message(FATAL_ERROR "Unsupported platform for LuaJIT")
endif()

add_library(lua INTERFACE)

target_include_directories(lua SYSTEM INTERFACE
    "${LUAJIT_SRC_DIR}"
)

if(MSVC)
    target_link_libraries(lua INTERFACE
        "${LUAJIT_LIBRARY}"
    )
elseif(WIN32)
    target_link_libraries(lua INTERFACE
        "${LUAJIT_LIBRARY}"
        m
    )
elseif(UNIX)
    target_link_libraries(lua INTERFACE
        "${LUAJIT_LIBRARY}"
        m
    )
    if(CMAKE_DL_LIBS)
        target_link_libraries(lua INTERFACE
            "${CMAKE_DL_LIBS}"
        )
    endif()
endif()

add_dependencies(lua luajit_external)

set(CMAKE_UNITY_BUILD ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 16)

if(MSVC)
    add_compile_options(/FI"${CMAKE_SOURCE_DIR}/include/types.h")
else()
    add_compile_options(-include "${CMAKE_SOURCE_DIR}/include/types.h")
endif()

add_subdirectory(modules)

add_executable(${PROJECT_NAME}
    "src/main.cpp"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${PROJECT_SOURCE_DIR}/include/"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    vb_base
    vb_animation
    vb_asset_server
    vb_character
    vb_render
    vb_scene
    vb_weapon
    vb_game
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        m
        dl
        pthread
    )
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data"
    COMMENT "Data copied successfully to build directory."
)

if(MSVC)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LUAJIT_DLL}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/lua51.dll"

        COMMENT "LuaJIT runtime copied successfully."
    )
endif()
