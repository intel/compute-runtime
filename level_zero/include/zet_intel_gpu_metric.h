/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZET_INTEL_GPU_METRIC_H
#define _ZET_INTEL_GPU_METRIC_H

#include <level_zero/zet_api.h>

#if defined(__cplusplus)
#pragma once
extern "C" {
#endif

#include <stdint.h>

#define ZET_INTEL_GPU_METRIC_VERSION_MAJOR 0
#define ZET_INTEL_GPU_METRIC_VERSION_MINOR 2
#define ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP 64u
#define ZET_INTEL_METRIC_PROGRAMMABLE_PARAM_TYPE_GENERIC_EXP (0x7ffffffe)

#if defined(__cplusplus)
} // extern "C"
#endif

#endif
