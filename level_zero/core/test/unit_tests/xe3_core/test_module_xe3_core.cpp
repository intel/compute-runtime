/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace NEO {
class Device;
} // namespace NEO

namespace L0 {
namespace ult {

using Xe3KernelSetupTests = ::testing::Test;

XE3_CORETEST_F(Xe3KernelSetupTests, givenParamsWhenSetupGroupSizeThenNumThreadsPerThreadGroupAreCorrectly) {
    DebugManagerStateRestore restore;
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());

    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 128u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 64u}, // SIMT Size, Max Num of threads
            {32u, 32u},

        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.privateState.numThreadsPerThreadGroup);
            kernel.privateState.groupSize[0] = kernel.privateState.groupSize[1] = kernel.privateState.groupSize[2] = 0;
        }
    }
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 160u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 48u}, // SIMT Size,  Max Num of threads
            {32u, 32u},
        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.privateState.numThreadsPerThreadGroup);
            kernel.privateState.groupSize[0] = kernel.privateState.groupSize[1] = kernel.privateState.groupSize[2] = 0;
        }
    }
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 192u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 40u}, // SIMT Size, Max Num of threads
            {32u, 32u},
        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.privateState.numThreadsPerThreadGroup);
            kernel.privateState.groupSize[0] = kernel.privateState.groupSize[1] = kernel.privateState.groupSize[2] = 0;
        }
    }
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 256u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 32u}, // SIMT Size, Max Num of threads
            {32u, 32u},
        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.privateState.numThreadsPerThreadGroup);
            kernel.privateState.groupSize[0] = kernel.privateState.groupSize[1] = kernel.privateState.groupSize[2] = 0;
        }
    }
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 512u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 16u}, // SIMT Size, Max Num of threads
            {32u, 16u},
        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.privateState.numThreadsPerThreadGroup);
            kernel.privateState.groupSize[0] = kernel.privateState.groupSize[1] = kernel.privateState.groupSize[2] = 0;
        }
    }
}

} // namespace ult
} // namespace L0
