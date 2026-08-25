/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

/*This file contains debug variables which are read by release builds of the driver. (All other variables are read in debug builds only)
  This is not a stable interface. Variables can be added, removed, modified at any time.
  Variables are provided for experimentation. Do not use in production deployments. No support provided.*/

DECLARE_RELEASE_VARIABLE(int32_t, EnableLEO, -1, "Enable LEO - Level Zero executing OpenCL. -1: default, 0: disabled, 1: enabled")
DECLARE_RELEASE_VARIABLE(int32_t, EnableLEOLoaderDispatch, -1, "Dispatch the Level Zero calls made by LEO through the Level Zero loader, so that loader layers can observe them. -1: default (disabled), 0: disabled, 1: enabled")
DECLARE_RELEASE_VARIABLE(int32_t, OverrideDefaultFP64Settings, -1, "-1: dont override, 0: disable, 1: enable.")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(bool, NEO_CAL_ENABLED, false, "Set by the Compute Aggregation Layer.")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(std::string, ZE_AFFINITY_MASK, std::string("default"), "Refer to the Level Zero Specification for a description")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(std::string, ZEX_NUMBER_OF_CCS, std::string("default"), "Define number of CCS engines per root device, e.g. setting Root Device Index 0 to 4 CCS, and Root Device Index 1 To 1 CCS: ZEX_NUMBER_OF_CCS=0:4,1:1")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(bool, ZE_ENABLE_PCI_ID_DEVICE_ORDER, false, "Refer to the Level Zero Specification for a description")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(int32_t, ZET_ENABLE_PROGRAM_DEBUGGING, 0, "Refer to the Level Zero Specification for a description. 0: disabled, 1: online debugging, 2: offline debugging.")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(bool, ZET_ENABLE_METRICS, false, "Refer to the Level Zero Specification for a description. Enables the metrics API.")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(bool, ZET_ENABLE_PROGRAM_INSTRUMENTATION, false, "Refer to the Level Zero Specification for a description. Enables GTPin instrumentation.")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(bool, ZES_ENABLE_SYSMAN, false, "Refer to the Level Zero Specification for a description. Enables the sysman API.")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(std::string, ZE_FLAT_DEVICE_HIERARCHY, std::string(""), "Refer to the Level Zero Specification for a description. Device hierarchy exposure mode: COMPOSITE, FLAT, or COMBINED.")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(int32_t, NEO_LOCAL_MEMORY_ALLOCATION_MODE, 0, "Specify device-USM allocation policy. 0: default for given HW; 1: require local-memory (return out-of-memory error otherwise); 2: prefer local-memory but refer to system-memory as a fallback")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(bool, NEO_FP64_EMULATION, false, "Enables FP64 emulation.")
DECLARE_RELEASE_VARIABLE_ENV_FIRST(bool, NEO_L0_SYSMAN_NO_CONTEXT_MODE, false, "Disables OS context/engine creation for sysman-only process usage on Windows.")
