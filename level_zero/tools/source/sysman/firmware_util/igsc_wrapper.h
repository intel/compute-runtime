/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef IGSC_PRESENT
#pragma warning(disable : 4200)
#include "igsc_lib.h"
#else
typedef struct igsc_device_info {
} igsc_device_info_t;
#endif