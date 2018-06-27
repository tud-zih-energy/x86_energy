include("${CMAKE_CURRENT_LIST_DIR}/ix86_energyTargets.cmake")

set(x86_energy_FOUND TRUE)

get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)

message(STATUS "Using x86_energy: ${_IMPORT_PREFIX}")

set(_IMPORT_PREFIX)
