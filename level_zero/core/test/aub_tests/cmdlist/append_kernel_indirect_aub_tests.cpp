/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using AUBAppendKernelIndirectL0 = Test<AUBFixtureL0>;

TEST_F(AUBAppendKernelIndirectL0, whenAppendKernelIndirectThenGlobalWorkSizeIsProperlyProgrammed) {
    const uint32_t groupSize[] = {1, 2, 3};
    const uint32_t groupCount[] = {4, 3, 1};
    const uint32_t expectedGlobalWorkSize[] = {groupSize[0] * groupCount[0],
                                               groupSize[1] * groupCount[1],
                                               groupSize[2] * groupCount[2]};
    uint8_t size = 3 * sizeof(uint32_t);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto pDispatchTraits = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(sizeof(ze_group_count_t), unifiedMemoryProperties);

    auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    memset(outBuffer, 0, size);

    ze_group_count_t &dispatchTraits = *reinterpret_cast<ze_group_count_t *>(pDispatchTraits);
    dispatchTraits.groupCountX = groupCount[0];
    dispatchTraits.groupCountY = groupCount[1];
    dispatchTraits.groupCountZ = groupCount[2];

    ze_module_handle_t moduleHandle = createModuleFromFile("test_kernel", context, device, "");
    ASSERT_NE(nullptr, moduleHandle);
    ze_kernel_handle_t kernel;

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_global_sizes";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &outBuffer));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelIndirect(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(csr->expectMemory(outBuffer, expectedGlobalWorkSize, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(moduleHandle));
    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
    driverHandle->svmAllocsManager->freeSVMAlloc(pDispatchTraits);
}

TEST_F(AUBAppendKernelIndirectL0, whenAppendKernelIndirectThenGroupCountIsProperlyProgrammed) {
    const uint32_t groupSize[] = {1, 2, 3};
    const uint32_t groupCount[] = {4, 3, 1};
    uint8_t size = 3 * sizeof(uint32_t);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto pDispatchTraits = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(sizeof(ze_group_count_t), unifiedMemoryProperties);

    auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    memset(outBuffer, 0, size);

    ze_group_count_t &dispatchTraits = *reinterpret_cast<ze_group_count_t *>(pDispatchTraits);
    dispatchTraits.groupCountX = groupCount[0];
    dispatchTraits.groupCountY = groupCount[1];
    dispatchTraits.groupCountZ = groupCount[2];

    ze_module_handle_t moduleHandle = createModuleFromFile("test_kernel", context, device, "");
    ASSERT_NE(nullptr, moduleHandle);
    ze_kernel_handle_t kernel;

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_group_count";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &outBuffer));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelIndirect(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(csr->expectMemory(outBuffer, groupCount, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(moduleHandle));
    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
    driverHandle->svmAllocsManager->freeSVMAlloc(pDispatchTraits);
}

