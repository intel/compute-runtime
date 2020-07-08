/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

/*This file contains debug variables which are read by release builds of the driver. (All other variables are read in debug builds only)
  This is not a stable interface. Variables can be added, removed, modified at any time.
  Variables are provided for experimentation. Do not use in production deployments. No support provided.*/

DECLARE_DEBUG_VARIABLE(bool, MakeAllBuffersResident, false, "Make all buffers resident after creation")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideDefaultFP64Settings, -1, "-1: dont override, 0: disable, 1: enable.")
DECLARE_DEBUG_VARIABLE(int32_t, EnableCrossDeviceAccess, -1, "-1: default behavior, 0: disabled, 1: enabled, Allows one device to access another device's memory")
