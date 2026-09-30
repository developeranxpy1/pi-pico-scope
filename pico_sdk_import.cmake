# This is a git repository for your project, so the SDK should not be inside it.
if (DEFINED ENV{PICO_SDK_PATH} AND (NOT $ENV{PICO_SDK_PATH} STREQUAL ""))
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "Path to the Raspberry Pi Pico SDK")
elseif (NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "Environment variable PICO_SDK_PATH must be set to the location of the Raspberry Pi Pico SDK")
endif()

get_filename_component(PICO_SDK_PATH "${PICO_SDK_PATH}" REALPATH BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
if (NOT EXISTS ${PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH directory does not exist: ${PICO_SDK_PATH}")
endif()

set(PICO_SDK_INIT_CMAKE_FILE ${PICO_SDK_PATH}/pico_sdk_init.cmake)
if (NOT EXISTS ${PICO_SDK_INIT_CMAKE_FILE})
    message(FATAL_ERROR "Wrong path: ${PICO_SDK_INIT_CMAKE_FILE} does not exist.")
endif()

include(${PICO_SDK_INIT_CMAKE_FILE})