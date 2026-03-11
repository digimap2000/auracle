# ---------------------------------------------------------------------------
# Bluetooth SIG assigned numbers — YAML → C++ code generation
# ---------------------------------------------------------------------------

set(ASSIGNED_NUMBERS_VENV    "${CMAKE_CURRENT_SOURCE_DIR}/.venv")
set(ASSIGNED_NUMBERS_PYTHON  "${ASSIGNED_NUMBERS_VENV}/bin/python3")

if(NOT EXISTS "${ASSIGNED_NUMBERS_PYTHON}")
  message(FATAL_ERROR
    "assigned-numbers venv not found at ${ASSIGNED_NUMBERS_VENV}\n"
    "Create it with:  python3 -m venv ${ASSIGNED_NUMBERS_VENV} && "
    "${ASSIGNED_NUMBERS_PYTHON} -m pip install pyyaml")
endif()

set(ASSIGNED_NUMBERS_INPUT_DIR
  "${PROJECT_SOURCE_DIR}/../vendor/bluetooth-sig-public/assigned_numbers"
  CACHE PATH "Path to Bluetooth SIG assigned_numbers YAML data")

set(ASSIGNED_NUMBERS_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(ASSIGNED_NUMBERS_SCRIPT  "${CMAKE_CURRENT_SOURCE_DIR}/generate.py")
set(ASSIGNED_NUMBERS_SERVICE_DATA_FORMAT_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/service-data-formats")

set(ASSIGNED_NUMBERS_YAML_DEPS
  "${ASSIGNED_NUMBERS_INPUT_DIR}/company_identifiers/company_identifiers.yaml"
  "${ASSIGNED_NUMBERS_INPUT_DIR}/uuids/service_uuids.yaml"
  "${ASSIGNED_NUMBERS_INPUT_DIR}/uuids/characteristic_uuids.yaml"
  "${ASSIGNED_NUMBERS_INPUT_DIR}/core/ad_types.yaml"
)

file(GLOB ASSIGNED_NUMBERS_METADATA_LTV_YAML_DEPS
  "${ASSIGNED_NUMBERS_INPUT_DIR}/profiles_and_services/generic_audio/metadata_ltv/*.yaml")

list(APPEND ASSIGNED_NUMBERS_YAML_DEPS
  ${ASSIGNED_NUMBERS_METADATA_LTV_YAML_DEPS}
)

file(GLOB ASSIGNED_NUMBERS_SERVICE_DATA_FORMAT_YAML_DEPS
  "${ASSIGNED_NUMBERS_SERVICE_DATA_FORMAT_DIR}/*.yaml")

list(APPEND ASSIGNED_NUMBERS_YAML_DEPS
  ${ASSIGNED_NUMBERS_SERVICE_DATA_FORMAT_YAML_DEPS}
)

set(ASSIGNED_NUMBERS_GENERATED_SRCS
  "${ASSIGNED_NUMBERS_GEN_DIR}/assigned_numbers_data.cpp"
  "${ASSIGNED_NUMBERS_GEN_DIR}/service_data_formats_generated.cpp"
)

add_custom_command(
  OUTPUT  ${ASSIGNED_NUMBERS_GENERATED_SRCS}
  COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSIGNED_NUMBERS_GEN_DIR}"
  COMMAND "${ASSIGNED_NUMBERS_PYTHON}" "${ASSIGNED_NUMBERS_SCRIPT}"
          "${ASSIGNED_NUMBERS_INPUT_DIR}"
          "${ASSIGNED_NUMBERS_GEN_DIR}"
  DEPENDS ${ASSIGNED_NUMBERS_SCRIPT} ${ASSIGNED_NUMBERS_YAML_DEPS}
  COMMENT "Generating C++ lookup tables from Bluetooth SIG assigned numbers"
)

# ---------------------------------------------------------------------------
# auracle_assigned_numbers library
# ---------------------------------------------------------------------------
add_library(auracle_assigned_numbers
  assigned_numbers.cpp
  ${ASSIGNED_NUMBERS_GENERATED_SRCS}
)

target_include_directories(auracle_assigned_numbers
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/..   # consumers: #include <assigned-numbers/assigned_numbers.hpp>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
