include("${CMAKE_CURRENT_LIST_DIR}/x86_energyTargets.cmake")

get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)

message(STATUS "Using x86_energy: ${_IMPORT_PREFIX}")

set(_IMPORT_PREFIX)
