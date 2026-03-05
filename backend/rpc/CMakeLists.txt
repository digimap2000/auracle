# ---------------------------------------------------------------------------
# Proto / gRPC code generation
# ---------------------------------------------------------------------------
set(PROTO_FILE
  "${AURACLE_PROTO_IMPORT_PATH}/auracle/inventory/v1/inventory.proto")

set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(PROTO_GENERATED_SRCS
  "${PROTO_GEN_DIR}/auracle/inventory/v1/inventory.pb.cc"
  "${PROTO_GEN_DIR}/auracle/inventory/v1/inventory.pb.h"
  "${PROTO_GEN_DIR}/auracle/inventory/v1/inventory.grpc.pb.cc"
  "${PROTO_GEN_DIR}/auracle/inventory/v1/inventory.grpc.pb.h"
)

add_custom_command(
  OUTPUT  ${PROTO_GENERATED_SRCS}
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROTO_GEN_DIR}/auracle/inventory/v1"
  COMMAND protobuf::protoc
          --cpp_out=${PROTO_GEN_DIR}
          --grpc_out=${PROTO_GEN_DIR}
          --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
          -I ${AURACLE_PROTO_IMPORT_PATH}
          ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  COMMENT "Generating protobuf/gRPC C++ sources for inventory.proto"
)

# Suppress warnings in generated protobuf/gRPC sources
set_source_files_properties(${PROTO_GENERATED_SRCS} PROPERTIES
  COMPILE_OPTIONS "-Wno-nullability-extension"
)

# ---------------------------------------------------------------------------
# auracle_rpc library
# ---------------------------------------------------------------------------
add_library(auracle_rpc
  src/inventory_service.cpp
  src/convert.cpp
  ${PROTO_GENERATED_SRCS}
)

target_link_libraries(auracle_rpc
  PUBLIC  auracle_inventory gRPC::grpc++ protobuf::libprotobuf
)

target_include_directories(auracle_rpc
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(auracle_rpc SYSTEM
  PUBLIC ${PROTO_GEN_DIR}
)
