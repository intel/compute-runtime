#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

list(APPEND l0_include_dirs
     ${NEO_IGSC_HEADERS_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/include
     ${CMAKE_CURRENT_SOURCE_DIR}/core/source/cmdlist/definitions${BRANCH_DIR_SUFFIX}
     ${NEO_SOURCE_DIR}/third_party/opengl_headers
)

list(APPEND l0_exclude_patterns
     "/cl_gl_private_intel_structures.h"
)

if(WIN32)
  list(APPEND l0_include_dirs
       ${CMAKE_CURRENT_SOURCE_DIR}/api/opencl/source/sharings/gl/windows/include
  )
else()
  list(APPEND l0_include_dirs
       ${CMAKE_CURRENT_SOURCE_DIR}/api/opencl/source/sharings/gl/linux/include
  )
endif()

if(WIN32)
  list(APPEND l0_exclude_patterns
       "/mock_fabric.h"
       "/zello_ipc_common.h"
  )
else()
  if(NOT NEO_ENABLE_I915_PRELIM_DETECTION)
    list(APPEND l0_exclude_patterns
         "/prelim/"
         "/mock_engine_prelim.h"
         "/mock_sysfs_scheduler_prelim.h"
         "/mock_sysfs_vf_management.h"
    )
  endif()
  if(NOT NEO_ENABLE_XE_PRELIM_DETECTION)
    list(APPEND l0_exclude_patterns
         "/debug_session_fixtures_linux_xe.h"
         "/mock_sysfs_vf_management_xe.h"
    )
  endif()
endif()

create_verify_headers_target(level_zero
                             INCLUDE_DIRS ${l0_include_dirs}
                             EXCLUDE_PATTERNS ${l0_exclude_patterns}
)
