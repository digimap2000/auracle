add_library(auracle_inventory
  src/registry.cpp
  providers/src/serial_provider.cpp
  probers/src/serial_agent_prober.cpp
)

target_include_directories(auracle_inventory
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/..   # consumers include as <inventory/registry.hpp>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}      # internal sources include as "registry.hpp"
)

target_link_libraries(auracle_inventory
  PUBLIC dts::dts
)
