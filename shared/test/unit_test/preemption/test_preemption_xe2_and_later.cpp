/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

using namespace NEO;

struct Xe2PreemptionTests : public DevicePreemptionTests {
    void SetUp() override {
        DevicePreemptionTests::SetUp();

        commandStream.replaceBuffer(buffer, Xe2PreemptionTests::bufferSize);
    }
    void TearDown() override {
        DevicePreemptionTests::TearDown();
    }

    constexpr static size_t bufferSize = 128u;
    uint8_t buffer[bufferSize];
    LinearStream commandStream;
};

struct Xe2MidThreadPreemptionTests : public Xe2PreemptionTests {
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());

        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
        preemptionMode = PreemptionMode::MidThread;

        Xe2PreemptionTests::SetUp();

        auto &csr = device->getGpgpuCommandStreamReceiver();
        if (device->getPreemptionMode() == NEO::PreemptionMode::MidThread &&
            !csr.getPreemptionAllocation()) {
            csr.createPreemptionAllocation();
        }
    }
    void TearDown() override {
        Xe2PreemptionTests::TearDown();
    }
};

struct Xe2ThreadGroupPreemptionTests : public Xe2PreemptionTests {
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());

        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));
        preemptionMode = PreemptionMode::ThreadGroup;

        Xe2PreemptionTests::SetUp();
    }
    void TearDown() override {
        Xe2PreemptionTests::TearDown();
    }
};

HWTEST2_F(Xe2MidThreadPreemptionTests, givenMidThreadPreemptionAndNoDebugEnabledWhenQueryingRequiredPreambleSizeThenExpectCorrectSize, IsAtLeastXe2HpgCore) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;

    // Mid thread preemption is forced And debugger not enabled
    size_t cmdSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(sizeof(STATE_CONTEXT_DATA_BASE_ADDRESS), cmdSize);

    // Mid thread preemption and debugger enabled
    device->getExecutionEnvironment()->rootDeviceEnvironments[0]->initDebuggerL0(device.get());
    cmdSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(0u, cmdSize);
}

HWTEST2_F(Xe2MidThreadPreemptionTests, givenNeitherMidThreadPreemptionNOrDebugEnabledWhenQueryingRequiredPreambleSizeThenExpectZeroSize, IsAtLeastXe2HpgCore) {
    device->overridePreemptionMode(PreemptionMode::Initial);
    size_t cmdSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(0u, cmdSize);
}

HWTEST2_F(Xe2MidThreadPreemptionTests, givenMidThreadPreemptionWhenProgrammingPreemptionThenExpectZeroCommandsDispatched, IsAtLeastXe2HpgCore) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    MemoryAllocation *csrSurface = static_cast<MemoryAllocation *>(csr.getPreemptionAllocation());
    ASSERT_NE(nullptr, csrSurface);

    size_t cmdSize = PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(PreemptionMode::MidThread, PreemptionMode::Initial);
    EXPECT_EQ(0u, cmdSize);

    PreemptionHelper::programCmdStream<FamilyType>(commandStream, PreemptionMode::MidThread, PreemptionMode::Initial, csrSurface);
    EXPECT_EQ(0u, commandStream.getUsed());
}

HWTEST2_F(Xe2MidThreadPreemptionTests, givenMidThreadPreemptionWhenProgrammingPreemptionPreambleThenExpectCsrCommandDispatched, IsXe2HpgCore) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    MemoryAllocation *csrSurface = static_cast<MemoryAllocation *>(csr.getPreemptionAllocation());
    ASSERT_NE(nullptr, csrSurface);

    uint64_t gpuVa = 0x1230000;
    auto gmmHelper = device->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(gpuVa);
    csrSurface->setCpuPtrAndGpuAddress(csrSurface->getUnderlyingBuffer(), canonizedGpuAddress);

    size_t cmdSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(sizeof(STATE_CONTEXT_DATA_BASE_ADDRESS), cmdSize);

    PreemptionHelper::programCsrBaseAddress<FamilyType>(commandStream, *device, csrSurface);
    EXPECT_EQ(cmdSize, commandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(commandStream, 0u);
    hwParse.findCsrBaseAddress<FamilyType>();
    ASSERT_NE(nullptr, hwParse.cmdGpgpuCsrBaseAddress);
    auto cmd = static_cast<STATE_CONTEXT_DATA_BASE_ADDRESS *>(hwParse.cmdGpgpuCsrBaseAddress);
    EXPECT_EQ(canonizedGpuAddress, cmd->getContextDataBaseAddress());
}

HWTEST2_F(Xe2ThreadGroupPreemptionTests, givenThreadGroupPreemptionWhenProgrammingPreemptionPreambleThenExpectNoCsrCommandDispatched, IsAtLeastXe2HpgCore) {
    size_t cmdSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(0u, cmdSize);

    PreemptionHelper::programCsrBaseAddress<FamilyType>(commandStream, *device, nullptr);
    EXPECT_EQ(0u, commandStream.getUsed());
}

HWTEST2_F(Xe2MidThreadPreemptionTests, whenProgramStateSipIsCalledThenStateSipCmdIsAddedToStream, IsXe2HpgCore) {
    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    EXPECT_NE(0U, requiredSize);

    constexpr auto bufferSize = 128u;
    uint64_t buffer[bufferSize];
    LinearStream cmdStream{buffer, bufferSize * sizeof(uint64_t)};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device, nullptr);
    EXPECT_NE(0U, cmdStream.getUsed());
}

HWTEST2_F(Xe2MidThreadPreemptionTests, GivenStateSipNotRequiredWhenProgramStateSipIsCalledThenStateSipIsNotAddedAndSizeIsZero, IsAtLeastXe2HpgCore) {
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        bool isStateSipRequired() const override {
            return false;
        }
    };
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*this->device->getExecutionEnvironment()->rootDeviceEnvironments[0]);

    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    EXPECT_EQ(0U, requiredSize);

    constexpr auto bufferSize = 128u;
    uint64_t buffer[bufferSize];
    LinearStream cmdStream{buffer, bufferSize * sizeof(uint64_t)};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device, nullptr);
    EXPECT_EQ(0U, cmdStream.getUsed());
}