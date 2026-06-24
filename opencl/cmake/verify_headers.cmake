#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

list(APPEND opencl_include_dirs ${KHRONOS_GL_HEADERS_DIR})

if(WIN32)
  list(APPEND opencl_include_dirs
       ${NEO_SOURCE_DIR}/opencl/source/os_interface/windows
  )
else()
  list(APPEND opencl_include_dirs
       ${NEO_SOURCE_DIR}/opencl/source/os_interface/linux
  )
endif()

list(APPEND opencl_exclude_patterns
     "/cl_gl_private_intel_structures.h"
)

if(UNIX)
  list(APPEND opencl_exclude_patterns
       "/mock_d3d_objects.h"
       "/d3d_test_fixture.h"
  )
endif()

create_verify_headers_target(opencl
                             INCLUDE_DIRS ${opencl_include_dirs}
                             EXCLUDE_PATTERNS ${opencl_exclude_patterns}
)
