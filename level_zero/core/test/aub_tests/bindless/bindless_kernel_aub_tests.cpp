/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace L0 {
namespace ult {

struct L0BindlessAub : Test<AUBFixtureL0> {

    void SetUp() override {
        DebugManager.flags.UseBindlessMode.set(1);
        DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
        AUBFixtureL0::SetUp();
    }
    void TearDown() override {

        module->destroy();
        AUBFixtureL0::TearDown();
    }

    void createModuleFromFile(const std::string &fileName, ze_context_handle_t context, L0::Device *device) {
        std::string testFile;
        retrieveBinaryKernelFilenameApiSpecific(testFile, fileName + "_", ".bin");

        size_t size = 0;
        auto src = loadDataFromFile(
            testFile.c_str(),
            size);

        ASSERT_NE(0u, size);
        ASSERT_NE(nullptr, src);

        ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
        moduleDesc.inputSize = size;
        moduleDesc.pBuildFlags = "";

        module = new ModuleImp(device, nullptr, ModuleType::User);
        bool success = module->initialize(&moduleDesc, device->getNEODevice());
        ASSERT_TRUE(success);
    }
    DebugManagerStateRestore restorer;
    ModuleImp *module = nullptr;
};

HWTEST_F(L0BindlessAub, GivenBindlessKernelWhenExecutedThenOutputIsCorrect) {
    constexpr size_t bufferSize = MemoryConstants::pageSize;
    const uint32_t groupSize[] = {32, 1, 1};
    const uint32_t groupCount[] = {bufferSize / 32, 1, 1};

    NEO::DebugManager.flags.UpdateCrossThreadDataSize.set(true);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto bufferSrc = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);
    memset(bufferSrc, 55, bufferSize);

    auto bufferDst = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);
    memset(bufferDst, 0, bufferSize);

    auto simulatedCsr = AUBFixtureL0::getSimulatedCsr<FamilyType>();
    simulatedCsr->initializeEngine();

    simulatedCsr->writeMemory(*driverHandle->svmAllocsManager->getSVMAlloc(bufferSrc)->gpuAllocations.getDefaultGraphicsAllocation());
    simulatedCsr->writeMemory(*driverHandle->svmAllocsManager->getSVMAlloc(bufferDst)->gpuAllocations.getDefaultGraphicsAllocation());

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = groupCount[0];
    dispatchTraits.groupCountY = groupCount[1];
    dispatchTraits.groupCountZ = groupCount[2];

    createModuleFromFile("bindless_stateful_copy_buffer", context, device);

    ze_kernel_handle_t kernel;
    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "StatefulCopyBuffer";

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(module->toHandle(), &kernelDesc, &kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &bufferSrc));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 1, sizeof(void *), &bufferDst));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    expectMemory<FamilyType>(reinterpret_cast<void *>(driverHandle->svmAllocsManager->getSVMAlloc(bufferDst)->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()),
                             bufferSrc, bufferSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    driverHandle->svmAllocsManager->freeSVMAlloc(bufferSrc);
    driverHandle->svmAllocsManager->freeSVMAlloc(bufferDst);
}

} // namespace ult
} // namespace L0
