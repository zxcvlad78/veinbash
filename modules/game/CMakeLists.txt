add_library(vb_game STATIC mod.c)

target_include_directories(vb_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_game PUBLIC 
    vb_base
    vb_animation
    vb_asset_server
    vb_character
    vb_render
    vb_scene
    vb_weapon
)