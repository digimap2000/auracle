# ---------------------------------------------------------------------------
# Proto / gRPC code generation — inventory
# ---------------------------------------------------------------------------
set(INVENTORY_PROTO_FILE
  "${AURACLE_PROTO_IMPORT_PATH}/auracle/inventory/v1/inventory.proto")

set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(INVENTORY_PROTO_GENERATED_SRCS
  "${PROTO_GEN_DIR}/auracle/inventory/v1/inventory.pb.cc"
  "${PROTO_GEN_DIR}/auracle/inventory/v1/inventory.pb.h"
  "${PROTO_GEN_DIR}/auracle/inventory/v1/inventory.grpc.pb.cc"
  "${PROTO_GEN_DIR}/auracle/inventory/v1/inventory.grpc.pb.h"
)

add_custom_command(
  OUTPUT  ${INVENTORY_PROTO_GENERATED_SRCS}
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROTO_GEN_DIR}/auracle/inventory/v1"
  COMMAND protobuf::protoc
          --cpp_out=${PROTO_GEN_DIR}
          --grpc_out=${PROTO_GEN_DIR}
          --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
          -I ${AURACLE_PROTO_IMPORT_PATH}
          ${INVENTORY_PROTO_FILE}
  DEPENDS ${INVENTORY_PROTO_FILE}
  COMMENT "Generating protobuf/gRPC C++ sources for inventory.proto"
)

# ---------------------------------------------------------------------------
# Proto / gRPC code generation — observation
# ---------------------------------------------------------------------------
set(OBSERVATION_PROTO_FILE
  "${AURACLE_PROTO_IMPORT_PATH}/auracle/observation/v1/observation.proto")

set(OBSERVATION_PROTO_GENERATED_SRCS
  "${PROTO_GEN_DIR}/auracle/observation/v1/observation.pb.cc"
  "${PROTO_GEN_DIR}/auracle/observation/v1/observation.pb.h"
  "${PROTO_GEN_DIR}/auracle/observation/v1/observation.grpc.pb.cc"
  "${PROTO_GEN_DIR}/auracle/observation/v1/observation.grpc.pb.h"
)

add_custom_command(
  OUTPUT  ${OBSERVATION_PROTO_GENERATED_SRCS}
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROTO_GEN_DIR}/auracle/observation/v1"
  COMMAND protobuf::protoc
          --cpp_out=${PROTO_GEN_DIR}
          --grpc_out=${PROTO_GEN_DIR}
          --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
          -I ${AURACLE_PROTO_IMPORT_PATH}
          ${OBSERVATION_PROTO_FILE}
  DEPENDS ${OBSERVATION_PROTO_FILE}
  COMMENT "Generating protobuf/gRPC C++ sources for observation.proto"
)

# Suppress warnings in generated protobuf/gRPC sources
set_source_files_properties(
  ${INVENTORY_PROTO_GENERATED_SRCS}
  ${OBSERVATION_PROTO_GENERATED_SRCS}
  PROPERTIES COMPILE_OPTIONS "-Wno-nullability-extension"
)

# ---------------------------------------------------------------------------
# auracle_rpc library
# ---------------------------------------------------------------------------
add_library(auracle_rpc
  src/inventory_service.cpp
  src/observation_service.cpp
  src/convert.cpp
  src/observation_convert.cpp
  ${INVENTORY_PROTO_GENERATED_SRCS}
  ${OBSERVATION_PROTO_GENERATED_SRCS}
)

target_link_libraries(auracle_rpc
  PUBLIC  auracle_inventory auracle_observation auracle_assigned_numbers gRPC::grpc++ protobuf::libprotobuf
)

target_include_directories(auracle_rpc
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(auracle_rpc SYSTEM
  PUBLIC ${PROTO_GEN_DIR}
)