TEST_F(AUBAppendKernelIndirectL0, whenAppendMultipleKernelsIndirectThenGroupCountIsProperlyProgrammed) {
    const uint32_t groupSize[] = {1, 2, 3};
    const uint32_t groupCount[] = {4, 3, 1};
    const uint32_t groupCount2[] = {7, 6, 4};
    uint8_t size = 3 * sizeof(uint32_t);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto pDispatchTraits = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(2 * sizeof(ze_group_count_t), unifiedMemoryProperties);

    auto kernelCount = reinterpret_cast<uint32_t *>(driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(sizeof(uint32_t), unifiedMemoryProperties));
    kernelCount[0] = 2;

    auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    auto outBuffer2 = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    memset(outBuffer, 0, size);
    memset(outBuffer2, 0, size);

    ze_group_count_t *dispatchTraits = reinterpret_cast<ze_group_count_t *>(pDispatchTraits);
    dispatchTraits[0].groupCountX = groupCount[0];
    dispatchTraits[0].groupCountY = groupCount[1];
    dispatchTraits[0].groupCountZ = groupCount[2];

    dispatchTraits[1].groupCountX = groupCount2[0];
    dispatchTraits[1].groupCountY = groupCount2[1];
    dispatchTraits[1].groupCountZ = groupCount2[2];

    ze_module_handle_t moduleHandle = createModuleFromFile("test_kernel", context, device, "");
    ASSERT_NE(nullptr, moduleHandle);
    ze_kernel_handle_t kernels[2];

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_group_count";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernels[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernels[0], 0, sizeof(void *), &outBuffer));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernels[0], groupSize[0], groupSize[1], groupSize[2]));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernels[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernels[1], 0, sizeof(void *), &outBuffer2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernels[1], groupSize[0], groupSize[1], groupSize[2]));

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchMultipleKernelsIndirect(cmdListHandle, 2, kernels, kernelCount, dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(csr->expectMemory(outBuffer, groupCount, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));
    EXPECT_TRUE(csr->expectMemory(outBuffer2, groupCount2, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernels[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernels[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(moduleHandle));
    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer2);
    driverHandle->svmAllocsManager->freeSVMAlloc(kernelCount);
    driverHandle->svmAllocsManager->freeSVMAlloc(pDispatchTraits);
}

TEST_F(AUBAppendKernelIndirectL0, whenAppendKernelIndirectThenWorkDimIsProperlyProgrammed) {
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    std::tuple<std::array<uint32_t, 3> /*groupSize*/, std::array<uint32_t, 3> /*groupCount*/, uint32_t /*expected workdim*/> testData[]{
        {{1, 1, 1}, {1, 1, 1}, 1},
        {{1, 1, 1}, {2, 1, 1}, 1},
        {{1, 1, 1}, {1, 2, 1}, 2},
        {{1, 1, 1}, {1, 1, 2}, 3},
        {{2, 1, 1}, {1, 1, 1}, 1},
        {{2, 1, 1}, {2, 1, 1}, 1},
        {{2, 1, 1}, {1, 2, 1}, 2},
        {{2, 1, 1}, {1, 1, 2}, 3},
        {{1, 2, 1}, {1, 1, 1}, 2},
        {{1, 2, 1}, {2, 1, 1}, 2},
        {{1, 2, 1}, {1, 2, 1}, 2},
        {{1, 2, 1}, {1, 1, 2}, 3},
        {{1, 1, 2}, {1, 1, 1}, 3},
        {{1, 1, 2}, {2, 1, 1}, 3},
        {{1, 1, 2}, {1, 2, 1}, 3},
        {{1, 1, 2}, {1, 1, 2}, 3}};

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    ze_module_handle_t moduleHandle = createModuleFromFile("test_kernel", context, device, "");
    ASSERT_NE(nullptr, moduleHandle);
    ze_kernel_handle_t kernel;

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_work_dim";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernel));
    for (auto i = 0u; i < arrayCount<>(testData); i++) {
        std::array<uint32_t, 3> groupSize;
        std::array<uint32_t, 3> groupCount;
        uint32_t expectedWorkDim;

        std::tie(groupSize, groupCount, expectedWorkDim) = testData[i];

        uint8_t size = sizeof(uint32_t);

        auto pDispatchTraits = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(sizeof(ze_group_count_t), unifiedMemoryProperties);

        auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

        memset(outBuffer, 0, size);

        ze_group_count_t &dispatchTraits = *reinterpret_cast<ze_group_count_t *>(pDispatchTraits);
        dispatchTraits.groupCountX = groupCount[0];
        dispatchTraits.groupCountY = groupCount[1];
        dispatchTraits.groupCountZ = groupCount[2];

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &outBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelIndirect(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
        commandList->close();

        pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
        pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

        EXPECT_TRUE(csr->expectMemory(outBuffer, &expectedWorkDim, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

        driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
        driverHandle->svmAllocsManager->freeSVMAlloc(pDispatchTraits);
        commandList->reset();
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(moduleHandle));
}

} // namespace ult
} // namespace L0
