add_library(vb_render STATIC mod.cpp)

target_include_directories(vb_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_render PUBLIC
    vb_base
    vb_animation
    vb_asset_server
)
