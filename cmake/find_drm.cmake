#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT DEFINED NEO_DRM_HEADERS_DIR OR NEO_DRM_HEADERS_DIR STREQUAL "")
  get_filename_component(NEO_DRM_HEADERS_DIR "${NEO_SOURCE_DIR}/third_party/uapi/drm-next/drm" ABSOLUTE)
endif()

message(STATUS "drm includes dir: ${NEO_DRM_HEADERS_DIR}")

if(NOT DEFINED NEO_I915_HEADERS_DIR OR NEO_I915_HEADERS_DIR STREQUAL "")
  get_filename_component(NEO_I915_HEADERS_DIR "${NEO_SOURCE_DIR}/third_party/uapi/drm-next/i915" ABSOLUTE)
endif()

message(STATUS "i915 includes dir: ${NEO_I915_HEADERS_DIR}")

if(NOT DEFINED NEO_I915_PRELIM_HEADERS_DIR OR NEO_I915_PRELIM_HEADERS_DIR STREQUAL "")
  get_filename_component(NEO_I915_PRELIM_HEADERS_DIR "${NEO_SOURCE_DIR}/third_party/uapi/drm-uapi-helper/master" ABSOLUTE)
endif()

message(STATUS "i915 prelim includes dir: ${NEO_I915_PRELIM_HEADERS_DIR}")

if(NOT DEFINED NEO_XE_HEADERS_DIR OR NEO_XE_HEADERS_DIR STREQUAL "")
  get_filename_component(NEO_XE_HEADERS_DIR "${NEO_SOURCE_DIR}/third_party/uapi/drm-next/xe" ABSOLUTE)
endif()

message(STATUS "xe includes dir: ${NEO_XE_HEADERS_DIR}")

if(NEO_ENABLE_XE_PRELIM_DETECTION)
  if(NOT DEFINED NEO_XE_PRELIM_HEADERS_DIR OR NEO_XE_PRELIM_HEADERS_DIR STREQUAL "")
    get_filename_component(NEO_XE_PRELIM_HEADERS_DIR "${NEO_SOURCE_DIR}/third_party/uapi/drm-uapi-helper/xe" ABSOLUTE)
  endif()
  message(STATUS "xe prelim includes dir: ${NEO_XE_PRELIM_HEADERS_DIR}")
endif()

if(NOT DEFINED NEO_IAF_HEADERS_DIR OR NEO_IAF_HEADERS_DIR STREQUAL "")
  get_filename_component(NEO_IAF_HEADERS_DIR "${NEO_SOURCE_DIR}/third_party/uapi/drm-uapi-helper/master" ABSOLUTE)
endif()

message(STATUS "iaf includes dir: ${NEO_IAF_HEADERS_DIR}")

set(NEO_LINUX_KMD_HEADERS_DIR
    ${NEO_DRM_HEADERS_DIR}
    ${NEO_I915_HEADERS_DIR}
    ${NEO_I915_PRELIM_HEADERS_DIR}
    ${NEO_XE_HEADERS_DIR}
    ${NEO_XE_PRELIM_HEADERS_DIR}
    ${NEO_IAF_HEADERS_DIR}
)
