/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/test.h"

#include "per_product_test_definitions.h"

#include <memory>

using namespace NEO;

using CriPreemptionTests = Test<DeviceFixture>;

CRITEST_F(CriPreemptionTests, givenCriProductWhenAdjustingDefaultPreemptionModeThenThreadGroupIsNotPromotedToMidThread) {
    auto capabilityTable = CRI::hwInfo.capabilityTable;
    const bool allowMidThread = true;
    const bool allowThreadGroup = true;
    const bool allowMidBatch = true;

    PreemptionHelper::adjustDefaultPreemptionMode(capabilityTable, allowMidThread, allowThreadGroup, allowMidBatch);

    EXPECT_EQ(PreemptionMode::ThreadGroup, capabilityTable.defaultPreemptionMode);
}

CRITEST_F(CriPreemptionTests, givenCriProductAndDefaultPreemptionModeWhenGettingTaskPreemptionModeThenThreadGroupIsReturned) {
    auto flags = PreemptionHelper::createPreemptionLevelFlags(*pDevice, nullptr);

    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(pDevice->getPreemptionMode(), flags));
}

CRITEST_F(CriPreemptionTests, givenCriProductAndNoDebuggerWhenCheckingStateSipRequirementThenStateSipIsNotRequired) {
    ASSERT_EQ(nullptr, pDevice->getDebugger());
    ASSERT_EQ(PreemptionMode::ThreadGroup, pDevice->getPreemptionMode());

    EXPECT_FALSE(pDevice->isStateSipRequired());
}

CRITEST_F(CriPreemptionTests, givenCriProductAndDefaultPreemptionModeWhenInitializingResourcesThenPreemptionAllocationIsNotCreated) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.resourcesInitialized = false;
    csr.setupContext(osContext);

    ASSERT_TRUE(csr.initializeResources(pDevice->getPreemptionMode()));

    EXPECT_EQ(nullptr, csr.getPreemptionAllocation());
}

CRITEST_F(CriPreemptionTests, givenCriProductAndForcedMidThreadPreemptionWhenInitializingResourcesThenPreemptionAllocationIsCreated) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.resourcesInitialized = false;
    csr.setupContext(osContext);

    ASSERT_TRUE(csr.initializeResources(PreemptionMode::MidThread));

    EXPECT_NE(nullptr, csr.getPreemptionAllocation());
}

CRITEST_F(CriPreemptionTests, givenCriProductAndDefaultPreemptionModeWhenQueryingPreambleAndStateSipSizesThenZeroIsReturned) {
    EXPECT_EQ(0u, PreemptionHelper::getRequiredPreambleSize<FamilyType>(*pDevice));
    EXPECT_EQ(0u, PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*pDevice, false));
}

CRITEST_F(CriPreemptionTests, givenCriProductAndDefaultPreemptionModeWhenProgrammingHeaplessStatePrologThenNeitherContextDataBaseAddressNorStateSipIsProgrammed) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ASSERT_EQ(PreemptionMode::ThreadGroup, pDevice->getPreemptionMode());

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    constexpr size_t bufferSize = 4096u;
    auto buffer = std::make_unique<uint8_t[]>(bufferSize);
    LinearStream commandStream(buffer.get(), bufferSize);

    csr.programHeaplessStateProlog(*pDevice, commandStream);
    ASSERT_NE(0u, commandStream.getUsed());
    EXPECT_LE(commandStream.getUsed(), csr.getCmdSizeForHeaplessPrologue(*pDevice));

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, commandStream.getCpuBase(), commandStream.getUsed());

    EXPECT_EQ(commands.end(), find<STATE_CONTEXT_DATA_BASE_ADDRESS *>(commands.begin(), commands.end()));
    EXPECT_EQ(commands.end(), find<STATE_SIP *>(commands.begin(), commands.end()));
}

CRITEST_F(CriPreemptionTests, givenCriProductAndDefaultPreemptionModeWhenProgrammingInterfaceDescriptorDataThenThreadPreemptionIsDisabled) {
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    auto flags = PreemptionHelper::createPreemptionLevelFlags(*pDevice, nullptr);
    auto taskPreemptionMode = PreemptionHelper::taskPreemptionMode(pDevice->getPreemptionMode(), flags);

    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;
    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&idd, taskPreemptionMode);

    EXPECT_FALSE(idd.getThreadPreemption());
}

CRITEST_F(CriPreemptionTests, givenCriProductWhenMidThreadPreemptionIsForcedThenDeviceModeIsRestoredToMidThread) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

    EXPECT_EQ(PreemptionMode::MidThread, PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo));
}
