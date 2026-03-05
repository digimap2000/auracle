find_package(dts CONFIG REQUIRED HINTS /opt/dts)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Proto import root (repo-level proto/ directory)
set(AURACLE_PROTO_IMPORT_PATH "${PROJECT_SOURCE_DIR}/../proto" CACHE PATH
  "Root import path for .proto files")
