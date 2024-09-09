/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef IGSC_PRESENT
#pragma warning(disable : 4200)
#include "igsc_lib.h"
using IgscDeviceInfo = igsc_device_info;
#else
using IgscDeviceInfo = void *;
#endif
