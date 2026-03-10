add_library(auracle_inventory
  src/registry.cpp
  src/probe_scheduler.cpp
  providers/src/serial_provider.cpp
  providers/src/host_bluetooth_provider.cpp
  providers/src/mdns_provider.cpp
  probers/src/serial_agent_prober.cpp
  probers/src/host_bluetooth_prober.cpp
  probers/src/mdns_agent_prober.cpp
)

target_include_directories(auracle_inventory
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/..   # consumers include as <inventory/registry.hpp>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}      # internal sources include as "registry.hpp"
)

target_link_libraries(auracle_inventory
  PUBLIC dts::dts auracle_dts_bluetooth
)
