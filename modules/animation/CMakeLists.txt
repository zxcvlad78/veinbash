add_library(vb_animation STATIC mod.c)

target_include_directories(vb_animation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_animation PUBLIC 
    vb_base
)