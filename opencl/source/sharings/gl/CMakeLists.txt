#
# Copyright (C) 2018-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_SHARINGS_GL
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gl_api_intel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gl_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_arb_sync_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_cl_image_format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_context_guard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sync_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_texture.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SHARINGS_GL})

add_subdirectories()
if(WIN32)
  set(ADDITIONAL_EXPORTS
      "clEnqueueMarkerWithSyncObjectINTEL"
      "clGetCLObjectInfoINTEL"
      "clGetCLEventInfoINTEL"
      "clReleaseGlSharedEventINTEL"
  )
  foreach(EXPORT_NAME ${ADDITIONAL_EXPORTS})
    set(MSVC_DEF_ADDITIONAL_EXPORTS "${MSVC_DEF_ADDITIONAL_EXPORTS}\n${EXPORT_NAME}")
  endforeach()
  set(MSVC_DEF_ADDITIONAL_EXPORTS "${MSVC_DEF_ADDITIONAL_EXPORTS}" PARENT_SCOPE)
endif()
set_property(GLOBAL PROPERTY RUNTIME_SRCS_SHARINGS_GL ${RUNTIME_SRCS_SHARINGS_GL})
