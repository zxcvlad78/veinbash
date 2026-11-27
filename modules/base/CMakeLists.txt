add_library(vb_base STATIC mod.c)

target_include_directories(vb_base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_base PUBLIC 
    raylib
    flecs::flecs_static
    lua
)