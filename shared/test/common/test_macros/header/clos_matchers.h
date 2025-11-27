/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "common_matchers.h"
using IsClosSupported = IsAnyProducts<IGFX_PVC, NEO::criProductEnumValue>;
using IsClosUnsupported = IsNoneProducts<IGFX_PVC, NEO::criProductEnumValue>;
