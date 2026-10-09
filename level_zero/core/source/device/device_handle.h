/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/helpers/api_handle_helper.h"

struct _ze_device_handle_t : BaseHandle {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_device_handle_t>);
