/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/aub_tests/fixtures/multicontext_l0_aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

#include <optional>

extern std::optional<uint32_t> blitterMaskOverride;

namespace L0 {
namespace ult {
struct SimpleMultiTileFixture : public MulticontextL0AubFixture {
    virtual void setUp() {
        MulticontextL0AubFixture::setUp(2, EnabledCommandStreamers::single, true);

        if (skipped || !rootDevice->isImplicitScalingCapable()) {
            GTEST_SKIP();
        }

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        ASSERT_NE(nullptr, hContext);
        context.reset(static_cast<ContextImp *>(Context::fromHandle(hContext)));

        ze_module_handle_t hModule = AUBFixtureL0::createModuleFromFile("test_kernel", context.get(), rootDevice, "");
        ASSERT_NE(nullptr, hModule);
        module.reset(Module::fromHandle(hModule));

        ze_kernel_handle_t hKernel;
        ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernelDesc.pKernelName = "test_get_global_sizes";
        zeKernelCreate(hModule, &kernelDesc, &hKernel);
        ASSERT_NE(nullptr, hKernel);
        kernel.reset(Kernel::fromHandle(hKernel));

        ze_result_t returnValue;
        commandList.reset(ult::CommandList::whiteboxCast(CommandList::create(rootDevice->getHwInfo().platform.eProductFamily, rootDevice, NEO::EngineGroupType::compute, 0u, returnValue, false)));
        ASSERT_NE(nullptr, commandList.get());

        ze_command_queue_desc_t queueDesc = {};
        cmdQ.reset(CommandQueue::create(rootDevice->getHwInfo().platform.eProductFamily, rootDevice, rootDevice->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
        ASSERT_NE(nullptr, cmdQ.get());
    }

    DestroyableZeUniquePtr<ContextImp> context;
    DestroyableZeUniquePtr<Module> module;
    DestroyableZeUniquePtr<Kernel> kernel;
    DestroyableZeUniquePtr<ult::WhiteBox<L0::CommandListImp>> commandList;
    DestroyableZeUniquePtr<CommandQueue> cmdQ;
};

struct SynchronizedDispatchMultiTileFixture : public SimpleMultiTileFixture {
    void setUp() override {
        debugManager.flags.ForceSynchronizedDispatchMode.set(1);
        if (blitterMaskOverride.has_value()) {
            debugManager.flags.BlitterEnableMaskOverride.set(blitterMaskOverride.value());
        }
        SimpleMultiTileFixture::setUp();
    }
};

using SynchronizedDispatchMultiTileL0AubTests = Test<SynchronizedDispatchMultiTileFixture>;

HWTEST_F(SynchronizedDispatchMultiTileL0AubTests, givenFullSyncDispatchWhenExecutingThenDataIsCorrect) {
    if (!rootDevice->isImplicitScalingCapable()) {
        GTEST_SKIP();
    }

    constexpr uint8_t size = 3 * sizeof(uint32_t);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, context->rootDeviceIndices, context->deviceBitfields);

    auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    memset(outBuffer, 0, size);

    ze_group_count_t groupCount = {};
    groupCount.groupCountX = 128;
    groupCount.groupCountY = 1;
    groupCount.groupCountZ = 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel.get(), 0, sizeof(void *), &outBuffer));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel.get(), 1, 1, 1));
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(cmdListHandle, kernel.get(), &groupCount, nullptr, 0, nullptr));
    commandList->close();

    cmdQ->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    cmdQ->synchronize(std::numeric_limits<uint32_t>::max());

    auto csr = getSimulatedCsr<FamilyType>(0, 0);
    rootDevice->getNEODevice()->getDefaultEngine().commandStreamReceiver->pollForCompletion();

    const uint32_t expectedGlobalWorkSize[3] = {128, 1, 1};
    uint64_t expectedTokenValue = 0;

    auto compareEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual;

    EXPECT_TRUE(csr->expectMemory(outBuffer, expectedGlobalWorkSize, size, compareEqual));
    EXPECT_TRUE(csr->expectMemory(reinterpret_cast<void *>(rootDevice->getSyncDispatchTokenAllocation()->getGpuAddress()), &expectedTokenValue, sizeof(uint64_t), compareEqual));

    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
}

