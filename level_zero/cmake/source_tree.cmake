#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

function(create_source_tree target directory)
    if(WIN32)
        get_filename_component(directory ${directory} ABSOLUTE)
        get_target_property(source_list ${target} SOURCES)
        source_group(TREE ${directory} FILES ${source_list})
    endif()
endfunction()

macro(add_subdirectoriesL0 curdir dirmask)
    file(GLOB children RELATIVE ${curdir} ${curdir}/${dirmask})
    set(dirlist "")

    foreach(child ${children})
        if(IS_DIRECTORY ${curdir}/${child})
            list(APPEND dirlist ${child})
        endif()
    endforeach()

    foreach(subdir ${dirlist})
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/CMakeLists.txt)
            add_subdirectory(${subdir})
        endif()
    endforeach()
endmacro()
