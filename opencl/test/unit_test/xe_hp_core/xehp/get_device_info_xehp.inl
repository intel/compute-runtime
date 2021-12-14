/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/device_info_fixture.h"

using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(GetDeviceInfoMemCapabilitiesTest, GivenValidParametersWhenGetDeviceInfoIsCalledForXE_HP_COREThenClSuccessIsReturned, IGFX_XE_HP_SDV);

XEHPTEST_F(GetDeviceInfoMemCapabilitiesTest, GivenValidParametersWhenGetDeviceInfoIsCalledForXEHPThenClSuccessIsReturned) {
    std::vector<TestParams> params = {
        {CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL, 0}};

    check(params);
}

XEHPTEST_F(GetDeviceInfoMemCapabilitiesTest, GivenA0ThenUsmHostMemSupportIsEnabledByDefault) {
    std::vector<TestParams> params = {{CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL, CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL}};

    check(params);
}

XEHPTEST_F(GetDeviceInfoMemCapabilitiesTest, GivenDebugVariableIsEnabledThenUsmHostMemSupportIsEnabled) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableHostUsmSupport.set(1);
    std::vector<TestParams> params = {{CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL, CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL}};

    check(params);
}

XEHPTEST_F(GetDeviceInfoMemCapabilitiesTest, GivenA0AndDebugVariableIsDisabledThenUsmHostMemSupportIsDisabled) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    defaultHwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, *defaultHwInfo);
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableHostUsmSupport.set(0);
    std::vector<TestParams> params = {{CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL, 0}};
    std::vector<TestParams> enabledParams = {{CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL, CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL}};

    check(params);

    DebugManager.flags.ForceLocalMemoryAccessMode.set(1);
    check(params);

    DebugManager.flags.EnableHostUsmSupport.set(1);
    check(enabledParams);

    DebugManager.flags.EnableHostUsmSupport.set(-1);
    check(params);
}

XEHPTEST_F(GetDeviceInfoMemCapabilitiesTest, GivenB0ThenUsmHostMemSupportIsSetCorrectly) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *defaultHwInfo);
    std::vector<TestParams> params = {{CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL, CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL}};
    std::vector<TestParams> disabledParameters = {{CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL, 0u}};

    check(params);

    {
        DebugManagerStateRestore dbgRestorer;
        DebugManager.flags.EnableHostUsmSupport.set(0);
        check(disabledParameters);
    }

    {
        DebugManagerStateRestore dbgRestorer;
        DebugManager.flags.EnableHostUsmSupport.set(1);
        check(params);
    }
}
