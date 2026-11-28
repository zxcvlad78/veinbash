add_library(vb_scene STATIC mod.cpp)

target_include_directories(vb_scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_scene PUBLIC
    vb_base
)
