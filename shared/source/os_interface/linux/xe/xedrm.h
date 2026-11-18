/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#if !defined(_XE_DRM_H_) && !defined(_UAPI_XE_DRM_H_)
#include <linux/types.h>
#include <sys/ioctl.h>
namespace NEO {
namespace XeDrm {
#include "xe_drm.h"
}
} // namespace NEO
using namespace NEO::XeDrm;

#endif
