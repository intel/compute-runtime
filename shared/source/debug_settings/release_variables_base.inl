/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

/*This file contains debug variables which are read by release builds of the driver. (All other variables are read in debug builds only)
  This is not a stable interface. Variables can be added, removed, modified at any time.
  Variables are provided for experimentation. Do not use in production deployments. No support provided.*/

DECLARE_DEBUG_VARIABLE(bool, MakeAllBuffersResident, false, "Make all buffers resident after creation")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideDefaultFP64Settings, -1, "-1: dont override, 0: disable, 1: enable.")
DECLARE_DEBUG_VARIABLE(bool, NEO_CAL_ENABLED, false, "Set by the Compute Aggregation Layer.")
DECLARE_DEBUG_VARIABLE(std::string, ZE_AFFINITY_MASK, std::string("default"), "Refer to the Level Zero Specification for a description")
DECLARE_DEBUG_VARIABLE(std::string, ZEX_NUMBER_OF_CCS, std::string("default"), "Define number of CCS engines per root device, e.g. setting Root Device Index 0 to 4 CCS, and Root Device Index 1 To 1 CCS: ZEX_NUMBER_OF_CCS=0:4,1:1")
DECLARE_DEBUG_VARIABLE(bool, ZE_ENABLE_PCI_ID_DEVICE_ORDER, false, "Refer to the Level Zero Specification for a description")
DECLARE_DEBUG_VARIABLE(int32_t, NEO_LOCAL_MEMORY_ALLOCATION_MODE, 0, "Specify device-USM allocation policy. 0: default for given HW; 1: require local-memory (return out-of-memory error otherwise); 2: prefer local-memory but refer to system-memory as a fallback");