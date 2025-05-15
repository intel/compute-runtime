/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
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
        if (module != nullptr) {
            module->destroy();
        }
        AUBFixtureL0::tearDown();
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

HWTEST_F(DebuggerSingleAddressSpaceAub, GivenSingleAddressSpaceWhenCmdListIsExecutedThenSbaAddressesAreTracked) {
    if (neoDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    constexpr size_t bufferSize = MemoryConstants::pageSize;
    const uint32_t groupSize[] = {32, 1, 1};
    const uint32_t groupCount[] = {bufferSize / 32, 1, 1};
    const uint32_t expectedSizes[] = {bufferSize, 1, 1};

    memoryManager = neoDevice->getMemoryManager();
    gmmHelper = neoDevice->getGmmHelper();
    rootDeviceIndex = neoDevice->getRootDeviceIndex();

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

    module = static_cast<L0::ModuleImp *>(Module::fromHandle(createModuleFromFile("test_kernel", context, device, "")));

    ze_kernel_handle_t kernel;
    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_global_sizes";

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(module->toHandle(), &kernelDesc, &kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &bufferDst));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint64_t>::max());

    expectMemory<FamilyType>(reinterpret_cast<void *>(driverHandle->svmAllocsManager->getSVMAlloc(bufferDst)->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()),
                             expectedSizes, sizeof(expectedSizes));

    const auto sbaAddress = device->getL0Debugger()->getSbaTrackingBuffer(csr->getOsContext().getContextId())->getGpuAddress();
    uint32_t low = sbaAddress & 0xffffffff;
    uint32_t high = (sbaAddress >> 32) & 0xffffffff;

    expectMMIO<FamilyType>(DebuggerRegisterOffsets::csGprR15, low);
    expectMMIO<FamilyType>(DebuggerRegisterOffsets::csGprR15 + 4, high);

    auto instructionHeapBaseAddress = memoryManager->getInternalHeapBaseAddress(rootDeviceIndex,
                                                                                memoryManager->isLocalMemoryUsedForIsa(rootDeviceIndex));
    instructionHeapBaseAddress = gmmHelper->canonize(instructionHeapBaseAddress);

    expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, instructionBaseAddress)),
                             &instructionHeapBaseAddress, sizeof(instructionHeapBaseAddress));

    if (neoDevice->getBindlessHeapsHelper()) {
        auto globalBase = neoDevice->getBindlessHeapsHelper()->getGlobalHeapsBase();

        auto surfaceStateBaseAddress = 0ull;
        auto bindlessSurfaceStateBaseAddress = globalBase;
        auto dynamicStateBaseAddress = globalBase;

        expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, surfaceStateBaseAddress)),
                                 &surfaceStateBaseAddress, sizeof(surfaceStateBaseAddress));

        expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, bindlessSurfaceStateBaseAddress)),
                                 &bindlessSurfaceStateBaseAddress, sizeof(bindlessSurfaceStateBaseAddress));

        expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, dynamicStateBaseAddress)),
                                 &dynamicStateBaseAddress, sizeof(dynamicStateBaseAddress));

    } else {
        auto commandListSurfaceHeapAllocation = commandList->commandContainer.getIndirectHeap(HeapType::surfaceState);
        if (commandListSurfaceHeapAllocation) {
            auto surfaceStateBaseAddress = commandListSurfaceHeapAllocation->getGraphicsAllocation()->getGpuAddress();
            surfaceStateBaseAddress = gmmHelper->canonize(surfaceStateBaseAddress);

            expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, surfaceStateBaseAddress)),
                                     &surfaceStateBaseAddress, sizeof(surfaceStateBaseAddress));
            expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, bindlessSurfaceStateBaseAddress)),
                                     &surfaceStateBaseAddress, sizeof(surfaceStateBaseAddress));
        }

        auto commandListDynamicHeapAllocation = commandList->commandContainer.getIndirectHeap(HeapType::dynamicState);
        if (commandListDynamicHeapAllocation) {
            auto dynamicStateBaseAddress = commandListDynamicHeapAllocation->getGraphicsAllocation()->getGpuAddress();
            dynamicStateBaseAddress = gmmHelper->canonize(dynamicStateBaseAddress);

            expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, dynamicStateBaseAddress)),
                                     &dynamicStateBaseAddress, sizeof(dynamicStateBaseAddress));
        }
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    driverHandle->svmAllocsManager->freeSVMAlloc(bufferDst);
}

struct DebuggerSingleAddressSpaceGlobalBindlessAllocatorAubFixture : public DebuggerAubFixture {
    void setUp() {
        NEO::debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
        NEO::debugManager.flags.UseBindlessMode.set(1);
        DebuggerAubFixture::setUp();
    }
    void tearDown() {
        DebuggerAubFixture::tearDown();
    }
};
using DebuggerGlobalAllocatorAub = Test<DebuggerSingleAddressSpaceGlobalBindlessAllocatorAubFixture>;
using PlatformsSupportingGlobalBindless = IsWithinGfxCore<IGFX_XE_HP_CORE, IGFX_XE2_HPG_CORE>;

