if(NOT AURACLE_BUILD_CLI)
  return()
endif()

add_executable(auracle
  src/main.cpp
  src/list.cpp
  src/watch.cpp
  src/render.cpp
)

target_link_libraries(auracle
  PRIVATE auracle_rpc
)
