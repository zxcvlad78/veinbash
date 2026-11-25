cmake_minimum_required(VERSION 3.24)
project(veinbash C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

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

set(LUA_VERSION "5.4.8")
FetchContent_Declare(lua
    URL https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
endif()

set(LUA_SRC "${lua_SOURCE_DIR}/src")
file(GLOB LUA_SOURCES "${LUA_SRC}/*.c")
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC ${LUA_SRC})
if(UNIX AND NOT APPLE)
    target_link_libraries(lua PUBLIC m ${CMAKE_DL_LIBS})
endif()

file(GLOB_RECURSE ${PROJECT_NAME}_SOURCES "src/*.c")
add_executable(${PROJECT_NAME}
    ${${PROJECT_NAME}_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${PROJECT_SOURCE_DIR}/include/"
)


target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    flecs::flecs_static
    lua
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
    m dl pthread
)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data"
    COMMENT "Data copied successfully to build directory."
)
