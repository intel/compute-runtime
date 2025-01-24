/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/device_info_fixture.h"

using namespace NEO;

PVCTEST_F(GetDeviceInfoMemCapabilitiesTest, GivenValidParametersWhenGetDeviceInfoIsCalledForPVCThenClSuccessIsReturned) {
    std::vector<TestParams> params = {
        {CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL, CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL},
        {CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL,
         0}};

    check(params);
}

PVCTEST_F(GetDeviceInfoMemCapabilitiesTest, GivenEnableUsmConcurrentAccessSupportWhenGetDeviceInfoIsCalledForPVCThenClSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUsmConcurrentAccessSupport.set(0b1110);
    debugManager.flags.UseKmdMigration.set(true);

    std::vector<TestParams> params = {
        {CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL, CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL},
        {CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL,
         0}};

    check(params);
}
