/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "common_matchers.h"
using IsClosSupported = IsAnyProducts<IGFX_PVC, IGFX_CRI>;
using IsClosUnsupported = IsNoneProducts<IGFX_PVC, IGFX_CRI>;
