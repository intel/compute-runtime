/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

class ThreadGroupPreemptionTests : public DevicePreemptionTests {
  public:
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));
        preemptionMode = PreemptionMode::ThreadGroup;
        DevicePreemptionTests::SetUp();
    }
    KernelDescriptor kernelDescriptor{};
};

class MidThreadPreemptionTests : public DevicePreemptionTests {
  public:
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
        preemptionMode = PreemptionMode::MidThread;
        DevicePreemptionTests::SetUp();
    }
    KernelDescriptor kernelDescriptor{};
};

TEST_F(ThreadGroupPreemptionTests, GivenDisallowedByKmdThenThreadGroupPreemptionIsDisabled) {
    waTable->flags.waDisablePerCtxtPreemptionGranularityControl = 1;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisallowByDeviceThenThreadGroupPreemptionIsDisabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidThread, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisallowByReadWriteFencesWaThenThreadGroupPreemptionIsDisabled) {
    kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = true;
    waTable->flags.waDisableLSQCROPERFforOCL = 1;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDefaultThenThreadGroupPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDefaultModeForNonKernelRequestThenThreadGroupPreemptionIsEnabled) {
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, nullptr);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOnThenThreadGroupPreemptionIsBeingSelected) {
    kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = false;
    waTable->flags.waDisableLSQCROPERFforOCL = 1;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOffThenThreadGroupPreemptionIsBeingSelected) {
    kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = true;
    waTable->flags.waDisableLSQCROPERFforOCL = 0;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDefaultThenMidBatchPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidBatch);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, nullptr);
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisabledThenPreemptionIsDisabled) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, nullptr);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(MidThreadPreemptionTests, GivenMidThreadPreemptionThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenNullKernelThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, nullptr);
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenDisallowMidThreadPreemptionByDeviceThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(MidThreadPreemptionTests, GivenDisallowMidThreadPreemptionByKernelThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionDisallowMidThreadByDeviceThenThreadGroupPreemptionIsEnabled) {
    kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionDisallowMidThreadByKernelThenThreadGroupPreemptionIsEnabled) {
    kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenDeviceSupportsMidThreadPreemptionThenMidThreadPreemptionIsEnabled) {
    kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(ThreadGroupPreemptionTests, GivenDebugKernelPreemptionWhenDeviceSupportsThreadGroupThenExpectDebugKeyMidThreadValue) {
    debugManager.flags.ForceKernelPreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

    EXPECT_EQ(PreemptionMode::ThreadGroup, device->getPreemptionMode());

    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenDebugKernelPreemptionWhenDeviceSupportsMidThreadThenExpectDebugKeyMidBatchValue) {
    debugManager.flags.ForceKernelPreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidBatch));

    EXPECT_EQ(PreemptionMode::MidThread, device->getPreemptionMode());

    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidBatch, outMode);
}

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

        MockRootDeviceEnvironment::resetBuiltins(mockDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get(), builtIns);
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

INSTANTIATE_TEST_SUITE_P(
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

    PreemptionHelper::programCsrBaseAddress<FamilyType>(cmdStream, *mockDevice, nullptr);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

INSTANTIATE_TEST_SUITE_P(
    NonMidThread,
    PreemptionTest,
    ::testing::Values(PreemptionMode::Disabled, PreemptionMode::MidBatch, PreemptionMode::ThreadGroup));

HWTEST_F(MidThreadPreemptionTests, GivenNoWaWhenCreatingCsrSurfaceThenSurfaceIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.workaroundTable.flags.waCSRUncachable = false;

    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    ASSERT_NE(nullptr, mockDevice.get());

    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    if (mockDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread &&
        !csr.getPreemptionAllocation()) {
        ASSERT_TRUE(csr.createPreemptionAllocation());
    }
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
            auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->template getHelper<GfxCoreHelper>();

            if (++allocateGraphicsMemoryCount > gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]).size() - 1) {
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

HWTEST2_F(MidThreadPreemptionTests, GivenWaWhenCreatingCsrSurfaceThenSurfaceIsCorrect, IsGen12LP) {
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

HWCMDTEST_F(IGFX_GEN12LP_CORE, MidThreadPreemptionTests, givenDirtyCsrStateWhenStateBaseAddressIsProgrammedThenStateSipIsAdded) {
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

HWCMDTEST_F(IGFX_GEN12LP_CORE, MidThreadPreemptionTests, WhenProgrammingPreemptionThenPreemptionProgrammedAfterVfeStateInCmdBuffer) {
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

HWTEST_F(MidThreadPreemptionTests, givenKernelWithRayTracingWhenGettingPreemptionFlagsThenMidThreadPreemptionIsNotDisabled) {

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    KernelDescriptor kernelDescriptor{};

    kernelDescriptor.kernelAttributes.flags.hasRTCalls = true;

    auto flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_FALSE(flags.flags.disabledMidThreadPreemptionKernel);
}

HWTEST_F(MidThreadPreemptionTests, givenKernelWithRayTracingAndMidThreadPreemptionIsEnabledWhenGettingPreemptionFlagsThenMidThreadPreemptionIsEnabled) {

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    KernelDescriptor kernelDescriptor{};

    kernelDescriptor.kernelAttributes.flags.hasRTCalls = true;

    auto flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_FALSE(flags.flags.disabledMidThreadPreemptionKernel);
}

HWTEST_F(MidThreadPreemptionTests, givenKernelWithRayTracingWhenGettingPreemptionFlagsThenMidThreadPreemptionIsEnabledBasedOnReleaseHelperCapability) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.kernelAttributes.flags.hasRTCalls = true;
    auto flags = PreemptionHelper::createPreemptionLevelFlags(*device, &kernelDescriptor);
    EXPECT_FALSE(flags.flags.disabledMidThreadPreemptionKernel);
}