HWTEST2_F(DebuggerGlobalAllocatorAub, GivenKernelWithScratchWhenCmdListExecutedThenSbaAddressesAreTracked, PlatformsSupportingGlobalBindless) {

    const uint32_t arraySize = 32;
    const uint32_t typeSize = sizeof(int);

    uint32_t bufferSize = (arraySize * 2 + 1) * typeSize - 4;
    const uint32_t groupSize[] = {arraySize, 1, 1};
    const uint32_t groupCount[] = {1, 1, 1};

    memoryManager = neoDevice->getMemoryManager();
    gmmHelper = neoDevice->getGmmHelper();
    rootDeviceIndex = neoDevice->getRootDeviceIndex();

    NEO::debugManager.flags.UpdateCrossThreadDataSize.set(true);

    ASSERT_NE(nullptr, neoDevice->getBindlessHeapsHelper());

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto bufferDst = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);
    memset(bufferDst, 0, bufferSize);
    auto bufferSrc = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);
    memset(bufferSrc, 0, bufferSize);
    auto bufferOffset = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(128 * arraySize, unifiedMemoryProperties);
    memset(bufferOffset, 0, 128 * arraySize);

    int *srcBufferInt = static_cast<int *>(bufferSrc);
    std::unique_ptr<int[]> expectedMemoryInt = std::make_unique<int[]>(bufferSize / typeSize);
    const int expectedVal1 = 16256;
    const int expectedVal2 = 512;

    for (uint32_t i = 0; i < arraySize; ++i) {
        srcBufferInt[i] = 2;
        expectedMemoryInt[i * 2] = expectedVal1;
        expectedMemoryInt[i * 2 + 1] = expectedVal2;
    }

    auto simulatedCsr = AUBFixtureL0::getSimulatedCsr<FamilyType>();
    simulatedCsr->initializeEngine();

    simulatedCsr->writeMemory(*driverHandle->svmAllocsManager->getSVMAlloc(bufferDst)->gpuAllocations.getDefaultGraphicsAllocation());

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = groupCount[0];
    dispatchTraits.groupCountY = groupCount[1];
    dispatchTraits.groupCountZ = groupCount[2];

    module = static_cast<L0::ModuleImp *>(Module::fromHandle(createModuleFromFile("simple_spill_fill_kernel", context, device, "", false)));

    ze_kernel_handle_t kernel;
    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "spill_test";

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(module->toHandle(), &kernelDesc, &kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &bufferSrc));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 1, sizeof(void *), &bufferDst));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 2, sizeof(void *), &bufferOffset));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint64_t>::max());

    expectMemory<FamilyType>(reinterpret_cast<void *>(driverHandle->svmAllocsManager->getSVMAlloc(bufferDst)->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()),
                             expectedMemoryInt.get(), bufferSize);

    const auto sbaAddress = device->getL0Debugger()->getSbaTrackingBuffer(csr->getOsContext().getContextId())->getGpuAddress();
    auto instructionHeapBaseAddress = memoryManager->getInternalHeapBaseAddress(rootDeviceIndex,
                                                                                memoryManager->isLocalMemoryUsedForIsa(rootDeviceIndex));
    instructionHeapBaseAddress = gmmHelper->canonize(instructionHeapBaseAddress);

    expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, instructionBaseAddress)),
                             &instructionHeapBaseAddress, sizeof(instructionHeapBaseAddress));

    auto commandListSurfaceHeapAllocation = commandList->commandContainer.getIndirectHeap(HeapType::surfaceState);

    auto surfaceStateBaseAddress = commandListSurfaceHeapAllocation->getGraphicsAllocation()->getGpuAddress();
    surfaceStateBaseAddress = gmmHelper->canonize(surfaceStateBaseAddress);

    expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, surfaceStateBaseAddress)),
                             &surfaceStateBaseAddress, sizeof(surfaceStateBaseAddress));

    auto bindlessSurfaceStateBaseAddress = neoDevice->getBindlessHeapsHelper()->getGlobalHeapsBase();
    expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, bindlessSurfaceStateBaseAddress)),
                             &bindlessSurfaceStateBaseAddress, sizeof(bindlessSurfaceStateBaseAddress));

    auto commandListDynamicHeapAllocation = commandList->commandContainer.getIndirectHeap(HeapType::dynamicState);
    if (commandListDynamicHeapAllocation) {
        auto dynamicStateBaseAddress = commandListDynamicHeapAllocation->getGraphicsAllocation()->getGpuAddress();
        dynamicStateBaseAddress = gmmHelper->canonize(dynamicStateBaseAddress);

        expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, dynamicStateBaseAddress)),
                                 &bindlessSurfaceStateBaseAddress, sizeof(bindlessSurfaceStateBaseAddress));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    driverHandle->svmAllocsManager->freeSVMAlloc(bufferDst);
    driverHandle->svmAllocsManager->freeSVMAlloc(bufferSrc);
    driverHandle->svmAllocsManager->freeSVMAlloc(bufferOffset);
}

} // namespace ult
} // namespace L0
