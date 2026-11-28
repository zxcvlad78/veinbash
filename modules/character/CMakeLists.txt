add_library(vb_character STATIC mod.cpp)

target_include_directories(vb_character PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_character PUBLIC
    vb_base
    vb_physics
)
