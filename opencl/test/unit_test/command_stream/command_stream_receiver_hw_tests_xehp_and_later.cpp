/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_scratch_space_controller_xehp_and_later.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/resource_barrier.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"
#include "reg_configs_common.h"

using namespace NEO;

namespace NEO {
template <typename GfxFamily>
class ImplicitFlushSettings {
  public:
    static bool &getSettingForNewResource();
    static bool &getSettingForGpuIdle();

  private:
    static bool defaultSettingForNewResource;
    static bool defaultSettingForGpuIdle;
};
} // namespace NEO

struct CommandStreamReceiverHwTestXeHPAndLater : public ClDeviceFixture,
                                                 public HardwareParse,
                                                 public ::testing::Test {

    void SetUp() override {
        ClDeviceFixture::setUp();
        HardwareParse::setUp();
    }

    void TearDown() override {
        HardwareParse::tearDown();
        ClDeviceFixture::tearDown();
    }
};

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenPreambleSentWhenL3ConfigRequestChangedThenDontProgramL3Register) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    size_t gws = 1;
    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    auto &commandStreamCSR = commandStreamReceiver->getCS();

    PreemptionMode initialPreemptionMode = commandStreamReceiver->lastPreemptionMode;
    PreemptionMode devicePreemptionMode = pDevice->getPreemptionMode();

    commandStreamReceiver->isPreambleSent = true;
    commandStreamReceiver->lastSentL3Config = 0;

    commandQueue.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);

    parseCommands<FamilyType>(commandStreamCSR, 0);
    auto itorCmd = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    if (PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(initialPreemptionMode, devicePreemptionMode) > 0u) {
        ASSERT_NE(cmdList.end(), itorCmd);
    } else {
        EXPECT_EQ(cmdList.end(), itorCmd);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, WhenCommandStreamReceiverHwIsCreatedThenDefaultSshSizeIs2MB) {
    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    EXPECT_EQ(2 * MB, commandStreamReceiver.defaultSshSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, WhenScratchSpaceExistsThenReturnNonZeroGpuAddressToPatch) {
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    void *ssh = alignedMalloc(512, 4096);

    uint32_t perThreadScratchSize = 0x400;

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    commandStreamReceiver->getScratchSpaceController()->setRequiredScratchSpace(ssh, 0u, perThreadScratchSize, 0u, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    ASSERT_NE(nullptr, commandStreamReceiver->getScratchAllocation());
    EXPECT_TRUE(cfeStateDirty);

    auto scratchSpaceAddr = commandStreamReceiver->getScratchPatchAddress();
    constexpr uint64_t notExpectedScratchGpuAddr = 0;
    EXPECT_NE(notExpectedScratchGpuAddr, scratchSpaceAddr);
    alignedFree(ssh);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, WhenOsContextSupportsMultipleDevicesThenScratchSpaceAllocationIsPlacedOnEachSupportedDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2u);
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->memoryManager.reset(new MockMemoryManager(false, true, *executionEnvironment));
    uint32_t tileMask = 0b11;
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular}, PreemptionMode::MidThread, tileMask)));
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*executionEnvironment, 0, tileMask);
    initPlatform();

    void *ssh = alignedMalloc(512, 4096);

    uint32_t perThreadScratchSize = 0x400;

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    commandStreamReceiver->getScratchSpaceController()->setRequiredScratchSpace(ssh, 0u, perThreadScratchSize, 0u, 0u, *osContext, stateBaseAddressDirty, cfeStateDirty);
    auto allocation = commandStreamReceiver->getScratchAllocation();
    EXPECT_EQ(tileMask, static_cast<uint32_t>(allocation->storageInfo.memoryBanks.to_ulong()));
    alignedFree(ssh);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, WhenScratchSpaceNotExistThenReturnZeroGpuAddressToPatch) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    auto scratchSpaceAddr = commandStreamReceiver.getScratchPatchAddress();
    constexpr uint64_t expectedScratchGpuAddr = 0;
    EXPECT_EQ(expectedScratchGpuAddr, scratchSpaceAddr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, whenProgrammingMiSemaphoreWaitThenSetRegisterPollModeMemoryPoll) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    MI_SEMAPHORE_WAIT miSemaphoreWait = FamilyType::cmdInitMiSemaphoreWait;
    EXPECT_EQ(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL, miSemaphoreWait.getRegisterPollMode());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenSratchAllocationRequestedThenProgramCfeStateWithScratchAllocation) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    const HardwareInfo &hwInfo = *defaultHwInfo;
    size_t gws = 1;
    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());
    scratchController->slotId = 2u;
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    auto &commandStreamCSR = commandStreamReceiver->getCS();

    kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 0x1000;
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    uint32_t computeUnits = hwHelper.getComputeUnitsUsedForScratch(&hwInfo);
    size_t scratchSpaceSize = kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0] * computeUnits;

    commandQueue.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);
    commandQueue.flush();

    parseCommands<FamilyType>(commandStreamCSR, 0);
    findHardwareCommands<FamilyType>();

    EXPECT_EQ(kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0], commandStreamReceiver->requiredScratchSize);
    EXPECT_EQ(scratchSpaceSize, scratchController->scratchSizeBytes);
    EXPECT_EQ(scratchSpaceSize, scratchController->getScratchSpaceAllocation()->getUnderlyingBufferSize());
    ASSERT_NE(nullptr, cmdMediaVfeState);
    auto cfeState = static_cast<CFE_STATE *>(cmdMediaVfeState);
    uint32_t bufferOffset = static_cast<uint32_t>(scratchController->slotId * scratchController->singleSurfaceStateSize * 2);
    EXPECT_EQ(bufferOffset, cfeState->getScratchSpaceBuffer());
    RENDER_SURFACE_STATE *scratchState = reinterpret_cast<RENDER_SURFACE_STATE *>(scratchController->surfaceStateHeap + bufferOffset);
    EXPECT_EQ(scratchController->scratchAllocation->getGpuAddress(), scratchState->getSurfaceBaseAddress());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, scratchState->getSurfaceType());

    SURFACE_STATE_BUFFER_LENGTH length = {0};
    length.Length = static_cast<uint32_t>(computeUnits - 1);
    EXPECT_EQ(length.SurfaceState.Depth + 1u, scratchState->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, scratchState->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, scratchState->getHeight());
    EXPECT_EQ(kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0], scratchState->getSurfacePitch());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenNewSshProvidedAndNoScratchAllocationExistThenNoDirtyBitSet) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    scratchController->surfaceStateHeap = reinterpret_cast<char *>(0x1000);
    scratchController->setRequiredScratchSpace(reinterpret_cast<void *>(0x2000), 0u, 0u, 0u, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_EQ(scratchController->surfaceStateHeap, reinterpret_cast<char *>(0x2000));
    EXPECT_FALSE(cfeStateDirty);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenRequiredScratchSpaceIsSetThenPerThreadScratchSizeIsAlignedTo64) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());

    uint32_t perThreadScratchSize = 1;
    uint32_t expectedValue = 1 << 6;
    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    uint8_t surfaceHeap[1000];
    scratchController->setRequiredScratchSpace(surfaceHeap, 0u, perThreadScratchSize, 0u, commandStreamReceiver->taskCount, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_EQ(expectedValue, scratchController->perThreadScratchSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenNewSshProvidedAndScratchAllocationExistsThenSetDirtyBitCopyCurrentState) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());
    scratchController->slotId = 0;
    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;

    void *oldSurfaceHeap = alignedMalloc(0x1000, 0x1000);
    scratchController->setRequiredScratchSpace(oldSurfaceHeap, 0u, 0x1000u, 0u, commandStreamReceiver->taskCount, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(1u, scratchController->slotId);
    EXPECT_EQ(scratchController->surfaceStateHeap, oldSurfaceHeap);
    char *surfaceStateBuf = static_cast<char *>(oldSurfaceHeap) + scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2;
    GraphicsAllocation *scratchAllocation = scratchController->scratchAllocation;
    RENDER_SURFACE_STATE *surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(scratchController->scratchAllocation->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, surfaceState->getSurfaceType());

    void *newSurfaceHeap = alignedMalloc(0x1000, 0x1000);
    scratchController->setRequiredScratchSpace(newSurfaceHeap, 0u, 0x1000u, 0u, commandStreamReceiver->taskCount, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(1u, scratchController->slotId);
    EXPECT_EQ(scratchController->surfaceStateHeap, newSurfaceHeap);
    EXPECT_EQ(scratchAllocation, scratchController->scratchAllocation);
    surfaceStateBuf = static_cast<char *>(newSurfaceHeap) + scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2;
    surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(scratchController->scratchAllocation->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, surfaceState->getSurfaceType());

    alignedFree(oldSurfaceHeap);
    alignedFree(newSurfaceHeap);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenBiggerScratchSpaceRequiredThenReplaceAllocation) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());
    scratchController->slotId = 6;

    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);
    scratchController->setRequiredScratchSpace(surfaceHeap, 0u, 0x1000u, 0u, commandStreamReceiver->taskCount,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(7u, scratchController->slotId);
    uint64_t offset = static_cast<uint64_t>(scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2);
    EXPECT_EQ(offset, scratchController->getScratchPatchAddress());
    EXPECT_EQ(0u, scratchController->calculateNewGSH());
    uint64_t gpuVa = scratchController->scratchAllocation->getGpuAddress();
    char *surfaceStateBuf = static_cast<char *>(scratchController->surfaceStateHeap) + offset;
    RENDER_SURFACE_STATE *surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(gpuVa, surfaceState->getSurfaceBaseAddress());

    scratchController->setRequiredScratchSpace(surfaceHeap, 0u, 0x2000u, 0u, commandStreamReceiver->taskCount,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(8u, scratchController->slotId);
    offset = static_cast<uint64_t>(scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2);
    EXPECT_EQ(offset, scratchController->getScratchPatchAddress());
    EXPECT_NE(gpuVa, scratchController->scratchAllocation->getGpuAddress());
    gpuVa = scratchController->scratchAllocation->getGpuAddress();
    surfaceStateBuf = static_cast<char *>(scratchController->surfaceStateHeap) + offset;
    surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(gpuVa, surfaceState->getSurfaceBaseAddress());

    alignedFree(surfaceHeap);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenScratchSlotIsNonZeroThenSlotIdIsUpdatedAndCorrectOffsetIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());

    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);
    scratchController->setRequiredScratchSpace(surfaceHeap, 1u, 0x1000u, 0u, commandStreamReceiver->taskCount,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(1u, scratchController->slotId);
    EXPECT_TRUE(scratchController->updateSlots);
    uint64_t offset = static_cast<uint64_t>(scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2);
    EXPECT_EQ(offset, scratchController->getScratchPatchAddress());
    EXPECT_EQ(0u, scratchController->calculateNewGSH());
    uint64_t gpuVa = scratchController->scratchAllocation->getGpuAddress();
    char *surfaceStateBuf = static_cast<char *>(scratchController->surfaceStateHeap) + offset;
    RENDER_SURFACE_STATE *surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(gpuVa, surfaceState->getSurfaceBaseAddress());
    alignedFree(surfaceHeap);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenProgramHeapsThenSetReqScratchSpaceAndProgramSurfaceStateAreCalled) {
    class MockScratchSpaceControllerXeHPAndLater : public ScratchSpaceControllerXeHPAndLater {
      public:
        uint32_t requiredScratchSpaceCalledTimes = 0u;
        uint32_t programSurfaceStateCalledTimes = 0u;
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        using ScratchSpaceControllerXeHPAndLater::scratchAllocation;

        void setRequiredScratchSpace(void *sshBaseAddress,
                                     uint32_t scratchSlot,
                                     uint32_t requiredPerThreadScratchSize,
                                     uint32_t requiredPerThreadPrivateScratchSize,
                                     uint32_t currentTaskCount,
                                     OsContext &osContext,
                                     bool &stateBaseAddressDirty,
                                     bool &vfeStateDirty) override {
            requiredScratchSpaceCalledTimes++;
        }

      protected:
        void programSurfaceState() override {
            programSurfaceStateCalledTimes++;
        };
    };

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                         *pDevice->executionEnvironment,
                                                                                                                         *commandStreamReceiver->getInternalAllocationStorage());
    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);
    NEO::GraphicsAllocation heap1(1u, NEO::AllocationType::BUFFER, surfaceHeap, 0u, 0u, 0u, MemoryPool::System4KBPages, 0u);
    NEO::GraphicsAllocation heap2(1u, NEO::AllocationType::BUFFER, surfaceHeap, 0u, 0u, 0u, MemoryPool::System4KBPages, 0u);
    NEO::GraphicsAllocation heap3(1u, NEO::AllocationType::BUFFER, surfaceHeap, 0u, 0u, 0u, MemoryPool::System4KBPages, 0u);
    HeapContainer container;

    container.push_back(&heap1);
    container.push_back(&heap2);
    container.push_back(&heap3);

    scratchController->programHeaps(container, 0u, 1u, 0u, 0u, commandStreamReceiver->getOsContext(), stateBaseAddressDirty, cfeStateDirty);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    EXPECT_EQ(scratch->requiredScratchSpaceCalledTimes, 1u);
    EXPECT_EQ(scratch->programSurfaceStateCalledTimes, 2u);

    alignedFree(surfaceHeap);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchWhenSetNewSshPtrAndChangeIdIsFalseThenSlotIdIsNotChanged) {
    class MockScratchSpaceControllerXeHPAndLater : public ScratchSpaceControllerXeHPAndLater {
      public:
        uint32_t programSurfaceStateCalledTimes = 0u;
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        using ScratchSpaceControllerXeHPAndLater::scratchAllocation;
        using ScratchSpaceControllerXeHPAndLater::slotId;

      protected:
        void programSurfaceState() override {
            programSurfaceStateCalledTimes++;
        };
    };

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                         *pDevice->executionEnvironment,
                                                                                                                         *commandStreamReceiver->getInternalAllocationStorage());

    NEO::GraphicsAllocation graphicsAllocation(1u, NEO::AllocationType::BUFFER, nullptr, 0u, 0u, 0u, MemoryPool::System4KBPages, 0u);

    bool cfeStateDirty = false;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    scratch->slotId = 10;
    scratch->scratchAllocation = &graphicsAllocation;
    scratch->setNewSshPtr(surfaceHeap, cfeStateDirty, false);
    scratch->scratchAllocation = nullptr;
    EXPECT_EQ(10u, scratch->slotId);
    EXPECT_EQ(scratch->programSurfaceStateCalledTimes, 1u);
    EXPECT_TRUE(cfeStateDirty);

    alignedFree(surfaceHeap);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchWhenProgramSurfaceStateAndUpdateSlotIsFalseThenSlotIdIsNotChanged) {
    class MockScratchSpaceControllerXeHPAndLater : public ScratchSpaceControllerXeHPAndLater {
      public:
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        using ScratchSpaceControllerXeHPAndLater::programSurfaceState;
        using ScratchSpaceControllerXeHPAndLater::scratchAllocation;
        using ScratchSpaceControllerXeHPAndLater::slotId;
        using ScratchSpaceControllerXeHPAndLater::surfaceStateHeap;
        using ScratchSpaceControllerXeHPAndLater::updateSlots;
    };

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                         *pDevice->executionEnvironment,
                                                                                                                         *commandStreamReceiver->getInternalAllocationStorage());

    NEO::GraphicsAllocation graphicsAllocation(1u, NEO::AllocationType::BUFFER, nullptr, 0u, 0u, 0u, MemoryPool::System4KBPages, 0u);

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    scratch->surfaceStateHeap = static_cast<char *>(surfaceHeap);
    scratch->slotId = 10;
    scratch->updateSlots = false;
    scratch->scratchAllocation = &graphicsAllocation;
    scratch->programSurfaceState();
    scratch->scratchAllocation = nullptr;
    EXPECT_EQ(10u, scratch->slotId);

    alignedFree(surfaceHeap);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenBiggerPrivateScratchSpaceRequiredThenReplaceAllocation) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[6];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    uint32_t sizeForPrivateScratch = MemoryConstants::pageSize;

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    uint64_t gpuVa = scratchController->privateScratchAllocation->getGpuAddress();
    EXPECT_EQ(gpuVa, surfaceState[3].getSurfaceBaseAddress());

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch * 2, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);

    EXPECT_NE(gpuVa, scratchController->privateScratchAllocation->getGpuAddress());
    EXPECT_EQ(scratchController->privateScratchAllocation->getGpuAddress(), surfaceState[5].getSurfaceBaseAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceControllerWithOnlyPrivateScratchSpaceWhenGettingPatchAddressThenGetCorrectValue) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[6];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    uint32_t sizeForPrivateScratch = MemoryConstants::pageSize;

    EXPECT_EQ(nullptr, scratchController->getScratchSpaceAllocation());
    EXPECT_EQ(nullptr, scratchController->getPrivateScratchSpaceAllocation());

    EXPECT_EQ(0u, scratchController->getScratchPatchAddress());

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    auto expectedPatchAddress = 2 * sizeof(RENDER_SURFACE_STATE);
    EXPECT_EQ(nullptr, scratchController->getScratchSpaceAllocation());
    EXPECT_NE(nullptr, scratchController->getPrivateScratchSpaceAllocation());

    EXPECT_EQ(expectedPatchAddress, scratchController->getScratchPatchAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenNotBiggerPrivateScratchSpaceRequiredThenCfeStateIsNotDirty) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[4];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    uint32_t sizeForPrivateScratch = MemoryConstants::pageSize;

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    uint64_t gpuVa = scratchController->privateScratchAllocation->getGpuAddress();
    cfeStateDirty = false;

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_FALSE(cfeStateDirty);

    EXPECT_EQ(gpuVa, scratchController->privateScratchAllocation->getGpuAddress());
    EXPECT_EQ(gpuVa, surfaceState[3].getSurfaceBaseAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateWithoutPrivateScratchSpaceWhenDoubleAllocationsScratchSpaceIsUsedThenPrivateScratchAddressIsZero) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[4];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    uint32_t sizeForScratch = MemoryConstants::pageSize;

    scratchController->setRequiredScratchSpace(surfaceState, 0u, sizeForScratch, 0u, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(nullptr, scratchController->privateScratchAllocation);

    EXPECT_EQ(0u, surfaceState[3].getSurfaceBaseAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceControllerWhenDebugKeyForPrivateScratchIsDisabledThenThereAre16Slots) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnablePrivateScratchSlot1.set(0);
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());
    EXPECT_EQ(16u, scratchController->stateSlotsCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceControllerWhenDebugKeyForPrivateScratchIsEnabledThenThereAre32Slots) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnablePrivateScratchSlot1.set(1);
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());
    EXPECT_EQ(32u, scratchController->stateSlotsCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenSizeForPrivateScratchSpaceIsMisalignedThenAlignItTo64) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[4];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    uint32_t misalignedSizeForPrivateScratch = MemoryConstants::pageSize + 1;

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;
    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, misalignedSizeForPrivateScratch, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_NE(scratchController->privateScratchSizeBytes, misalignedSizeForPrivateScratch * scratchController->computeUnitsUsedForScratch);
    EXPECT_EQ(scratchController->privateScratchSizeBytes, alignUp(misalignedSizeForPrivateScratch, 64) * scratchController->computeUnitsUsedForScratch);
    EXPECT_EQ(scratchController->privateScratchSizeBytes, scratchController->getPrivateScratchSpaceAllocation()->getUnderlyingBufferSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenDisabledPrivateScratchSpaceWhenSizeForPrivateScratchSpaceIsProvidedThenItIsNotCreated) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnablePrivateScratchSlot1.set(0);
    RENDER_SURFACE_STATE surfaceState[4];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;
    scratchController->setRequiredScratchSpace(surfaceState, 0u, MemoryConstants::pageSize, MemoryConstants::pageSize, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_EQ(0u, scratchController->privateScratchSizeBytes);
    EXPECT_EQ(nullptr, scratchController->getPrivateScratchSpaceAllocation());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenDisabledPrivateScratchSpaceWhenGettingOffsetForSlotThenEachSlotContainsOnlyOneSurfaceState) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnablePrivateScratchSlot1.set(0);
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());
    EXPECT_EQ(sizeof(RENDER_SURFACE_STATE), scratchController->getOffsetToSurfaceState(1u));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenBlockedCacheFlushCmdWhenSubmittingThenDispatchBlockedCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    MockContext context(pClDevice);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->timestampPacketWriteEnabled = true;
    mockCsr->storeFlushedTaskStream = true;

    auto cmdQ0 = clUniquePtr(new MockCommandQueueHw<FamilyType>(&context, pClDevice, nullptr));

    auto &secondEngine = pDevice->getEngine(pDevice->getHardwareInfo().capabilityTable.defaultEngineType, EngineUsage::LowPriority);
    static_cast<UltCommandStreamReceiver<FamilyType> *>(secondEngine.commandStreamReceiver)->timestampPacketWriteEnabled = true;

    auto cmdQ1 = clUniquePtr(new MockCommandQueueHw<FamilyType>(&context, pClDevice, nullptr));
    cmdQ1->gpgpuEngine = &secondEngine;
    cmdQ1->timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
    EXPECT_NE(&cmdQ0->getGpgpuCommandStreamReceiver(), &cmdQ1->getGpgpuCommandStreamReceiver());

    MockTimestampPacketContainer node0(*pDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node1(*pDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event0(cmdQ0.get(), 0, 0, 0); // on the same CSR
    event0.addTimestampPacketNodes(node0);
    Event event1(cmdQ1.get(), 0, 0, 0); // on different CSR
    event1.addTimestampPacketNodes(node1);

    uint32_t numEventsOnWaitlist = 3;

    UserEvent userEvent;
    cl_event waitlist[] = {&event0, &event1, &userEvent};

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr(Buffer::create(&context, 0, MemoryConstants::pageSize, nullptr, retVal));
    cl_resource_barrier_descriptor_intel descriptor = {};
    descriptor.mem_object = buffer.get();
    BarrierCommand barrierCommand(cmdQ0.get(), &descriptor, 1);

    cmdQ0->enqueueResourceBarrier(&barrierCommand, numEventsOnWaitlist, waitlist, nullptr);

    userEvent.setStatus(CL_COMPLETE);

    HardwareParse hwParserCsr;
    HardwareParse hwParserCmdQ;
    LinearStream taskStream(mockCsr->storedTaskStream.get(), mockCsr->storedTaskStreamSize);
    taskStream.getSpace(mockCsr->storedTaskStreamSize);
    hwParserCsr.parseCommands<FamilyType>(mockCsr->commandStream, 0);
    hwParserCmdQ.parseCommands<FamilyType>(taskStream, 0);

    {
        auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end());
        auto expectedQueueSemaphoresCount = 1u;
        if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(pDevice->getHardwareInfo())) {
            expectedQueueSemaphoresCount += 1;
        }
        EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0]));
        EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());

        auto dataAddress = TimestampPacketHelper::getContextEndGpuAddress(*node0.getNode(0));
        EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    }
    {
        auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
        EXPECT_EQ(1u, csrSemaphores.size());
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(csrSemaphores[0]));
        EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());

        auto dataAddress = TimestampPacketHelper::getContextEndGpuAddress(*node1.getNode(0));

        EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    }

    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    EXPECT_EQ(pDevice->getPreemptionMode(), mockCsr->passedDispatchFlags.preemptionMode);

    cmdQ0->isQueueBlocked();
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, WhenOsContextSupportsMultipleDevicesThenCommandStreamReceiverIsMultiOsContextCapable) {
    uint32_t multiDeviceMask = 0b11;
    uint32_t singleDeviceMask = 0b10;
    std::unique_ptr<OsContext> multiDeviceOsContext(OsContext::create(nullptr, 0u,
                                                                      EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular}, PreemptionMode::MidThread,
                                                                                                                   multiDeviceMask)));
    std::unique_ptr<OsContext> singleDeviceOsContext(OsContext::create(nullptr, 0u,
                                                                       EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular}, PreemptionMode::MidThread,
                                                                                                                    singleDeviceMask)));

    EXPECT_EQ(2u, multiDeviceOsContext->getNumSupportedDevices());
    EXPECT_EQ(1u, singleDeviceOsContext->getNumSupportedDevices());

    UltCommandStreamReceiver<FamilyType> commandStreamReceiverMulti(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), multiDeviceMask);
    commandStreamReceiverMulti.callBaseIsMultiOsContextCapable = true;
    EXPECT_TRUE(commandStreamReceiverMulti.isMultiOsContextCapable());
    EXPECT_EQ(2u, commandStreamReceiverMulti.deviceBitfield.count());

    UltCommandStreamReceiver<FamilyType> commandStreamReceiverSingle(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), singleDeviceMask);
    commandStreamReceiverSingle.callBaseIsMultiOsContextCapable = true;
    EXPECT_FALSE(commandStreamReceiverSingle.isMultiOsContextCapable());
    EXPECT_EQ(1u, commandStreamReceiverSingle.deviceBitfield.count());
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenXE_HP_COREDefaultSupportEnabledWhenOsSupportsNewResourceImplicitFlushThenReturnOsSupportValue, IsXeHpCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    EXPECT_TRUE(ImplicitFlushSettings<FamilyType>::getSettingForNewResource());

    VariableBackup<bool> defaultSettingForNewResourceBackup(&ImplicitFlushSettings<FamilyType>::getSettingForNewResource(), true);

    if (commandStreamReceiver.getOSInterface()->newResourceImplicitFlush) {
        EXPECT_TRUE(commandStreamReceiver.checkPlatformSupportsNewResourceImplicitFlush());
    } else {
        EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsNewResourceImplicitFlush());
    }
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenXE_HP_COREDefaultSupportDisabledWhenOsSupportsNewResourceImplicitFlushThenReturnOsSupportValue, IsXeHpCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    VariableBackup<bool> defaultSettingForNewResourceBackup(&ImplicitFlushSettings<FamilyType>::getSettingForNewResource(), false);

    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsNewResourceImplicitFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenPlatformSupportsImplicitFlushForNewResourceWhenCsrIsMultiContextThenExpectNoSupport) {
    VariableBackup<bool> defaultSettingForNewResourceBackup(&ImplicitFlushSettings<FamilyType>::getSettingForNewResource(), true);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);
    commandStreamReceiver.multiOsContextCapable = true;

    EXPECT_TRUE(ImplicitFlushSettings<FamilyType>::getSettingForNewResource());
    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsNewResourceImplicitFlush());
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenXE_HP_COREDefaultSupportEnabledWhenOsSupportsGpuIdleImplicitFlushThenReturnOsSupportValue, IsXeHpCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    EXPECT_TRUE(ImplicitFlushSettings<FamilyType>::getSettingForGpuIdle());

    VariableBackup<bool> defaultSettingForGpuIdleBackup(&ImplicitFlushSettings<FamilyType>::getSettingForGpuIdle(), true);

    if (commandStreamReceiver.getOSInterface()->newResourceImplicitFlush) {
        EXPECT_TRUE(commandStreamReceiver.checkPlatformSupportsGpuIdleImplicitFlush());
    } else {
        EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsGpuIdleImplicitFlush());
    }
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenXE_HP_COREDefaultSupportDisabledWhenOsSupportsGpuIdleImplicitFlushThenReturnOsSupportValue, IsXeHpCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    VariableBackup<bool> defaultSettingForGpuIdleBackup(&ImplicitFlushSettings<FamilyType>::getSettingForGpuIdle(), false);

    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsGpuIdleImplicitFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenPlatformSupportsImplicitFlushForIdleGpuWhenCsrIsMultiContextThenExpectNoSupport) {
    VariableBackup<bool> defaultSettingForGpuIdleBackup(&ImplicitFlushSettings<FamilyType>::getSettingForGpuIdle(), true);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    commandStreamReceiver.multiOsContextCapable = true;

    EXPECT_TRUE(ImplicitFlushSettings<FamilyType>::getSettingForGpuIdle());
    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsGpuIdleImplicitFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenPlatformSupportsImplicitFlushForIdleGpuWhenCsrIsMultiContextAndDirectSubmissionActiveThenExpectSupportTrue) {
    VariableBackup<bool> defaultSettingForGpuIdleBackup(&ImplicitFlushSettings<FamilyType>::getSettingForGpuIdle(), true);
    VariableBackup<bool> backupOsSettingForGpuIdle(&OSInterface::gpuIdleImplicitFlush, true);

    osContext->setDirectSubmissionActive();

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    commandStreamReceiver.multiOsContextCapable = true;

    EXPECT_TRUE(ImplicitFlushSettings<FamilyType>::getSettingForGpuIdle());
    EXPECT_TRUE(commandStreamReceiver.checkPlatformSupportsGpuIdleImplicitFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, whenCreatingWorkPartitionAllocationThenItsPropertiesAreCorrect) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableStaticPartitioning.set(1);
    DebugManager.flags.EnableImplicitScaling.set(1);
    DebugManager.flags.EnableLocalMemory.set(1);
    UltDeviceFactory deviceFactory{1, 2};
    MockDevice &rootDevice = *deviceFactory.rootDevices[0];
    CommandStreamReceiver &csr = rootDevice.getGpgpuCommandStreamReceiver();

    StorageInfo workPartitionAllocationStorageInfo = csr.getWorkPartitionAllocation()->storageInfo;
    EXPECT_EQ(rootDevice.getDeviceBitfield(), workPartitionAllocationStorageInfo.memoryBanks);
    EXPECT_EQ(rootDevice.getDeviceBitfield(), workPartitionAllocationStorageInfo.pageTablesVisibility);
    EXPECT_FALSE(workPartitionAllocationStorageInfo.cloningOfPageTables);
    EXPECT_TRUE(workPartitionAllocationStorageInfo.tileInstanced);
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenXeHpWhenRayTracingEnabledThenDoNotAddCommandBatchBuffer, IsXEHP) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto cmdSize = commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo());
    EXPECT_EQ(0u, cmdSize);
    std::unique_ptr<char> buffer(new char[cmdSize]);

    LinearStream cs(buffer.get(), cmdSize);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.usePerDssBackedBuffer = true;

    commandStreamReceiver.programPerDssBackedBuffer(cs, *pDevice, dispatchFlags);
    EXPECT_EQ(0u, cs.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenStaticPartitionEnabledWhenOnlySinglePartitionUsedThenExpectSinglePipeControlAsBarrier, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    constexpr size_t cmdSize = 256;
    std::unique_ptr<char> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);

    commandStreamReceiver.staticWorkPartitioningEnabled = true;
    commandStreamReceiver.activePartitions = 1;

    size_t estimatedCmdSize = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    EXPECT_EQ(sizeof(PIPE_CONTROL), estimatedCmdSize);

    commandStreamReceiver.programStallingNoPostSyncCommandsForBarrier(cs);
    EXPECT_EQ(estimatedCmdSize, cs.getUsed());

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenStaticPartitionDisabledWhenMultiplePartitionsUsedThenExpectSinglePipeControlAsBarrier, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    constexpr size_t cmdSize = 256;
    std::unique_ptr<char> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);

    commandStreamReceiver.staticWorkPartitioningEnabled = false;
    commandStreamReceiver.activePartitions = 2;

    size_t estimatedCmdSize = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    EXPECT_EQ(sizeof(PIPE_CONTROL), estimatedCmdSize);

    commandStreamReceiver.programStallingNoPostSyncCommandsForBarrier(cs);
    EXPECT_EQ(estimatedCmdSize, cs.getUsed());

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenStaticPartitionEnabledWhenMultiplePartitionsUsedThenExpectImplicitScalingWithoutSelfCleanupBarrier, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    constexpr size_t cmdSize = 256;
    std::unique_ptr<char> buffer(new char[cmdSize]);
    MockGraphicsAllocation allocation(buffer.get(), cmdSize);
    allocation.gpuAddress = 0xFF000;
    LinearStream cs(buffer.get(), cmdSize);
    cs.replaceGraphicsAllocation(&allocation);

    commandStreamReceiver.staticWorkPartitioningEnabled = true;
    commandStreamReceiver.activePartitions = 2;

    size_t expectedSize = sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_BATCH_BUFFER_START) +
                          2 * sizeof(uint32_t);
    size_t estimatedCmdSize = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    EXPECT_EQ(expectedSize, estimatedCmdSize);

    commandStreamReceiver.programStallingNoPostSyncCommandsForBarrier(cs);
    EXPECT_EQ(estimatedCmdSize, cs.getUsed());

    void *cmdBuffer = buffer.get();
    size_t offset = 0;

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(cmdBuffer);
    ASSERT_NE(nullptr, pipeControl);
    offset += sizeof(PIPE_CONTROL);

    MI_ATOMIC *miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miAtomic);
    offset += sizeof(MI_ATOMIC);

    MI_SEMAPHORE_WAIT *miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miSemaphore);
    offset += sizeof(MI_SEMAPHORE_WAIT);

    MI_BATCH_BUFFER_START *bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, bbStart);
    offset += sizeof(MI_BATCH_BUFFER_START);

    uint32_t *data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    EXPECT_EQ(estimatedCmdSize, offset);
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenStaticPartitionEnabledWhenSinglePartitionUsedForPostSyncBarrierThenExpectOnlyPostSyncCommands, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &hwInfo = pDevice->getHardwareInfo();

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    auto &commandStreamCSR = commandStreamReceiver->getCS();

    TagNodeBase *tagNode = commandStreamReceiver->getTimestampPacketAllocator()->getTag();
    uint64_t gpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tagNode);

    TimestampPacketDependencies timestampPacketDependencies;
    timestampPacketDependencies.barrierNodes.add(tagNode);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.barrierTimestampPacketNodes = &timestampPacketDependencies.barrierNodes;

    commandStreamReceiver->staticWorkPartitioningEnabled = true;
    commandStreamReceiver->activePartitions = 1;

    size_t expectedCmdSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(hwInfo);
    size_t estimatedCmdSize = commandStreamReceiver->getCmdSizeForStallingCommands(dispatchFlags);
    EXPECT_EQ(expectedCmdSize, estimatedCmdSize);

    commandStreamReceiver->programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags);
    EXPECT_EQ(estimatedCmdSize, commandStreamCSR.getUsed());

    parseCommands<FamilyType>(commandStreamCSR, 0);
    findHardwareCommands<FamilyType>();
    auto cmdItor = cmdList.begin();

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hwInfo)) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
        ASSERT_NE(nullptr, pipeControl);
        cmdItor++;
        if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hwInfo) > 0) {
            cmdItor++;
        }
    }
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(0u, pipeControl->getImmediateData());
    EXPECT_EQ(gpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenStaticPartitionDisabledWhenMultiplePartitionsUsedForPostSyncBarrierThenExpectOnlyPostSyncCommands, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &hwInfo = pDevice->getHardwareInfo();

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    auto &commandStreamCSR = commandStreamReceiver->getCS();

    TagNodeBase *tagNode = commandStreamReceiver->getTimestampPacketAllocator()->getTag();
    uint64_t gpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tagNode);

    TimestampPacketDependencies timestampPacketDependencies;
    timestampPacketDependencies.barrierNodes.add(tagNode);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.barrierTimestampPacketNodes = &timestampPacketDependencies.barrierNodes;

    commandStreamReceiver->staticWorkPartitioningEnabled = false;
    commandStreamReceiver->activePartitions = 2;

    size_t expectedCmdSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(hwInfo);
    size_t estimatedCmdSize = commandStreamReceiver->getCmdSizeForStallingCommands(dispatchFlags);
    EXPECT_EQ(expectedCmdSize, estimatedCmdSize);

    commandStreamReceiver->programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags);
    EXPECT_EQ(estimatedCmdSize, commandStreamCSR.getUsed());

    parseCommands<FamilyType>(commandStreamCSR, 0);
    findHardwareCommands<FamilyType>();
    auto cmdItor = cmdList.begin();

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hwInfo)) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
        ASSERT_NE(nullptr, pipeControl);
        cmdItor++;
        if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hwInfo) > 0) {
            cmdItor++;
        }
    }
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(0u, pipeControl->getImmediateData());
    EXPECT_EQ(gpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
}

