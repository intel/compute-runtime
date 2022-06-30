/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

/*This file contains debug variables which are read by release builds of the driver. (All other variables are read in debug builds only)
  This is not a stable interface. Variables can be added, removed, modified at any time.
  Variables are provided for experimentation. Do not use in production deployments. No support provided.*/

DECLARE_DEBUG_VARIABLE(bool, MakeAllBuffersResident, false, "Make all buffers resident after creation")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideDefaultFP64Settings, -1, "-1: dont override, 0: disable, 1: enable.")
DECLARE_DEBUG_VARIABLE(std::string, ZE_AFFINITY_MASK, std::string("default"), "Refer to the Level Zero Specification for a description")
DECLARE_DEBUG_VARIABLE(bool, ZE_ENABLE_PCI_ID_DEVICE_ORDER, true, "Refer to the Level Zero Specification for a description")
