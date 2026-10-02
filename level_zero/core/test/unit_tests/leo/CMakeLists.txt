#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

get_property(LEO_SOURCES GLOBAL PROPERTY LEO_TEST_SOURCES)

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${LEO_SOURCES}
)

if(WIN32)
  target_include_directories(${TARGET_NAME} PRIVATE
                             ${NEO_SOURCE_DIR}/level_zero/api/opencl/source/sharings/gl/windows/include
                             ${KHRONOS_GL_HEADERS_DIR}
  )
endif()
