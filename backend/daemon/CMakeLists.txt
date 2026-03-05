add_executable(auracle-daemon
  src/main.cpp
)

target_link_libraries(auracle-daemon
  PRIVATE auracle_rpc
)
