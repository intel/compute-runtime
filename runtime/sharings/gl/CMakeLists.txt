# Copyright (c) 2017-2018, Intel Corporation
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

if(WIN32)
  set(RUNTIME_SRCS_SHARINGS_GL
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gl_api.cpp
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
