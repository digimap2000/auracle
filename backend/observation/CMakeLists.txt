add_library(auracle_observation
    src/scanner_manager.cpp
    src/local_bluetooth_scanner.cpp
    src/remote_wire_scanner.cpp
)

target_include_directories(auracle_observation
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(auracle_observation
    PUBLIC auracle_inventory auracle_dts_bluetooth dts::dts
)
