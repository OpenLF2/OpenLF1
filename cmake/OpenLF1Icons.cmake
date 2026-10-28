# SPDX-License-Identifier: MIT
# Copyright (c) 2026 OpenLF1

# Icons are rendered from packaging/openlf1.svg by tools/icons/make_icons.py and never committed.
# OPENLF1_ICON_DIR lets packaging scripts pre-fill icons where Python is not available.
set(OPENLF1_ICON_DIR "" CACHE PATH "Directory of icons rendered by tools/icons/make_icons.py (empty: render into the build tree)")
set(openlf1_icons "")
set(openlf1_icon_svg "${PROJECT_SOURCE_DIR}/packaging/openlf1.svg")
set(openlf1_icon_tool "${PROJECT_SOURCE_DIR}/tools/icons/make_icons.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${openlf1_icon_svg}" "${openlf1_icon_tool}")
if(OPENLF1_ICON_DIR)
    set(openlf1_icons "${OPENLF1_ICON_DIR}")
else()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(openlf1_icon_command "${Python3_EXECUTABLE}" "${openlf1_icon_tool}" "${CMAKE_BINARY_DIR}/icons")
        if(APPLE)
            list(APPEND openlf1_icon_command --icns)
        endif()
        execute_process(COMMAND ${openlf1_icon_command}
            RESULT_VARIABLE openlf1_icon_result ERROR_VARIABLE openlf1_icon_error)
        if(openlf1_icon_result EQUAL 0)
            set(openlf1_icons "${CMAKE_BINARY_DIR}/icons")
        else()
            message(WARNING "Icons were not generated: ${openlf1_icon_error}")
        endif()
    else()
        message(STATUS "Python 3 not found: building without icons")
    endif()
endif()
if(openlf1_icons AND (NOT EXISTS "${openlf1_icons}/window_icon.hpp" OR
        (WIN32 AND NOT EXISTS "${openlf1_icons}/openlf1.ico") OR
        (APPLE AND NOT EXISTS "${openlf1_icons}/openlf1.icns")))
    message(WARNING "${openlf1_icons} holds no rendered icons; building without them")
    set(openlf1_icons "")
endif()
