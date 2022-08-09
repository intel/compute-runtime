/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

class MidThreadPreemptionTests : public DevicePreemptionTests {
  public:
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
        preemptionMode = PreemptionMode::MidThread;
        DevicePreemptionTests::SetUp();
    }
};

TEST_F(DevicePreemptionTests, GivenMidThreadPreemptionWhenSettingDefaultPreemptionThenPreemptionLevelIsSetCorrectly) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::MidThread, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, GivenThreadGroupPreemptionWhenSettingDefaultPreemptionThenPreemptionLevelIsSetCorrectly) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::ThreadGroup;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::ThreadGroup, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, GivenNoMidThreadSupportWhenSettingDefaultPreemptionThenThreadGroupPreemptionIsSet) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, false, true, true);
    EXPECT_EQ(PreemptionMode::ThreadGroup, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, GivenMidBatchPreemptionWhenSettingDefaultPreemptionThenPreemptionLevelIsSetCorrectly) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidBatch;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::MidBatch, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, GivenNoThreadGroupSupportWhenSettingDefaultPreemptionThenMidBatchPreemptionIsSet) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, false, false, true);
    EXPECT_EQ(PreemptionMode::MidBatch, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, GivenDisabledPreemptionWhenSettingDefaultPreemptionThenPreemptionLevelIsDisabled) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::Disabled;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::Disabled, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, GivenNoPreemptionSupportWhenSettingDefaultPreemptionThenDisabledIsSet) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, false, false, false);
    EXPECT_EQ(PreemptionMode::Disabled, devCapabilities.defaultPreemptionMode);
}

struct PreemptionHwTest : ::testing::Test, ::testing::WithParamInterface<PreemptionMode> {
};

HWTEST_P(PreemptionHwTest, GivenPreemptionModeIsNotChangingWhenGettingRequiredCmdStreamSizeThenZeroIsReturned) {
    PreemptionMode mode = GetParam();
    size_t requiredSize = PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(mode, mode);
    EXPECT_EQ(0U, requiredSize);

    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    {
        auto builtIns = new MockBuiltins();

        mockDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->builtins.reset(builtIns);
        PreemptionHelper::programCmdStream<FamilyType>(cmdStream, mode, mode, nullptr);
    }
    EXPECT_EQ(0U, cmdStream.getUsed());
}

HWTEST_P(PreemptionHwTest, GivenPreemptionModeIsChangingWhenGettingRequiredCmdStreamSizeThenCorrectSizeIsReturned) {
    PreemptionMode mode = GetParam();
    PreemptionMode differentPreemptionMode = static_cast<PreemptionMode>(0);

    if (false == getPreemptionTestHwDetails<FamilyType>().supportsPreemptionProgramming()) {
        EXPECT_EQ(0U, PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(mode, differentPreemptionMode));
        return;
    }

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    size_t requiredSize = PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(mode, differentPreemptionMode);
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), requiredSize);

    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    size_t minCsrSize = mockDevice->getHardwareInfo().gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);

    PreemptionHelper::programCmdStream<FamilyType>(cmdStream, mode, differentPreemptionMode, nullptr);
    EXPECT_EQ(requiredSize, cmdStream.getUsed());
}

HWTEST_P(PreemptionHwTest, WhenProgrammingCmdStreamThenProperMiLoadRegisterImmCommandIsAddedToStream) {
    PreemptionMode mode = GetParam();
    PreemptionMode differentPreemptionMode = static_cast<PreemptionMode>(0);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    if (false == getPreemptionTestHwDetails<FamilyType>().supportsPreemptionProgramming()) {
        LinearStream cmdStream(nullptr, 0U);
        PreemptionHelper::programCmdStream<FamilyType>(cmdStream, mode, differentPreemptionMode, nullptr);
        EXPECT_EQ(0U, cmdStream.getUsed());
        return;
    }

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto hwDetails = getPreemptionTestHwDetails<FamilyType>();

    uint32_t defaultRegValue = hwDetails.defaultRegValue;

    uint32_t expectedRegValue = defaultRegValue;
    if (hwDetails.modeToRegValueMap.find(mode) != hwDetails.modeToRegValueMap.end()) {
        expectedRegValue = hwDetails.modeToRegValueMap[mode];
    }

    size_t requiredSize = PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(mode, differentPreemptionMode);
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    size_t minCsrSize = mockDevice->getHardwareInfo().gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);

    PreemptionHelper::programCmdStream<FamilyType>(cmdStream, mode, differentPreemptionMode, &csrSurface);

    HardwareParse cmdParser;
    cmdParser.parseCommands<FamilyType>(cmdStream);
    const uint32_t regAddress = hwDetails.regAddress;
    MI_LOAD_REGISTER_IMM *cmd = findMmioCmd<FamilyType>(cmdParser.cmdList.begin(), cmdParser.cmdList.end(), regAddress);
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(expectedRegValue, cmd->getDataDword());
}

INSTANTIATE_TEST_CASE_P(
    CreateParametrizedPreemptionHwTest,
    PreemptionHwTest,
    ::testing::Values(PreemptionMode::Disabled, PreemptionMode::MidBatch, PreemptionMode::ThreadGroup, PreemptionMode::MidThread));

struct PreemptionTest : ::testing::Test, ::testing::WithParamInterface<PreemptionMode> {
};

HWTEST_P(PreemptionTest, whenInNonMidThreadModeThenSizeForStateSipIsZero) {
    PreemptionMode mode = GetParam();
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setPreemptionMode(mode);

    auto size = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice, false);
    EXPECT_EQ(0u, size);
}

HWTEST_P(PreemptionTest, whenInNonMidThreadModeThenStateSipIsNotProgrammed) {
    PreemptionMode mode = GetParam();
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setPreemptionMode(mode);

    auto requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice, false);
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *mockDevice, nullptr);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

