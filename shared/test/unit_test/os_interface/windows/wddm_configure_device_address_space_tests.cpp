/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST_F(WddmInstrumentationTest, whenConfiguringDeviceAddressSpaceThenTrueIsReturned) {
    SYSTEM_INFO sysInfo = {};
    WddmMock::getSystemInfo(&sysInfo);

    D3DKMT_HANDLE adapterHandle = ADAPTER_HANDLE;
    D3DKMT_HANDLE deviceHandle = DEVICE_HANDLE;
    const HardwareInfo hwInfo = *defaultHwInfo;
    BOOLEAN ftrL3IACoherency = hwInfo.featureTable.flags.ftrL3IACoherency ? 1 : 0;
    uintptr_t maxAddr = hwInfo.capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress
                            ? reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress) + 1
                            : 0;

    wddm->init();
    EXPECT_EQ(1u, gmmMem->configureDeviceAddressSpaceCalled);
    EXPECT_EQ(adapterHandle, gmmMem->configureDeviceAddressSpaceParamsPassed[0].hAdapter);
    EXPECT_EQ(deviceHandle, gmmMem->configureDeviceAddressSpaceParamsPassed[0].hDevice);
    EXPECT_EQ(wddm->getGdi()->escape.mFunc, gmmMem->configureDeviceAddressSpaceParamsPassed[0].pfnEscape);
    EXPECT_EQ(maxAddr, gmmMem->configureDeviceAddressSpaceParamsPassed[0].svmSize);
    EXPECT_EQ(ftrL3IACoherency, gmmMem->configureDeviceAddressSpaceParamsPassed[0].bdwL3Coherency);
}