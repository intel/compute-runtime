/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/register_offsets.h"
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

struct DebuggerAub : Test<AUBFixtureL0> {

    void SetUp() override {
        AUBFixtureL0::setUp(NEO::defaultHwInfo.get(), true);
    }
    void TearDown() override {

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
    }
    DebugManagerStateRestore restorer;
    ModuleImp *module = nullptr;
};

struct DebuggerSingleAddressSpaceAub : public DebuggerAub {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(1);
        DebuggerAub::SetUp();
    }
    void TearDown() override {

        DebuggerAub::TearDown();
    }
};

using IsBetweenGen12LpAndXeHp = IsWithinGfxCore<IGFX_GEN12LP_CORE, IGFX_XE_HP_CORE>;

HWTEST2_F(DebuggerSingleAddressSpaceAub, GivenSingleAddressSpaceWhenCmdListIsExecutedThenSbaAddressesAreTracked, IsBetweenGen12LpAndXeHp) {
    constexpr size_t bufferSize = MemoryConstants::pageSize;
    const uint32_t groupSize[] = {32, 1, 1};
    const uint32_t groupCount[] = {bufferSize / 32, 1, 1};
    const uint32_t expectedSizes[] = {bufferSize, 1, 1};

    NEO::DebugManager.flags.UpdateCrossThreadDataSize.set(true);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY,
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
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    expectMemory<FamilyType>(reinterpret_cast<void *>(driverHandle->svmAllocsManager->getSVMAlloc(bufferDst)->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()),
                             expectedSizes, sizeof(expectedSizes));

    const auto sbaAddress = device->getL0Debugger()->getSbaTrackingBuffer(csr->getOsContext().getContextId())->getGpuAddress();
    uint32_t low = sbaAddress & 0xffffffff;
    uint32_t high = (sbaAddress >> 32) & 0xffffffff;

    expectMMIO<FamilyType>(CS_GPR_R15, low);
    expectMMIO<FamilyType>(CS_GPR_R15 + 4, high);

    auto instructionHeapBaseAddress = neoDevice->getMemoryManager()->getInternalHeapBaseAddress(device->getRootDeviceIndex(), neoDevice->getMemoryManager()->isLocalMemoryUsedForIsa(neoDevice->getRootDeviceIndex()));
    auto dynamicStateBaseAddress = commandList->commandContainer.getIndirectHeap(HeapType::DYNAMIC_STATE)->getGraphicsAllocation()->getGpuAddress();
    auto surfaceStateBaseAddress = commandList->commandContainer.getIndirectHeap(HeapType::SURFACE_STATE)->getGraphicsAllocation()->getGpuAddress();

    expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, SurfaceStateBaseAddress)),
                             &surfaceStateBaseAddress, sizeof(surfaceStateBaseAddress));

    expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, DynamicStateBaseAddress)),
                             &dynamicStateBaseAddress, sizeof(dynamicStateBaseAddress));

    expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, InstructionBaseAddress)),
                             &instructionHeapBaseAddress, sizeof(instructionHeapBaseAddress));

    expectMemory<FamilyType>(reinterpret_cast<void *>(sbaAddress + offsetof(NEO::SbaTrackedAddresses, BindlessSurfaceStateBaseAddress)),
                             &surfaceStateBaseAddress, sizeof(surfaceStateBaseAddress));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    driverHandle->svmAllocsManager->freeSVMAlloc(bufferDst);
}

} // namespace ult
} // namespace L0
