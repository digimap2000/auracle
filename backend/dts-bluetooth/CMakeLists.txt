if(APPLE)
    set(BT_SOURCES
        src/bluetooth_macos.mm
        src/scanner_macos.mm
    )
else()
    set(BT_SOURCES src/bluetooth_stub.cpp)
endif()

add_library(auracle_dts_bluetooth ${BT_SOURCES})

target_include_directories(auracle_dts_bluetooth
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(auracle_dts_bluetooth
    PUBLIC dts::dts
)

if(APPLE)
    set_source_files_properties(
        src/bluetooth_macos.mm
        src/scanner_macos.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )
    target_link_libraries(auracle_dts_bluetooth
        PUBLIC
            "-framework IOBluetooth"
            "-framework CoreBluetooth"
            "-framework Foundation"
    )
endif()
