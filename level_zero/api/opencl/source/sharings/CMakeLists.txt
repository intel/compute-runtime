#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_include_directories(${L0_STATIC_LIB_NAME} PRIVATE
                           ${KHRONOS_HEADERS_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
)

if(UNIX AND NOT DISABLE_WDDM_LINUX)
  target_include_directories(${L0_STATIC_LIB_NAME} PUBLIC ${WDK_INCLUDE_PATHS})
endif()

set(LEO_SHARINGS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_sharing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_sharing_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_sharing_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_sharing_factory.inl
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SHARINGS_SRCS})
set_property(GLOBAL PROPERTY LEO_SHARINGS_SRCS ${LEO_SHARINGS_SRCS})

add_subdirectories()