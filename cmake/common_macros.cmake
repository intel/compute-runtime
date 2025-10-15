#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(hide_subdir subdir)
  file(RELATIVE_PATH subdir_relative ${NEO_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${subdir})
  set(${subdir_relative}_hidden} TRUE)
endmacro()

macro(add_subdirectory_unique subdir)
  file(RELATIVE_PATH subdir_relative ${NEO_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${subdir})
  if(NOT ${subdir_relative}_hidden})
    add_subdirectory(${subdir} ${ARGN})
  endif()
  hide_subdir(${subdir})
endmacro()

macro(add_subdirectories)
  file(GLOB subdirectories RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
  foreach(subdir ${subdirectories})
    file(RELATIVE_PATH subdir_relative ${NEO_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${subdir})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/CMakeLists.txt AND NOT ${subdir_relative}_hidden})
      add_subdirectory(${subdir})
    endif()
  endforeach()
endmacro()

macro(create_project_source_tree target)
  if(MSVC)
    set(prefixes ${CMAKE_CURRENT_SOURCE_DIR} ${ARGN} ${NEO_SOURCE_DIR})
    get_target_property(source_list ${target} SOURCES)
    foreach(source_file ${source_list})
      if(NOT ${source_file} MATCHES "\<*\>")
        string(TOLOWER ${source_file} source_file_relative)
        foreach(prefix ${prefixes})
          if(source_file_relative)
            string(TOLOWER ${prefix} prefix)
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
  create_project_source_tree(${target} ${ARGN})
  if(MSVC)
    if(NOT "${exports_filename}" STREQUAL "")
      source_group("exports" FILES "${exports_filename}")
    endif()
  endif()
endmacro()

macro(apply_macro_for_each_core_type type)
  set(given_type ${type})
  foreach(CORE_TYPE ${ALL_CORE_TYPES})
    string(TOLOWER ${CORE_TYPE} CORE_TYPE_LOWER)
    CORE_CONTAINS_ANY_PLATFORM(${given_type} ${CORE_TYPE} COREX_HAS_PLATFORMS)
    if(${COREX_HAS_PLATFORMS})
      macro_for_each_core_type()
    endif()
  endforeach()
endmacro()

macro(apply_macro_for_each_platform given_type)
  GET_PLATFORMS_FOR_CORE_TYPE(${given_type} ${CORE_TYPE} TESTED_COREX_PLATFORMS)
  foreach(PLATFORM_IT ${TESTED_COREX_PLATFORMS})
    string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
    macro_for_each_platform()
  endforeach()
endmacro()

macro(append_sources_from_properties list_name)
  foreach(name ${ARGN})
    get_property(${name} GLOBAL PROPERTY ${name})
    list(APPEND ${list_name} ${${name}})
  endforeach()
endmacro()

function(parse_revision_config config default_device out_device out_revision_id)
  string(REPLACE "/" ";" config_as_list ${config})
  list(LENGTH config_as_list config_entries)
  if(${config_entries} EQUAL 2)
    list(GET config_as_list 0 device_id)
    list(GET config_as_list 1 revision_id)
    set(${out_device} ${device_id} PARENT_SCOPE)
    set(${out_revision_id} ${revision_id} PARENT_SCOPE)
  else()
    list(GET config_as_list 0 revision_id)
    set(${out_device} ${default_device} PARENT_SCOPE)
    set(${out_revision_id} ${revision_id} PARENT_SCOPE)
  endif()
endfunction()

function(create_verify_headers_target folder)
  cmake_parse_arguments(ARG "" ""
                        "INCLUDE_DIRS;EXCLUDE_PATTERNS"
                        ${ARGN}
  )

  set(LIB_NAME verify_${folder})
  add_library(${LIB_NAME} INTERFACE)

  target_compile_definitions(${LIB_NAME} INTERFACE MOCKABLE_VIRTUAL=virtual ${TESTED_CORE_FLAGS_DEFINITONS})
  set(NEO_SUPPORTED_PRODUCT_FAMILIES ${ALL_TESTED_PRODUCT_FAMILY})
  string(REPLACE ";" "," NEO_SUPPORTED_PRODUCT_FAMILIES "${NEO_SUPPORTED_PRODUCT_FAMILIES}")
  target_compile_definitions(${LIB_NAME} INTERFACE SUPPORTED_TEST_PRODUCT_FAMILIES=${NEO_SUPPORTED_PRODUCT_FAMILIES} IGA_LIBRARY_NAME="")

  target_include_directories(${LIB_NAME} INTERFACE
                             ${AOT_CONFIG_HEADERS_DIR}
                             ${ENGINE_NODE_DIR}
                             ${NEO__GMM_INCLUDE_DIR}
                             ${CIF_BASE_DIR}
                             ${IGC_OCL_ADAPTOR_DIR}
                             ${NEO__IGC_INCLUDE_DIR}
                             ${KHRONOS_HEADERS_DIR}
                             ${VISA_INCLUDE_DIR}
                             ${IGA_INCLUDE_DIR}
                             ${NEO_SOURCE_DIR}/shared/test/common/test_configuration/unit_tests
                             ${NEO_SOURCE_DIR}/shared/test/common/test_macros/header/${BRANCH_TYPE}
                             ${NEO_SOURCE_DIR}/shared/test/common/helpers/includes/${BRANCH_TYPE}
                             $<TARGET_PROPERTY:neo_gtest,INTERFACE_INCLUDE_DIRECTORIES>
  )

  if(WIN32 OR NOT DISABLE_WDDM_LINUX)
    target_include_directories(${LIB_NAME} INTERFACE ${WDK_INCLUDE_PATHS})
  endif()
  if(WIN32)
    target_include_directories(${LIB_NAME} INTERFACE
                               ${NEO_SOURCE_DIR}/shared/source/os_interface/windows
    )
  else()
    target_include_directories(${LIB_NAME} INTERFACE
                               ${NEO_SOURCE_DIR}/shared/source/os_interface/linux
    )
  endif()

  if(ARG_INCLUDE_DIRS)
    target_include_directories(${LIB_NAME} INTERFACE ${ARG_INCLUDE_DIRS})
  endif()

  file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "${NEO_SOURCE_DIR}/${folder}/*.h")

  set(exclude_win32 "/linux/" "/va/")
  set(exclude_linux "/windows/" "/d3d/")

  if(WIN32)
    set(all_exclude ${exclude_win32} ${ARG_EXCLUDE_PATTERNS})
  else()
    set(all_exclude ${exclude_linux} ${ARG_EXCLUDE_PATTERNS})
  endif()

  foreach(header ${HEADER_FILES})
    foreach(pattern ${all_exclude})
      string(FIND "${header}" "${pattern}" found)
      if(NOT (found EQUAL -1))
        list(REMOVE_ITEM HEADER_FILES "${header}")
        break()
      endif()
    endforeach()
  endforeach()

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    target_sources(${LIB_NAME} INTERFACE FILE_SET HEADERS FILES ${HEADER_FILES})
  else()
    target_sources(${LIB_NAME} INTERFACE ${HEADER_FILES})
  endif()

endfunction()
