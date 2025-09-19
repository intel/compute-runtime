/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
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
#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

namespace NEO {
class InternalAllocationStorage;
} // namespace NEO

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

template <template <typename> class CsrType>
struct CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrT
    : public CommandStreamReceiverHwTestXeHPAndLater {
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<CsrType<FamilyType>>();

        CommandStreamReceiverHwTestXeHPAndLater::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        CommandStreamReceiverHwTestXeHPAndLater::TearDown();
    }
};

using CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw = CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrT<MockCsrHw>;
using CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw2 = CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrT<MockCsrHw2>;

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenPreambleSentWhenL3ConfigRequestChangedThenDontProgramL3Register) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    size_t gws = 1;
    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

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
    EXPECT_EQ(2 * MemoryConstants::megaByte, commandStreamReceiver.defaultSshSize);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, WhenScratchSpaceExistsThenReturnNonZeroGpuAddressToPatch) {
    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    void *ssh = alignedMalloc(512, 4096);

    uint32_t perThreadScratchSize = 0x400;

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    commandStreamReceiver->getScratchSpaceController()->setRequiredScratchSpace(ssh, 0u, perThreadScratchSize, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    ASSERT_NE(nullptr, commandStreamReceiver->getScratchAllocation());
    EXPECT_TRUE(cfeStateDirty);

    auto scratchSpaceAddr = commandStreamReceiver->getScratchPatchAddress();
    constexpr uint64_t notExpectedScratchGpuAddr = 0;
    EXPECT_NE(notExpectedScratchGpuAddr, scratchSpaceAddr);
    alignedFree(ssh);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, WhenOsContextSupportsMultipleDevicesThenScratchSpaceAllocationIsPlacedOnEachSupportedDevice) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2u);
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0));
    executionEnvironment->memoryManager.reset(new MockMemoryManager(false, true, *executionEnvironment));
    uint32_t tileMask = 0b11;
    uint32_t rootDeviceIndex = 0;
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, rootDeviceIndex, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::MidThread, tileMask)));
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*executionEnvironment, rootDeviceIndex, tileMask);

    void *ssh = alignedMalloc(512, 4096);

    uint32_t perThreadScratchSize = 0x400;

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    commandStreamReceiver->getScratchSpaceController()->setRequiredScratchSpace(ssh, 0u, perThreadScratchSize, 0u, *osContext, stateBaseAddressDirty, cfeStateDirty);
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

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenScratchSpaceSurfaceStateEnabledWhenScratchAllocationRequestedThenProgramCfeStateWithScratchAllocation) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
            GTEST_SKIP();
        }

        using CFE_STATE = typename FamilyType::CFE_STATE;
        using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

        size_t gws = 1;
        MockContext ctx(pClDevice);
        MockKernelWithInternals kernel(*pClDevice);
        CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
        auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
        auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());
        scratchController->slotId = 2u;
        auto &commandStreamCSR = commandStreamReceiver->getCS();

        kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 0x1000;
        auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
        uint32_t computeUnits = gfxCoreHelper.getComputeUnitsUsedForScratch(pDevice->getRootDeviceEnvironment());
        auto perThreadScratchSize = kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0];

        size_t scratchSpaceSize = perThreadScratchSize * computeUnits;

        auto &productHelper = pDevice->getProductHelper();
        productHelper.adjustScratchSize(scratchSpaceSize);

        commandQueue.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);
        commandQueue.flush();

        parseCommands<FamilyType>(commandStreamCSR, 0);
        findHardwareCommands<FamilyType>();

        EXPECT_EQ(kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0], commandStreamReceiver->requiredScratchSlot0Size);
        EXPECT_EQ(scratchSpaceSize, scratchController->scratchSlot0SizeInBytes);
        EXPECT_EQ(scratchSpaceSize, scratchController->getScratchSpaceSlot0Allocation()->getUnderlyingBufferSize());
        ASSERT_NE(nullptr, cmdMediaVfeState);
        auto cfeState = static_cast<CFE_STATE *>(cmdMediaVfeState);
        uint32_t bufferOffset = static_cast<uint32_t>(scratchController->slotId * scratchController->singleSurfaceStateSize * 2);
        EXPECT_EQ(bufferOffset, cfeState->getScratchSpaceBuffer());
        RENDER_SURFACE_STATE *scratchState = reinterpret_cast<RENDER_SURFACE_STATE *>(scratchController->surfaceStateHeap + bufferOffset);
        EXPECT_EQ(scratchController->scratchSlot0Allocation->getGpuAddress(), scratchState->getSurfaceBaseAddress());
        EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, scratchState->getSurfaceType());

        SurfaceStateBufferLength length = {0};
        length.length = static_cast<uint32_t>(computeUnits - 1);
        EXPECT_EQ(length.surfaceState.depth + 1u, scratchState->getDepth());
        EXPECT_EQ(length.surfaceState.width + 1u, scratchState->getWidth());
        EXPECT_EQ(length.surfaceState.height + 1u, scratchState->getHeight());

        EXPECT_EQ(perThreadScratchSize, EncodeSurfaceState<FamilyType>::getPitchForScratchInBytes(scratchState, productHelper));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenNewSshProvidedAndNoScratchAllocationExistThenNoDirtyBitSet) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    scratchController->surfaceStateHeap = reinterpret_cast<char *>(0x1000);
    scratchController->setRequiredScratchSpace(reinterpret_cast<void *>(0x2000), 0u, 0u, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_EQ(scratchController->surfaceStateHeap, reinterpret_cast<char *>(0x2000));
    EXPECT_FALSE(cfeStateDirty);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenNewSshProvidedAndScratchAllocationExistsThenSetDirtyBitCopyCurrentState) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());
    scratchController->slotId = 0;
    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;

    void *oldSurfaceHeap = alignedMalloc(0x1000, 0x1000);
    scratchController->setRequiredScratchSpace(oldSurfaceHeap, 0u, 0x1000u, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(1u, scratchController->slotId);
    EXPECT_EQ(scratchController->surfaceStateHeap, oldSurfaceHeap);
    char *surfaceStateBuf = static_cast<char *>(oldSurfaceHeap) + scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2;
    GraphicsAllocation *scratchAllocation = scratchController->scratchSlot0Allocation;
    RENDER_SURFACE_STATE *surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(scratchController->scratchSlot0Allocation->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, surfaceState->getSurfaceType());

    void *newSurfaceHeap = alignedMalloc(0x1000, 0x1000);
    scratchController->setRequiredScratchSpace(newSurfaceHeap, 0u, 0x1000u, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(1u, scratchController->slotId);
    EXPECT_EQ(scratchController->surfaceStateHeap, newSurfaceHeap);
    EXPECT_EQ(scratchAllocation, scratchController->scratchSlot0Allocation);
    surfaceStateBuf = static_cast<char *>(newSurfaceHeap) + scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2;
    surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(scratchController->scratchSlot0Allocation->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, surfaceState->getSurfaceType());

    alignedFree(oldSurfaceHeap);
    alignedFree(newSurfaceHeap);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenScratchSpaceSurfaceStateEnabledWhenBiggerScratchSpaceRequiredThenReplaceAllocation) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());
    scratchController->slotId = 6;

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);
    scratchController->setRequiredScratchSpace(surfaceHeap, 0u, 0x1000u, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(7u, scratchController->slotId);
    uint64_t offset = static_cast<uint64_t>(scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2);
    EXPECT_EQ(offset, scratchController->getScratchPatchAddress());
    EXPECT_EQ(0u, scratchController->calculateNewGSH());
    uint64_t gpuVa = scratchController->scratchSlot0Allocation->getGpuAddress();
    char *surfaceStateBuf = static_cast<char *>(scratchController->surfaceStateHeap) + offset;
    RENDER_SURFACE_STATE *surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(gpuVa, surfaceState->getSurfaceBaseAddress());

    scratchController->setRequiredScratchSpace(surfaceHeap, 0u, 0x2000u, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(8u, scratchController->slotId);
    offset = static_cast<uint64_t>(scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2);
    EXPECT_EQ(offset, scratchController->getScratchPatchAddress());
    EXPECT_NE(gpuVa, scratchController->scratchSlot0Allocation->getGpuAddress());
    gpuVa = scratchController->scratchSlot0Allocation->getGpuAddress();
    surfaceStateBuf = static_cast<char *>(scratchController->surfaceStateHeap) + offset;
    surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(gpuVa, surfaceState->getSurfaceBaseAddress());

    alignedFree(surfaceHeap);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenScratchSpaceSurfaceStateEnabledWhenScratchSlotIsNonZeroThenSlotIdIsUpdatedAndCorrectOffsetIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);
    scratchController->setRequiredScratchSpace(surfaceHeap, 1u, 0x1000u, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(1u, scratchController->slotId);
    EXPECT_TRUE(scratchController->updateSlots);
    uint64_t offset = static_cast<uint64_t>(scratchController->slotId * sizeof(RENDER_SURFACE_STATE) * 2);
    EXPECT_EQ(offset, scratchController->getScratchPatchAddress());
    EXPECT_EQ(0u, scratchController->calculateNewGSH());
    uint64_t gpuVa = scratchController->scratchSlot0Allocation->getGpuAddress();
    char *surfaceStateBuf = static_cast<char *>(scratchController->surfaceStateHeap) + offset;
    RENDER_SURFACE_STATE *surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuf);
    EXPECT_EQ(gpuVa, surfaceState->getSurfaceBaseAddress());
    alignedFree(surfaceHeap);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenScratchSpaceSurfaceStateEnabledWhenProgramHeapsThenSetReqScratchSpaceAndProgramSurfaceStateAreCalled) {
    class MockScratchSpaceControllerXeHPAndLater : public ScratchSpaceControllerXeHPAndLater {
      public:
        uint32_t requiredScratchSpaceCalledTimes = 0u;
        uint32_t programSurfaceStateCalledTimes = 0u;
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        using ScratchSpaceControllerXeHPAndLater::scratchSlot0Allocation;

        void setRequiredScratchSpace(void *sshBaseAddress,
                                     uint32_t scratchSlot,
                                     uint32_t requiredPerThreadScratchSizeSlot0,
                                     uint32_t requiredPerThreadScratchSizeSlot1,

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

    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                         *pDevice->executionEnvironment,
                                                                                                                         *commandStreamReceiver->getInternalAllocationStorage());
    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);
    NEO::GraphicsAllocation heap1(1u, 1u /*num gmms*/, NEO::AllocationType::buffer, surfaceHeap, 0u, 0u, 0u, MemoryPool::system4KBPages, 0u);
    NEO::GraphicsAllocation heap2(1u, 1u /*num gmms*/, NEO::AllocationType::buffer, surfaceHeap, 0u, 0u, 0u, MemoryPool::system4KBPages, 0u);
    NEO::GraphicsAllocation heap3(1u, 1u /*num gmms*/, NEO::AllocationType::buffer, surfaceHeap, 0u, 0u, 0u, MemoryPool::system4KBPages, 0u);
    HeapContainer container;

    container.push_back(&heap1);
    container.push_back(&heap2);
    container.push_back(&heap3);

    scratchController->programHeaps(container, 0u, 1u, 0u, commandStreamReceiver->getOsContext(), stateBaseAddressDirty, cfeStateDirty);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    EXPECT_EQ(scratch->requiredScratchSpaceCalledTimes, 1u);
    EXPECT_EQ(scratch->programSurfaceStateCalledTimes, 2u);

    alignedFree(surfaceHeap);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenScratchWhenSetNewSshPtrAndChangeIdIsFalseThenSlotIdIsNotChanged) {
    class MockScratchSpaceControllerXeHPAndLater : public ScratchSpaceControllerXeHPAndLater {
      public:
        uint32_t programSurfaceStateCalledTimes = 0u;
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        using ScratchSpaceControllerXeHPAndLater::scratchSlot0Allocation;
        using ScratchSpaceControllerXeHPAndLater::slotId;

      protected:
        void programSurfaceState() override {
            programSurfaceStateCalledTimes++;
        };
    };

    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                         *pDevice->executionEnvironment,
                                                                                                                         *commandStreamReceiver->getInternalAllocationStorage());

    NEO::GraphicsAllocation graphicsAllocation(1u, 1u /*num gmms*/, NEO::AllocationType::buffer, nullptr, 0u, 0u, 0u, MemoryPool::system4KBPages, 0u);

    bool cfeStateDirty = false;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    scratch->slotId = 10;
    scratch->scratchSlot0Allocation = &graphicsAllocation;
    scratch->setNewSshPtr(surfaceHeap, cfeStateDirty, false);
    scratch->scratchSlot0Allocation = nullptr;
    EXPECT_EQ(10u, scratch->slotId);
    EXPECT_EQ(scratch->programSurfaceStateCalledTimes, 1u);
    EXPECT_TRUE(cfeStateDirty);

    alignedFree(surfaceHeap);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenScratchWhenProgramSurfaceStateAndUpdateSlotIsFalseThenSlotIdIsNotChanged) {
    class MockScratchSpaceControllerXeHPAndLater : public ScratchSpaceControllerXeHPAndLater {
      public:
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        using ScratchSpaceControllerXeHPAndLater::programSurfaceState;
        using ScratchSpaceControllerXeHPAndLater::scratchSlot0Allocation;
        using ScratchSpaceControllerXeHPAndLater::slotId;
        using ScratchSpaceControllerXeHPAndLater::surfaceStateHeap;
        using ScratchSpaceControllerXeHPAndLater::updateSlots;
    };

    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                         *pDevice->executionEnvironment,
                                                                                                                         *commandStreamReceiver->getInternalAllocationStorage());

    NEO::GraphicsAllocation graphicsAllocation(1u, 1u /*num gmms*/, NEO::AllocationType::buffer, nullptr, 0u, 0u, 0u, MemoryPool::system4KBPages, 0u);

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    scratch->surfaceStateHeap = static_cast<char *>(surfaceHeap);
    scratch->slotId = 10;
    scratch->updateSlots = false;
    scratch->scratchSlot0Allocation = &graphicsAllocation;
    scratch->programSurfaceState();
    scratch->scratchSlot0Allocation = nullptr;
    EXPECT_EQ(10u, scratch->slotId);

    alignedFree(surfaceHeap);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenBiggerPrivateScratchSpaceRequiredThenReplaceAllocation) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[6];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    uint32_t sizeForPrivateScratch = MemoryConstants::pageSize;

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    uint64_t gpuVa = scratchController->scratchSlot1Allocation->getGpuAddress();
    EXPECT_EQ(gpuVa, surfaceState[3].getSurfaceBaseAddress());

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch * 2,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);

    EXPECT_NE(gpuVa, scratchController->scratchSlot1Allocation->getGpuAddress());
    EXPECT_EQ(scratchController->scratchSlot1Allocation->getGpuAddress(), surfaceState[5].getSurfaceBaseAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceControllerWithOnlyPrivateScratchSpaceWhenGettingPatchAddressThenGetCorrectValue) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[6];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    uint32_t sizeForPrivateScratch = MemoryConstants::pageSize;

    EXPECT_EQ(nullptr, scratchController->getScratchSpaceSlot0Allocation());
    EXPECT_EQ(nullptr, scratchController->getScratchSpaceSlot1Allocation());

    EXPECT_EQ(0u, scratchController->getScratchPatchAddress());

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    auto expectedPatchAddress = 2 * sizeof(RENDER_SURFACE_STATE);
    EXPECT_EQ(nullptr, scratchController->getScratchSpaceSlot0Allocation());
    EXPECT_NE(nullptr, scratchController->getScratchSpaceSlot1Allocation());

    EXPECT_EQ(expectedPatchAddress, scratchController->getScratchPatchAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateEnabledWhenNotBiggerPrivateScratchSpaceRequiredThenCfeStateIsNotDirty) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[4];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    uint32_t sizeForPrivateScratch = MemoryConstants::pageSize;

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    uint64_t gpuVa = scratchController->scratchSlot1Allocation->getGpuAddress();
    cfeStateDirty = false;

    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, sizeForPrivateScratch,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_FALSE(cfeStateDirty);

    EXPECT_EQ(gpuVa, scratchController->scratchSlot1Allocation->getGpuAddress());
    EXPECT_EQ(gpuVa, surfaceState[3].getSurfaceBaseAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceSurfaceStateWithoutPrivateScratchSpaceWhenDoubleAllocationsScratchSpaceIsUsedThenPrivateScratchAddressIsZero) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[4];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    uint32_t sizeForScratch = MemoryConstants::pageSize;

    scratchController->setRequiredScratchSpace(surfaceState, 0u, sizeForScratch, 0u,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    EXPECT_EQ(nullptr, scratchController->scratchSlot1Allocation);

    EXPECT_EQ(0u, surfaceState[3].getSurfaceBaseAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceControllerWhenDebugKeyForPrivateScratchIsDisabledThenThereAre16Slots) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(0);
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());
    EXPECT_EQ(16u, scratchController->stateSlotsCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenScratchSpaceControllerWhenDebugKeyForPrivateScratchIsEnabledThenThereAre32Slots) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(1);
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());
    EXPECT_EQ(32u, scratchController->stateSlotsCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenDisabledPrivateScratchSpaceWhenSizeForPrivateScratchSpaceIsProvidedThenItIsNotCreated) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(0);
    RENDER_SURFACE_STATE surfaceState[4];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;
    scratchController->setRequiredScratchSpace(surfaceState, 0u, MemoryConstants::pageSize, MemoryConstants::pageSize,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_EQ(0u, scratchController->scratchSlot1SizeInBytes);
    EXPECT_EQ(nullptr, scratchController->getScratchSpaceSlot1Allocation());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenDisabledPrivateScratchSpaceWhenGettingOffsetForSlotThenEachSlotContainsOnlyOneSurfaceState) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(0);
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());
    EXPECT_EQ(sizeof(RENDER_SURFACE_STATE), scratchController->getOffsetToSurfaceState(1u));
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw2, givenBlockedCacheFlushCmdWhenSubmittingThenDispatchBlockedCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    MockContext context(pClDevice);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->timestampPacketWriteEnabled = true;
    mockCsr->storeFlushedTaskStream = true;

    auto cmdQ0 = clUniquePtr(new MockCommandQueueHw<FamilyType>(&context, pClDevice, nullptr));

    auto &secondEngine = pDevice->getEngine(pDevice->getHardwareInfo().capabilityTable.defaultEngineType, EngineUsage::lowPriority);
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
    descriptor.memObject = buffer.get();
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
        auto expectedQueueSemaphoresCount = 2u;
        if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(pDevice->getRootDeviceEnvironment())) {
            expectedQueueSemaphoresCount += 1;
        }
        EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
        ASSERT_GT(queueSemaphores.size(), 0u);
        {
            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0]));
            EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
            EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());

            auto dataAddress = TimestampPacketHelper::getContextEndGpuAddress(*node0.getNode(0));
            EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
        }
        {
            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[1]));
            EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
            EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());

            auto dataAddress = TimestampPacketHelper::getContextEndGpuAddress(*node1.getNode(0));
            EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
        }
    }
    {
        auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
        EXPECT_EQ(0u, csrSemaphores.size());
    }

    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    if (mockCsr->isUpdateTagFromWaitEnabled()) {
        EXPECT_FALSE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    } else {
        EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    }
    EXPECT_EQ(pDevice->getPreemptionMode(), mockCsr->passedDispatchFlags.preemptionMode);

    cmdQ0->isQueueBlocked();
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, WhenOsContextSupportsMultipleDevicesThenCommandStreamReceiverIsMultiOsContextCapable) {
    uint32_t multiDeviceMask = 0b11;
    uint32_t singleDeviceMask = 0b10;
    std::unique_ptr<OsContext> multiDeviceOsContext(OsContext::create(nullptr, pDevice->getRootDeviceIndex(), 0u,
                                                                      EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}, PreemptionMode::MidThread,
                                                                                                                   multiDeviceMask)));
    std::unique_ptr<OsContext> singleDeviceOsContext(OsContext::create(nullptr, pDevice->getRootDeviceIndex(), 0u,
                                                                       EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}, PreemptionMode::MidThread,
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

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTestXeHPAndLater, givenPlatformSupportsImplicitFlushForNewResourceWhenCsrIsMultiContextThenExpectNoSupport) {
    VariableBackup<bool> defaultSettingForNewResourceBackup(&ImplicitFlushSettings<FamilyType>::getSettingForNewResource(), true);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);
    commandStreamReceiver.multiOsContextCapable = true;

    EXPECT_TRUE(ImplicitFlushSettings<FamilyType>::getSettingForNewResource());
    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsNewResourceImplicitFlush());
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
    debugManager.flags.EnableStaticPartitioning.set(1);
    debugManager.flags.EnableImplicitScaling.set(1);
    debugManager.flags.EnableLocalMemory.set(1);
    UltDeviceFactory deviceFactory{1, 2};
    MockDevice &rootDevice = *deviceFactory.rootDevices[0];
    CommandStreamReceiver &csr = rootDevice.getGpgpuCommandStreamReceiver();

    StorageInfo workPartitionAllocationStorageInfo = csr.getWorkPartitionAllocation()->storageInfo;
    EXPECT_EQ(rootDevice.getDeviceBitfield(), workPartitionAllocationStorageInfo.memoryBanks);
    EXPECT_EQ(rootDevice.getDeviceBitfield(), workPartitionAllocationStorageInfo.pageTablesVisibility);
    EXPECT_FALSE(workPartitionAllocationStorageInfo.cloningOfPageTables);
    EXPECT_TRUE(workPartitionAllocationStorageInfo.tileInstanced);
}

