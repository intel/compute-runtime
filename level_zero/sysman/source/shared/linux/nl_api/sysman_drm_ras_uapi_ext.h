/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "third_party/uapi/drm-next/drm/drm_ras.h"

enum {
    DRM_RAS_A_ERROR_THRESHOLD_ATTRS_NODE_ID = 1,
    DRM_RAS_A_ERROR_THRESHOLD_ATTRS_ERROR_ID,
    DRM_RAS_A_ERROR_THRESHOLD_ATTRS_ERROR_NAME,
    DRM_RAS_A_ERROR_THRESHOLD_ATTRS_ERROR_THRESHOLD,

    __DRM_RAS_A_ERROR_THRESHOLD_ATTRS_MAX,
    DRM_RAS_A_ERROR_THRESHOLD_ATTRS_MAX = (__DRM_RAS_A_ERROR_THRESHOLD_ATTRS_MAX - 1)
};

/* DRM_RAS_CMD_GET_ERROR_COUNTER = 2 is the last command in upstream drm_ras.h */
#define DRM_RAS_CMD_CLEAR_ERROR_COUNTER 3
#define DRM_RAS_CMD_GET_ERROR_THRESHOLD 4
#define DRM_RAS_CMD_SET_ERROR_THRESHOLD 5
#define DRM_RAS_CMD_ERROR_EVENT 6

/* Total number of RAS commands including all extensions */
#define NEO_DRM_RAS_CMD_MAX 6
