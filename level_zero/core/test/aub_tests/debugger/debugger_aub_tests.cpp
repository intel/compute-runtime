/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

struct DebuggerAubFixture : AUBFixtureL0 {

    void setUp() {
        AUBFixtureL0::setUp(NEO::defaultHwInfo.get(), true);
    }
    void tearDown() {

        module->destroy();
        AUBFixtureL0::tearDown();
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
        ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
        result = module->initialize(&moduleDesc, device->getNEODevice());
        ASSERT_EQ(result, ZE_RESULT_SUCCESS);

        memoryManager = neoDevice->getMemoryManager();
        gmmHelper = neoDevice->getGmmHelper();
        rootDeviceIndex = neoDevice->getRootDeviceIndex();
    }

    DebugManagerStateRestore restorer;

    ModuleImp *module = nullptr;
    NEO::GmmHelper *gmmHelper = nullptr;
    NEO::MemoryManager *memoryManager = nullptr;

    uint32_t rootDeviceIndex = 0;
};

struct DebuggerSingleAddressSpaceAubFixture : public DebuggerAubFixture {
    void setUp() {
        NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(1);
        DebuggerAubFixture::setUp();
    }
    void tearDown() {
        DebuggerAubFixture::tearDown();
    }
};
using DebuggerSingleAddressSpaceAub = Test<DebuggerSingleAddressSpaceAubFixture>;

using PlatformsSupportingSingleAddressSpace = IsAtLeastGen12lp;

HWTEST2_F(DebuggerSingleAddressSpaceAub, GivenSingleAddressSpaceWhenCmdListIsExecutedThenSbaAddressesAreTracked, PlatformsSupportingSingleAddressSpace) {
    constexpr size_t bufferSize = MemoryConstants::pageSize;
    const uint32_t groupSize[] = {32, 1, 1};
    const uint32_t groupCount[] = {bufferSize / 32, 1, 1};
    const uint32_t expectedSizes[] = {bufferSize, 1, 1};

    NEO::debugManager.flags.UpdateCrossThreadDataSize.set(true);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto bufferDst = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);
    memset(bufferDst, 0, bufferSize);

    auto simulatedCsr = AUBFixtureL0::getSimulatedCsr<FamilyType>();
    simulatedCsr->initializeEngine();

    simulatedCsr->writeMemory(*driverHandle->svmAllocsManager->getSVMAlloc(bufferDst)->gpuAllocations.getDefaultGraphicsAllocation());

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = groupCount[0];
    dispatchTraits.groupCountY = groupCount[1];
    dispatchTraits.groupCountZ = groupCount[2];

    createModuleFromFile("test_kernel", context, device);

    ze_kernel_handle_t kernel;
    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_global_sizes";

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(module->toHandle(), &kernelDesc, &kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &bufferDst));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false);
    pCmdq->synchronize(std::numeric_limits<uint64_t>::max());

    expectMemory<FamilyType>(reinterpret_cast<void *>(driverHandle->svmAllocsManager->getSVMAlloc(bufferDst)->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()),
                             expectedSizes, sizeof(expectedSizes));

    const auto sbaAddress = device->getL0Debugger()->getSbaTrackingBuffer(csr->getOsContext().getContextId())->getGpuAddress();
    uint32_t low = sbaAddress & 0xffffffff;
    uint32_t high = (sbaAddress >> 32) & 0xffffffff;

    expectMMIO<FamilyType>(RegisterOffsets::csGprR15, low);
    expectMMIO<FamilyType>(RegisterOffsets::csGprR15 + 4, high);

    auto instructionHeapBaseAddress = memoryManager->getInternalHeapBaseAddress(rootDeviceIndex,
                                                                                memoryManager->isLocalMemoryUsedForIsa(rootDeviceIndex));
    instructionHeapBaseAddress = gmmHelper->canonize(instructionHeapBaseAddress);

    expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, instructionBaseAddress)),
                             &instructionHeapBaseAddress, sizeof(instructionHeapBaseAddress));

    auto commandListSurfaceHeapAllocation = commandList->commandContainer.getIndirectHeap(HeapType::SURFACE_STATE);
    if (commandListSurfaceHeapAllocation) {
        auto surfaceStateBaseAddress = commandListSurfaceHeapAllocation->getGraphicsAllocation()->getGpuAddress();
        surfaceStateBaseAddress = gmmHelper->canonize(surfaceStateBaseAddress);

        expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, surfaceStateBaseAddress)),
                                 &surfaceStateBaseAddress, sizeof(surfaceStateBaseAddress));
        expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, bindlessSurfaceStateBaseAddress)),
                                 &surfaceStateBaseAddress, sizeof(surfaceStateBaseAddress));
    }

    auto commandListDynamicHeapAllocation = commandList->commandContainer.getIndirectHeap(HeapType::DYNAMIC_STATE);
    if (commandListDynamicHeapAllocation) {
        auto dynamicStateBaseAddress = commandListDynamicHeapAllocation->getGraphicsAllocation()->getGpuAddress();
        dynamicStateBaseAddress = gmmHelper->canonize(dynamicStateBaseAddress);

        expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, dynamicStateBaseAddress)),
                                 &dynamicStateBaseAddress, sizeof(dynamicStateBaseAddress));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    driverHandle->svmAllocsManager->freeSVMAlloc(bufferDst);
}

} // namespace ult
} // namespace L0
