#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(RUNTIME_SRCS_SHARINGS_GL
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gl_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gl_api_intel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_arb_sync_event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_arb_sync_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_cl_image_format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_context_guard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sync_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sync_event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_texture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_texture.cpp
  )
  set(ADDITIONAL_EXPORTS "clEnqueueMarkerWithSyncObjectINTEL"
                         "clGetCLObjectInfoINTEL"
                         "clGetCLEventInfoINTEL"
                         "clReleaseGlSharedEventINTEL")
  foreach(EXPORT_NAME ${ADDITIONAL_EXPORTS})
    set(MSVC_DEF_ADDITIONAL_EXPORTS "${MSVC_DEF_ADDITIONAL_EXPORTS}\n${EXPORT_NAME}")
  endforeach(EXPORT_NAME)

  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SHARINGS_GL})
  set(MSVC_DEF_ADDITIONAL_EXPORTS "${MSVC_DEF_ADDITIONAL_EXPORTS}" PARENT_SCOPE)

  target_sources(${SHARINGS_ENABLE_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/win_enable_gl.cpp)
endif()