HWTEST_P(PreemptionTest, whenInNonMidThreadModeThenSizeForCsrBaseAddressIsZero) {
    PreemptionMode mode = GetParam();
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setPreemptionMode(mode);

    auto size = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice);
    EXPECT_EQ(0u, size);
}

HWTEST_P(PreemptionTest, whenInNonMidThreadModeThenCsrBaseAddressIsNotProgrammed) {
    PreemptionMode mode = GetParam();
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setPreemptionMode(mode);

    auto requiredSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice);
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    PreemptionHelper::programCsrBaseAddress<FamilyType>(cmdStream, *mockDevice, nullptr, nullptr);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

INSTANTIATE_TEST_CASE_P(
    NonMidThread,
    PreemptionTest,
    ::testing::Values(PreemptionMode::Disabled, PreemptionMode::MidBatch, PreemptionMode::ThreadGroup));

HWTEST_F(MidThreadPreemptionTests, GivenNoWaWhenCreatingCsrSurfaceThenSurfaceIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.workaroundTable.flags.waCSRUncachable = false;

    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    ASSERT_NE(nullptr, mockDevice.get());

    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    MemoryAllocation *csrSurface = static_cast<MemoryAllocation *>(csr.getPreemptionAllocation());
    ASSERT_NE(nullptr, csrSurface);
    EXPECT_FALSE(csrSurface->uncacheable);

    GraphicsAllocation *devCsrSurface = csr.getPreemptionAllocation();
    EXPECT_EQ(csrSurface, devCsrSurface);
}

HWTEST_F(MidThreadPreemptionTests, givenMidThreadPreemptionWhenFailingOnCsrSurfaceAllocationThenFailToCreateDevice) {

    class FailingMemoryManager : public OsAgnosticMemoryManager {
      public:
        FailingMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment) {}

        GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override {
            auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
            if (++allocateGraphicsMemoryCount > HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo).size() - 1) {
                return nullptr;
            }
            return OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(allocationData);
        }

        uint32_t allocateGraphicsMemoryCount = 0;
    };
    ExecutionEnvironment *executionEnvironment = MockDevice::prepareExecutionEnvironment(nullptr, 0u);
    executionEnvironment->memoryManager = std::make_unique<FailingMemoryManager>(*executionEnvironment);
    if (executionEnvironment->memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }

    std::unique_ptr<MockDevice> mockDevice(MockDevice::create<MockDevice>(executionEnvironment, 0));
    EXPECT_EQ(nullptr, mockDevice.get());
}

HWTEST2_F(MidThreadPreemptionTests, GivenWaWhenCreatingCsrSurfaceThenSurfaceIsCorrect, IsAtMostGen12lp) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.workaroundTable.flags.waCSRUncachable = true;

    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    ASSERT_NE(nullptr, mockDevice.get());

    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    MemoryAllocation *csrSurface = static_cast<MemoryAllocation *>(csr.getPreemptionAllocation());
    ASSERT_NE(nullptr, csrSurface);
    EXPECT_TRUE(csrSurface->uncacheable);

    GraphicsAllocation *devCsrSurface = csr.getPreemptionAllocation();
    EXPECT_EQ(csrSurface, devCsrSurface);

    constexpr size_t expectedMask = (256 * MemoryConstants::kiloByte) - 1;

    size_t addressValue = reinterpret_cast<size_t>(devCsrSurface->getUnderlyingBuffer());
    EXPECT_EQ(0u, expectedMask & addressValue);
}

HWCMDTEST_F(IGFX_GEN8_CORE, MidThreadPreemptionTests, givenDirtyCsrStateWhenStateBaseAddressIsProgrammedThenStateSipIsAdded) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);

        auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
        csr.isPreambleSent = true;

        auto requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice, false);
        StackVec<char, 4096> buff(requiredSize);
        LinearStream commandStream(buff.begin(), buff.size());

        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

        void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

        std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
        std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

        csr.flushTask(commandStream,
                      0,
                      heap.get(),
                      heap.get(),
                      heap.get(),
                      0,
                      dispatchFlags,
                      *mockDevice);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr.getCS(0));

        auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_NE(hwParser.cmdList.end(), stateBaseAddressItor);

        auto stateSipItor = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_NE(hwParser.cmdList.end(), stateSipItor);

        auto stateSipAfterSBA = ++stateBaseAddressItor;
        while ((stateSipAfterSBA != hwParser.cmdList.end()) && (*stateSipAfterSBA != *stateSipItor)) {
            stateSipAfterSBA = ++stateBaseAddressItor;
        }
        EXPECT_EQ(*stateSipAfterSBA, *stateSipItor);

        alignedFree(buffer);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, MidThreadPreemptionTests, WhenProgrammingPreemptionThenPreemptionProgrammedAfterVfeStateInCmdBuffer) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);

        auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
        csr.isPreambleSent = true;

        auto requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice, false);
        StackVec<char, 4096> buff(requiredSize);
        LinearStream commandStream(buff.begin(), buff.size());

        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

        void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

        std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
        std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

        csr.flushTask(commandStream,
                      0,
                      heap.get(),
                      heap.get(),
                      heap.get(),
                      0,
                      dispatchFlags,
                      *mockDevice);

        auto hwDetails = getPreemptionTestHwDetails<FamilyType>();

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr.getCS(0));

        const uint32_t regAddress = hwDetails.regAddress;
        auto itorPreemptionMode = findMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), regAddress);
        auto itorMediaVFEMode = find<MEDIA_VFE_STATE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());

        itorMediaVFEMode++;
        EXPECT_TRUE(itorMediaVFEMode == itorPreemptionMode);

        alignedFree(buffer);
    }
}
