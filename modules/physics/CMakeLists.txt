add_library(vb_physics STATIC mod.cpp)

target_include_directories(vb_physics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_physics PUBLIC
    vb_base
)
