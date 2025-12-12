/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using CommandStreamReceiverXe3pTest = ::testing::Test;

XE3P_CORETEST_F(CommandStreamReceiverXe3pTest, givenHeaplessProgramComputeModeHeaplessThenCorrectStateIsProgrammed) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    UltCommandStreamReceiver<FamilyType> csr(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr.setupContext(*device->getDefaultEngine().osContext);

    auto &commandStream = csr.getCS(sizeof(STATE_COMPUTE_MODE));

    csr.programComputeModeHeapless(*device, commandStream);

    EXPECT_EQ(false, csr.getStateComputeModeDirty());

    STATE_COMPUTE_MODE *encodedScm = reinterpret_cast<STATE_COMPUTE_MODE *>(commandStream.getCpuBase());

    EXPECT_FALSE(encodedScm->getLargeGrfMode());
    EXPECT_TRUE(encodedScm->getEnableVariableRegisterSizeAllocationVrt());
    EXPECT_FALSE(encodedScm->getOutOfBoundariesInTranslationExceptionEnable());
    EXPECT_FALSE(encodedScm->getPageFaultExceptionEnable());
    EXPECT_FALSE(encodedScm->getMemoryExceptionEnable());
    EXPECT_FALSE(encodedScm->getEnableBreakpoints());
    EXPECT_FALSE(encodedScm->getEnableForceExternalHaltAndForceException());
}

XE3P_CORETEST_F(CommandStreamReceiverXe3pTest, givenTemporaryEnablePageFaultExceptionWhenProgramComputeModeHeaplessThenCorrectStateIsProgrammed) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    DebugManagerStateRestore restorer;
    {
        debugManager.flags.TemporaryEnablePageFaultException.set(0);

        UltCommandStreamReceiver<FamilyType> csr(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
        csr.setupContext(*device->getDefaultEngine().osContext);

        auto &commandStream = csr.getCS(sizeof(STATE_COMPUTE_MODE));

        csr.programComputeModeHeapless(*device, commandStream);

        STATE_COMPUTE_MODE *encodedScm = reinterpret_cast<STATE_COMPUTE_MODE *>(commandStream.getCpuBase());
        EXPECT_FALSE(encodedScm->getPageFaultExceptionEnable());
    }
    {
        debugManager.flags.TemporaryEnablePageFaultException.set(1);
        auto &rootDeviceEnvironment = device->getRootDeviceEnvironmentRef();
        rootDeviceEnvironment.initDebuggerL0(device.get());
        UltCommandStreamReceiver<FamilyType> csr(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
        csr.setupContext(*device->getDefaultEngine().osContext);

        auto &commandStream = csr.getCS(sizeof(STATE_COMPUTE_MODE));

        csr.programComputeModeHeapless(*device, commandStream);

        STATE_COMPUTE_MODE *encodedScm = reinterpret_cast<STATE_COMPUTE_MODE *>(commandStream.getCpuBase());
        EXPECT_TRUE(encodedScm->getPageFaultExceptionEnable());
        EXPECT_TRUE(encodedScm->getMemoryExceptionEnable());
        EXPECT_TRUE(encodedScm->getEnableBreakpoints());
        EXPECT_TRUE(encodedScm->getEnableForceExternalHaltAndForceException());
    }
}

XE3P_CORETEST_F(CommandStreamReceiverXe3pTest, givenDebugEnabledWhenInitializingDeviceThenExceptionsAreProgrammedInScm) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    VariableBackup<bool> backupUseMockSip(&MockSipData::useMockSip, true);
    VariableBackup<bool> backupMockSipInit(&MockSipData::called, false);
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.Enable64BitAddressing.set(true);
    debugManager.flags.Enable64bAddressingStateInit.set(true);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;

    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(NEO::PreemptionMode::Disabled));
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0u));
    MockSipData::uninitializedSipRequested = false;

    HardwareParse hwParser;
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto itScm = find<STATE_COMPUTE_MODE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itScm);
    auto stateComputeCmd = static_cast<STATE_COMPUTE_MODE *>(*itScm);
    ASSERT_NE(nullptr, stateComputeCmd);

    EXPECT_TRUE(stateComputeCmd->getMemoryExceptionEnable());
    EXPECT_TRUE(stateComputeCmd->getEnableBreakpoints());
    EXPECT_TRUE(stateComputeCmd->getEnableForceExternalHaltAndForceException());
}

XE3P_CORETEST_F(CommandStreamReceiverXe3pTest, givenStateInitDebugFlagDisabledAnd1ContextGroupWhenPrimaryEngineCreatedThenFullStateIsNotInitialized) {
    DebugManagerStateRestore dbgRestorer;
    const uint32_t contextGroupSize = 1;

    debugManager.flags.ContextGroupSize.set(contextGroupSize);
    debugManager.flags.Enable64bAddressingStateInit.set(0);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    auto &ultCsr = deviceFactory.rootDevices[0]->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = ultCsr.commandStream;

    EXPECT_FALSE(ultCsr.osContext->isPartOfContextGroup());
    EXPECT_EQ(0u, csrStream.getUsed());
}