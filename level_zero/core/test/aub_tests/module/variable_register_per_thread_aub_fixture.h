/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/driver_experimental/zex_module.h"

#include <algorithm>
#include <string>
#include <vector>

namespace L0::ult {

struct AUBVariableRegisterPerThreadL0 : Test<AUBFixtureL0> {
    struct GrfKernelExecution {
        ze_module_handle_t module = nullptr;
        ze_kernel_handle_t kernel = nullptr;
        void *outputBuffer = nullptr;
    };

    NEO::SupportedNumGrfs getGrfSizes(ze_device_handle_t device) {
        ze_device_module_properties_t deviceModuleProperties{};
        zex_device_module_register_file_exp_t deviceModuleRegisterFile{ZEX_STRUCTURE_DEVICE_MODULE_REGISTER_FILE_EXP};
        deviceModuleProperties.pNext = &deviceModuleRegisterFile;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetModuleProperties(device, &deviceModuleProperties));

        NEO::SupportedNumGrfs result{};
        result.resize(deviceModuleRegisterFile.registerFileSizesCount);
        deviceModuleRegisterFile.registerFileSizes = result.data();
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetModuleProperties(device, &deviceModuleProperties));
        return result;
    }

    NEO::SupportedNumGrfs getVerifiedGrfSizes() {
        const auto grfSizes = getGrfSizes(device);
        const auto &expectedGrfSizes = device->getProductHelper().getSupportedNumGrfs(device->getNEODevice()->getReleaseHelper());
        EXPECT_NE(0u, grfSizes.size());
        EXPECT_EQ(expectedGrfSizes, grfSizes);
        return grfSizes;
    }

    NEO::SupportedNumGrfs selectGrfSizes(const NEO::SupportedNumGrfs &grfSizes, const std::vector<uint32_t> &grfSizesToSelect) {
        NEO::SupportedNumGrfs result{};
        for (const auto &grfSize : grfSizes) {
            if (std::find(grfSizesToSelect.begin(), grfSizesToSelect.end(), grfSize) != grfSizesToSelect.end()) {
                result.push_back(grfSize);
            }
        }
        return result;
    }

    void *allocateDeviceMemory(ze_context_handle_t context, ze_device_handle_t device, size_t size, size_t alignment) {
        void *result = nullptr;
        ze_device_mem_alloc_desc_t descriptor{};
        descriptor.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemAllocDevice(context, &descriptor, size, alignment, device, &result));
        return result;
    }

    template <typename FamilyType>
    void executeKernelsWithGrfSizes(const NEO::SupportedNumGrfs &grfSizes) {
        constexpr uint32_t elementCount = 256u;
        constexpr size_t bufferSize = elementCount * sizeof(int32_t);
        const std::vector<int32_t> input(elementCount, 1);
        const std::vector<int32_t> expectedOutput(elementCount, 2);

        ze_command_list_desc_t commandListDescriptor{};
        commandListDescriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
        ze_command_list_handle_t commandList{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context, device, &commandListDescriptor, &commandList));

        auto *inputBuffer = allocateDeviceMemory(context, device, bufferSize, 1u);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(commandList, inputBuffer, input.data(), bufferSize, nullptr, 0u, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandList, nullptr, 0u, nullptr));

        std::vector<GrfKernelExecution> executions;
        executions.reserve(grfSizes.size());
        for (const auto &grfSize : grfSizes) {
            std::string filename = "grf_" + std::to_string(grfSize) + "_kernel_variable_register_per_thread";
            std::string buildFlags = BuildOptions::registerFileSize.str() + " " + std::to_string(grfSize);

            GrfKernelExecution execution{};
            execution.module = createModuleFromFile(filename, context, device, buildFlags);
            ASSERT_NE(nullptr, execution.module);

            ze_kernel_desc_t kernelDescriptor{};
            kernelDescriptor.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
            kernelDescriptor.pKernelName = "kernelVariableRegisterPerThread";
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(execution.module, &kernelDescriptor, &execution.kernel));

            const auto numGrfRequired = Kernel::fromHandle(execution.kernel)->getKernelDescriptor().kernelAttributes.numGrfRequired;
            EXPECT_EQ(grfSize, numGrfRequired);

            execution.outputBuffer = allocateDeviceMemory(context, device, bufferSize, 1u);
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(execution.kernel, 0u, sizeof(inputBuffer), &inputBuffer));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(execution.kernel, 1u, sizeof(execution.outputBuffer), &execution.outputBuffer));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(execution.kernel, elementCount, 1u, 1u));

            ze_group_count_t groupCount{1u, 1u, 1u};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandList, execution.kernel, &groupCount, nullptr, 0u, nullptr));
            executions.push_back(execution);
        }

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandList, nullptr, 0u, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListClose(commandList));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueExecuteCommandLists(pCmdq, 1u, &commandList, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueSynchronize(pCmdq, UINT64_MAX));

        for (auto &execution : executions) {
            expectMemory<FamilyType>(execution.outputBuffer, expectedOutput.data(), bufferSize);

            EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemFree(context, execution.outputBuffer));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(execution.kernel));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(execution.module));
        }

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemFree(context, inputBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(commandList));
    }
};

} // namespace L0::ult
