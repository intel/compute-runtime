#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

list(APPEND shared_exclude_patterns
     "/neo_gmmlib.h"
     "/aarch64/"
     "/wsl_compute_helper/"
)

if(UNIX)
  list(APPEND shared_exclude_patterns
       "/mock_ostime_win.h"
  )
  if(NOT NEO_ENABLE_I915_PRELIM_DETECTION)
    list(APPEND shared_exclude_patterns
         "i915_prelim"
         "drm_prelim"
         "fixture_prelim"
         "drm_mock_extended"
    )
  endif()
  if(NOT NEO_ENABLE_XE_PRELIM_DETECTION)
    list(APPEND shared_exclude_patterns
         "xedrm_prelim"
         "eudebug_interface_prelim"
         "debug_mock_drm_xe"
    )
  endif()
endif()

create_verify_headers_target(shared
                             EXCLUDE_PATTERNS ${shared_exclude_patterns}
)