struct CopyOffloadMultiTileFixture : public SimpleMultiTileFixture {
    void setUp() override {
        debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(1);
        debugManager.flags.ForceInOrderImmediateCmdListExecution.set(1);
        if (blitterMaskOverride.has_value()) {
            debugManager.flags.BlitterEnableMaskOverride.set(blitterMaskOverride.value());
        }
        SimpleMultiTileFixture::setUp();
    }
};

using CopyOffloadMultiTileL0AubTests = Test<CopyOffloadMultiTileFixture>;

HWTEST2_F(CopyOffloadMultiTileL0AubTests, givenCopyOffloadCmdListWhenDispatchingThenDataIsCorrect, IsAtLeastXeCore) {
    if (!rootDevice->isImplicitScalingCapable()) {
        GTEST_SKIP();
    }

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    commandList.reset(ult::CommandList::whiteboxCast(CommandList::createImmediate(rootDevice->getHwInfo().platform.eProductFamily, rootDevice, &queueDesc, false, NEO::EngineGroupType::compute, returnValue)));
    ASSERT_TRUE(commandList->isCopyOffloadEnabled());

    commandList->getCsr(true)->overrideDispatchPolicy(NEO::DispatchMode::immediateDispatch);

    constexpr uint8_t size = 3 * sizeof(uint32_t);
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, context->rootDeviceIndices, context->deviceBitfields);
    auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    auto readBackBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    auto fillPtr = reinterpret_cast<uint32_t *>(outBuffer);

    std::fill(fillPtr, ptrOffset(fillPtr, size), uint32_t(1));
    memset(readBackBuffer, 0, size);

    uint32_t hostData[3] = {0, 0, 0};

    zeCommandListAppendMemoryCopy(commandList->toHandle(), outBuffer, hostData, size, nullptr, 0, nullptr);

    ze_group_count_t groupCount = {};
    groupCount.groupCountX = 128;
    groupCount.groupCountY = 1;
    groupCount.groupCountZ = 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel.get(), 0, sizeof(void *), &outBuffer));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel.get(), 1, 1, 1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandList->toHandle(), kernel.get(), &groupCount, nullptr, 0, nullptr));

    zeCommandListAppendMemoryCopy(commandList->toHandle(), readBackBuffer, outBuffer, size, nullptr, 0, nullptr);

    zeCommandListHostSynchronize(commandList->toHandle(), std::numeric_limits<uint32_t>::max());
    commandList->getCsr(true)->pollForCompletion();

    const uint32_t expectedGlobalWorkSize[3] = {groupCount.groupCountX, groupCount.groupCountY, groupCount.groupCountZ};

    auto compareEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual;

    auto csr = getSimulatedCsr<FamilyType>(0, 0);

    EXPECT_TRUE(csr->expectMemory(readBackBuffer, expectedGlobalWorkSize, size, compareEqual));

    uint64_t expectedCounterValue = commandList->inOrderExecInfo->getCounterValue();

    for (uint32_t i = 0; i < commandList->inOrderExecInfo->getNumDevicePartitionsToWait(); i++) {
        uint64_t offset = i * rootDevice->getL0GfxCoreHelper().getImmediateWritePostSyncOffset();
        EXPECT_TRUE(csr->expectMemory(reinterpret_cast<void *>(commandList->inOrderExecInfo->getBaseDeviceAddress() + offset), &expectedCounterValue, sizeof(uint64_t), compareEqual));
    }

    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
    driverHandle->svmAllocsManager->freeSVMAlloc(readBackBuffer);
}
} // namespace ult
} // namespace L0
