add_library(vb_weapon STATIC mod.c)

target_include_directories(vb_weapon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_weapon PUBLIC 
    vb_base
)