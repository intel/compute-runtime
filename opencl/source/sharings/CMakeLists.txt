#
# Copyright (C) 2018-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

file(GLOB_RECURSE SHARING_ENABLE_CPPS enable*.cpp)
add_library(${SHARINGS_ENABLE_LIB_NAME} OBJECT EXCLUDE_FROM_ALL ${SHARING_ENABLE_CPPS})
set_target_properties(${SHARINGS_ENABLE_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${SHARINGS_ENABLE_LIB_NAME} PROPERTIES FOLDER ${OPENCL_RUNTIME_PROJECTS_FOLDER})

target_include_directories(${SHARINGS_ENABLE_LIB_NAME} PRIVATE
                           ${KHRONOS_HEADERS_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
)

if(UNIX AND NOT DISABLE_WDDM_LINUX)
  target_include_directories(${SHARINGS_ENABLE_LIB_NAME} PUBLIC ${WDK_INCLUDE_PATHS})
endif()

set(RUNTIME_SRCS_SHARINGS
    ${CMAKE_CURRENT_SOURCE_DIR}/sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sharing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sharing_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sharing_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sharing_factory.inl
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SHARINGS})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_SHARINGS ${RUNTIME_SRCS_SHARINGS})

if(WIN32)
  target_include_directories(${SHARINGS_ENABLE_LIB_NAME} PRIVATE ${NEO_SOURCE_DIR}/opencl/source/os_interface/windows)
else()
  target_include_directories(${SHARINGS_ENABLE_LIB_NAME} PRIVATE ${NEO_SOURCE_DIR}/opencl/source/os_interface/linux)
endif()

add_subdirectories()

create_project_source_tree(${SHARINGS_ENABLE_LIB_NAME})
set(MSVC_DEF_ADDITIONAL_EXPORTS ${MSVC_DEF_ADDITIONAL_EXPORTS} PARENT_SCOPE)
