# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

macro(hide_subdir subdir)
  file(RELATIVE_PATH subdir_relative ${IGDRCL_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${subdir})
  set(${subdir_relative}_hidden} TRUE)
endmacro()

macro(add_subdirectories)
  file(GLOB subdirectories RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
  foreach(subdir ${subdirectories})
    file(RELATIVE_PATH subdir_relative ${IGDRCL_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${subdir})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/CMakeLists.txt AND NOT ${subdir_relative}_hidden})
      add_subdirectory(${subdir})
    endif()
  endforeach()
endmacro()

macro(create_project_source_tree target)
  if(MSVC)
    set(prefixes ${CMAKE_CURRENT_SOURCE_DIR} ${ARGN})
    get_target_property(source_list ${target} SOURCES)
    foreach(source_file ${source_list})
      if(NOT ${source_file} MATCHES "\<*\>")
        set(source_file_relative ${source_file})
        foreach(prefix ${prefixes})
          if(source_file_relative)
            string(REPLACE "${prefix}" "" source_file_relative ${source_file_relative})
          endif()
        endforeach()
        get_filename_component(source_path_relative ${source_file_relative} PATH)
        if(source_path_relative)
          string(REPLACE "/" "\\" source_path_relative ${source_path_relative})
        endif()
        source_group("Source Files\\${source_path_relative}" FILES ${source_file})
      endif()
    endforeach()
  endif()
endmacro()

macro(create_project_source_tree_with_exports target exports_filename)
  create_project_source_tree(${target})
  if(MSVC)
    if(NOT "${exports_filename}" STREQUAL "")
      source_group("exports" FILES "${exports_filename}")
    endif()
  endif()
endmacro()

macro(apply_macro_for_each_gen type)
  set(given_type ${type})
  foreach(GEN_TYPE ${ALL_GEN_TYPES})
    string(TOLOWER ${GEN_TYPE} GEN_TYPE_LOWER)
    GEN_CONTAINS_PLATFORMS(${given_type} ${GEN_TYPE} GENX_HAS_PLATFORMS)
    if(${GENX_HAS_PLATFORMS})
      macro_for_each_gen()
    endif()
  endforeach()
endmacro()

macro(apply_macro_for_each_platform)
  GET_PLATFORMS_FOR_GEN(${given_type} ${GEN_TYPE} TESTED_GENX_PLATFORMS)
  foreach(PLATFORM_IT ${TESTED_GENX_PLATFORMS})
    string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
    macro_for_each_platform()
  endforeach()
endmacro()

macro(apply_macro_for_each_test_config type)
  GET_TEST_CONFIGURATIONS_FOR_PLATFORM(${type} ${GEN_TYPE} ${PLATFORM_IT} PLATFORM_CONFIGURATIONS)
  foreach(PLATFORM_CONFIGURATION ${PLATFORM_CONFIGURATIONS})
    string(REPLACE "/" ";" CONFIGURATION_PARAMS ${PLATFORM_CONFIGURATION})
    list(GET CONFIGURATION_PARAMS 1 SLICES)
    list(GET CONFIGURATION_PARAMS 2 SUBSLICES)
    list(GET CONFIGURATION_PARAMS 3 EU_PER_SS)
    macro_for_each_test_config()
  endforeach()
endmacro()