HWTEST2_TEMPLATED_F(CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenStaticPartitionEnabledWhenSinglePartitionUsedForPostSyncBarrierThenExpectOnlyPostSyncCommands, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    auto &commandStreamCSR = commandStreamReceiver->getCS();
    commandStreamCSR.replaceBuffer(commandStreamCSR.getCpuBase(), commandStreamCSR.getMaxAvailableSpace());

    TagNodeBase *tagNode = commandStreamReceiver->getTimestampPacketAllocator()->getTag();
    uint64_t gpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tagNode);

    TimestampPacketDependencies timestampPacketDependencies;
    timestampPacketDependencies.barrierNodes.add(tagNode);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.barrierTimestampPacketNodes = &timestampPacketDependencies.barrierNodes;

    commandStreamReceiver->staticWorkPartitioningEnabled = true;
    commandStreamReceiver->activePartitions = 1;

    size_t expectedCmdSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData);
    size_t estimatedCmdSize = commandStreamReceiver->getCmdSizeForStallingCommands(dispatchFlags);
    EXPECT_EQ(expectedCmdSize, estimatedCmdSize);

    commandStreamReceiver->programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags.barrierTimestampPacketNodes, false);
    EXPECT_EQ(estimatedCmdSize, commandStreamCSR.getUsed());

    parseCommands<FamilyType>(commandStreamCSR, 0);
    findHardwareCommands<FamilyType>();
    auto cmdItor = cmdList.begin();

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(rootDeviceEnvironment)) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
        ASSERT_NE(nullptr, pipeControl);
        cmdItor++;
        if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment) > 0) {
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

HWTEST2_TEMPLATED_F(CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenStaticPartitionDisabledWhenMultiplePartitionsUsedForPostSyncBarrierThenExpectOnlyPostSyncCommands, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    auto &commandStreamCSR = commandStreamReceiver->getCS();
    commandStreamCSR.replaceBuffer(commandStreamCSR.getCpuBase(), commandStreamCSR.getMaxAvailableSpace());

    TagNodeBase *tagNode = commandStreamReceiver->getTimestampPacketAllocator()->getTag();
    uint64_t gpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tagNode);

    TimestampPacketDependencies timestampPacketDependencies;
    timestampPacketDependencies.barrierNodes.add(tagNode);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.barrierTimestampPacketNodes = &timestampPacketDependencies.barrierNodes;

    commandStreamReceiver->staticWorkPartitioningEnabled = false;
    commandStreamReceiver->activePartitions = 2;

    size_t expectedCmdSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData);
    size_t estimatedCmdSize = commandStreamReceiver->getCmdSizeForStallingCommands(dispatchFlags);
    EXPECT_EQ(expectedCmdSize, estimatedCmdSize);

    commandStreamReceiver->programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags.barrierTimestampPacketNodes, false);
    EXPECT_EQ(estimatedCmdSize, commandStreamCSR.getUsed());

    parseCommands<FamilyType>(commandStreamCSR, 0);
    findHardwareCommands<FamilyType>();
    auto cmdItor = cmdList.begin();

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(rootDeviceEnvironment)) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
        ASSERT_NE(nullptr, pipeControl);
        cmdItor++;
        if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment) > 0) {
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

HWTEST2_TEMPLATED_F(CommandStreamReceiverHwTestXeHPAndLaterWithMockCsrHw, givenStaticPartitionEnabledWhenMultiplePartitionsUsedThenExpectImplicitScalingPostSyncBarrierWithoutSelfCleanup, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    auto &commandStreamCSR = commandStreamReceiver->getCS();
    commandStreamCSR.replaceBuffer(commandStreamCSR.getCpuBase(), commandStreamCSR.getMaxAvailableSpace());

    TagNodeBase *tagNode = commandStreamReceiver->getTimestampPacketAllocator()->getTag();
    uint64_t gpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tagNode);

    TimestampPacketDependencies timestampPacketDependencies;
    timestampPacketDependencies.barrierNodes.add(tagNode);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.barrierTimestampPacketNodes = &timestampPacketDependencies.barrierNodes;

    commandStreamReceiver->staticWorkPartitioningEnabled = true;
    commandStreamReceiver->activePartitions = 2;

    size_t expectedSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          2 * sizeof(uint32_t);
    size_t estimatedCmdSize = commandStreamReceiver->getCmdSizeForStallingCommands(dispatchFlags);
    EXPECT_EQ(expectedSize, estimatedCmdSize);

    commandStreamReceiver->programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags.barrierTimestampPacketNodes, false);
    EXPECT_EQ(estimatedCmdSize, commandStreamCSR.getUsed());
    EXPECT_EQ(2u, tagNode->getPacketsUsed());

    parseCommands<FamilyType>(commandStreamCSR, 0);
    findHardwareCommands<FamilyType>();
    auto cmdItor = cmdList.begin();

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(rootDeviceEnvironment)) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdItor);
        ASSERT_NE(nullptr, pipeControl);
        cmdItor++;
        if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment) > 0) {
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

    if (MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment) > 0) {
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
