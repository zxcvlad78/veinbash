add_library(vb_asset_server STATIC mod.cpp)

target_include_directories(vb_asset_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_asset_server PUBLIC
    vb_base
)