HWTEST2_F(CommandStreamReceiverHwTestXeHPAndLater, givenStaticPartitionEnabledWhenMultiplePartitionsUsedThenExpectImplicitScalingPostSyncBarrierWithoutSelfCleanup, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto &hwInfo = pDevice->getHardwareInfo();

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    auto &commandStreamCSR = commandStreamReceiver->getCS();

    TagNodeBase *tagNode = commandStreamReceiver->getTimestampPacketAllocator()->getTag();
    uint64_t gpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tagNode);

    TimestampPacketDependencies timestampPacketDependencies;
    timestampPacketDependencies.barrierNodes.add(tagNode);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.barrierTimestampPacketNodes = &timestampPacketDependencies.barrierNodes;

    commandStreamReceiver->staticWorkPartitioningEnabled = true;
    commandStreamReceiver->activePartitions = 2;

    size_t expectedSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(hwInfo) +
                          sizeof(MI_ATOMIC) + sizeof(MI_SEMAPHORE_WAIT) +
                          sizeof(MI_BATCH_BUFFER_START) +
                          2 * sizeof(uint32_t);
    size_t estimatedCmdSize = commandStreamReceiver->getCmdSizeForStallingCommands(dispatchFlags);
    EXPECT_EQ(expectedSize, estimatedCmdSize);

    commandStreamReceiver->programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags);
    EXPECT_EQ(estimatedCmdSize, commandStreamCSR.getUsed());
    EXPECT_EQ(2u, tagNode->getPacketsUsed());

    parseCommands<FamilyType>(commandStreamCSR, 0);
    findHardwareCommands<FamilyType>();
    auto cmdItor = cmdList.begin();

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hwInfo)) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
        ASSERT_NE(nullptr, pipeControl);
        cmdItor++;
        if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hwInfo) > 0) {
            cmdItor++;
        }
    }
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(0u, pipeControl->getImmediateData());
    EXPECT_EQ(gpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_TRUE(pipeControl->getWorkloadPartitionIdOffsetEnable());
    cmdItor++;

    if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hwInfo) > 0) {
        cmdItor++;
    }

    MI_ATOMIC *miAtomic = genCmdCast<MI_ATOMIC *>(*cmdItor);
    ASSERT_NE(nullptr, miAtomic);
    cmdItor++;

    MI_SEMAPHORE_WAIT *miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*cmdItor);
    ASSERT_NE(nullptr, miSemaphore);
    cmdItor++;

    MI_BATCH_BUFFER_START *bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmdItor);
    ASSERT_NE(nullptr, bbStart);
}
