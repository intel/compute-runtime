/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/driver_experimental/zex_module.h"

namespace L0::ult {

struct AUBVariableRegisterPerThreadL0 : Test<AUBFixtureL0> {
    std::vector<uint32_t> getGrfSizes(ze_device_handle_t device) {
        ze_device_module_properties_t deviceModuleProperties{};
        zex_device_module_register_file_exp_t deviceModuleRegisterFile{ZEX_STRUCTURE_DEVICE_MODULE_REGISTER_FILE_EXP};
        deviceModuleProperties.pNext = &deviceModuleRegisterFile;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetModuleProperties(device, &deviceModuleProperties));

        std::vector<uint32_t> result(deviceModuleRegisterFile.registerFileSizesCount);
        deviceModuleRegisterFile.registerFileSizes = result.data();
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetModuleProperties(device, &deviceModuleProperties));
        return result;
    }

    void *allocateDeviceMemory(ze_context_handle_t context, ze_device_handle_t device, size_t size, size_t alignment) {
        void *result = nullptr;
        ze_device_mem_alloc_desc_t descriptor{};
        descriptor.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemAllocDevice(context, &descriptor, size, alignment, device, &result));
        return result;
    }
};

XE3_CORETEST_F(AUBVariableRegisterPerThreadL0, givenZeOptRegisterFileSizeOptionWhenExecutingKernelThenCorrectValuesAreReturned) {
    constexpr auto bufferSize = 256u;
    const auto grfSizes = getGrfSizes(device);
    const auto &expectedGrfSizes = device->getProductHelper().getSupportedNumGrfs(device->getNEODevice()->getReleaseHelper());
    EXPECT_NE(0u, grfSizes.size());
    EXPECT_EQ(expectedGrfSizes, grfSizes);

    for (const auto &grfSize : grfSizes) {
        std::string filename = "grf_" + std::to_string(grfSize) + "_kernel_variable_register_per_thread";
        std::string buildFlags = "-ze-exp-register-file-size " + std::to_string(grfSize);
        ze_module_handle_t module = createModuleFromFile(filename, context, device, buildFlags);
        ASSERT_NE(nullptr, module);

        ze_kernel_handle_t kernel;
        ze_kernel_desc_t kernelDescriptor{};
        kernelDescriptor.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
        kernelDescriptor.pKernelName = "kernelVariableRegisterPerThread";
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(module, &kernelDescriptor, &kernel));

        const auto numGrfRequired = Kernel::fromHandle(kernel)->getKernelDescriptor().kernelAttributes.numGrfRequired;
        EXPECT_EQ(grfSize, numGrfRequired);

        ze_command_list_desc_t commandListDescriptor{};
        commandListDescriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
        ze_command_list_handle_t commandList{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context, device, &commandListDescriptor, &commandList));

        const std::vector<int32_t> input(bufferSize, 1);
        const std::vector<int32_t> expectedOutput(bufferSize, 2);

        auto *inputBuffer = allocateDeviceMemory(context, device, bufferSize, 1u);
        auto *outputBuffer = allocateDeviceMemory(context, device, bufferSize, 1u);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(commandList, inputBuffer, input.data(), bufferSize, nullptr, 0u, nullptr));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandList, nullptr, 0u, nullptr));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0u, sizeof(inputBuffer), &inputBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 1u, sizeof(outputBuffer), &outputBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, bufferSize, 1u, 1u));

        ze_group_count_t groupCount{1u, 1u, 1u};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandList, kernel, &groupCount, nullptr, 0u, nullptr));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandList, nullptr, 0u, nullptr));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListClose(commandList));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueExecuteCommandLists(pCmdq, 1u, &commandList, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueSynchronize(pCmdq, UINT64_MAX));

        expectMemory<FamilyType>(outputBuffer, expectedOutput.data(), bufferSize);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemFree(context, inputBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemFree(context, outputBuffer));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(commandList));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(module));
    }
}

} // namespace L0::ult
