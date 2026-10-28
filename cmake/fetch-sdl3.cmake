# SPDX-License-Identifier: MIT
# Copyright (c) 2026 OpenLF1

include("${CMAKE_CURRENT_LIST_DIR}/dependencies.cmake")

if(NOT OPENLF1_SDL3_OUTPUT_DIR)
    message(FATAL_ERROR "OPENLF1_SDL3_OUTPUT_DIR must be set")
endif()

set(archive "${OPENLF1_SDL3_OUTPUT_DIR}/sdl3.tar.gz")
set(extract_dir "${OPENLF1_SDL3_OUTPUT_DIR}/sdl3-extract")
file(DOWNLOAD "${OPENLF1_SDL3_DJGPP_URL}" "${archive}" TLS_VERIFY ON)
file(REMOVE_RECURSE "${extract_dir}" "${OPENLF1_SDL3_OUTPUT_DIR}/sdl3")
file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${extract_dir}")
file(GLOB sdl3_dirs LIST_DIRECTORIES true "${extract_dir}/*")
list(LENGTH sdl3_dirs count)
if(NOT count EQUAL 1 OR NOT IS_DIRECTORY "${sdl3_dirs}")
    message(FATAL_ERROR "Expected exactly one directory in ${extract_dir}")
endif()
file(RENAME "${sdl3_dirs}" "${OPENLF1_SDL3_OUTPUT_DIR}/sdl3")
file(REMOVE_RECURSE "${extract_dir}")
