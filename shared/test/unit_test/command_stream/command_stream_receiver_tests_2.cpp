/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/gmm_callbacks.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/kmd_notify_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/pool_allocators.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_allocations_list.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/mock_host_function_allocator.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_scratch_space_controller_xehp_and_later.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/heapless_matchers.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/unit_test/command_stream/command_stream_receiver_tests.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "gtest/gtest.h"

#include <chrono>
#include <functional>
#include <limits>
#include <vector>

using namespace NEO;

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenFlushingMiDWThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.flushMiFlushDW(false));

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, commandStreamReceiver.flushMiFlushDW(false));
}

HWTEST_F(CommandStreamReceiverHwTest, givenCsrWhenFlushingMiFlushDwThenCommandStreamAllocationTaskCountIsUpdated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.flushMiFlushDW(false));

    auto commandStreamAllocation = commandStreamReceiver.commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, commandStreamAllocation);
    EXPECT_EQ(commandStreamReceiver.peekTaskCount(), commandStreamAllocation->getTaskCount(commandStreamReceiver.getOsContext().getContextId()));
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenFlushingPipeControlThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.flushPipeControl(false));

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, commandStreamReceiver.flushPipeControl(false));
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenFlushingTagUpdateThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.flushTagUpdate());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, commandStreamReceiver.flushTagUpdate());
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenInitializingDeviceWithFirstSubmissionThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.latestFlushedTaskCount = 0;
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));

    commandStreamReceiver.latestFlushedTaskCount = 0;
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
}

TEST(CommandStreamReceiverSimpleTest, givenNoOsContextWhenCsrIsCreatedThenPreemptionAllocationIsNotSkipped) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::make_unique<MockDriverModel>());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::move(osInterface);
    auto &mockDriverModel = *static_cast<MockDriverModel *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel());

    {
        mockDriverModel.isLatePreemptionStartSupportedResult = true;
        MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
        EXPECT_TRUE(csr.getSkipPreemptionAllocation());
    }
    {
        mockDriverModel.isLatePreemptionStartSupportedResult = false;
        MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
        EXPECT_FALSE(csr.getSkipPreemptionAllocation());
    }
    {
        executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(nullptr);
        MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
        EXPECT_FALSE(csr.getSkipPreemptionAllocation());
    }
    {
        executionEnvironment.rootDeviceEnvironments[0]->osInterface = nullptr;
        MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
        EXPECT_FALSE(csr.getSkipPreemptionAllocation());
    }
}

TEST(CommandStreamReceiverSimpleTest, givenLatePreemptionStartSupportedWhenSettingUpContextThenPreemptionAllocationIsSkippedOnlyForLatePreemptionStartTarget) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::make_unique<MockDriverModel>());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::move(osInterface);
    static_cast<MockDriverModel *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel())->isLatePreemptionStartSupportedResult = true;

    struct Case {
        aub_stream::EngineType engineType;
        EngineUsage engineUsage;
        uint32_t contextGroupCount;
        bool expectedSkip;
    };
    for (auto &testCase : {Case{aub_stream::ENGINE_CCS, EngineUsage::regular, 0u, true},
                           Case{aub_stream::ENGINE_CCS, EngineUsage::regular, 2u, false},
                           Case{aub_stream::ENGINE_CCS, EngineUsage::lowPriority, 0u, false},
                           Case{aub_stream::ENGINE_CCS, EngineUsage::internal, 0u, false},
                           Case{aub_stream::ENGINE_CCS1, EngineUsage::regular, 0u, false},
                           Case{aub_stream::ENGINE_BCS, EngineUsage::regular, 0u, false}}) {
        std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor({testCase.engineType, testCase.engineUsage})));
        osContext->setContextGroupCount(testCase.contextGroupCount);

        MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
        EXPECT_TRUE(csr.getSkipPreemptionAllocation());
        csr.setupContext(*osContext);
        EXPECT_EQ(testCase.expectedSkip, csr.getSkipPreemptionAllocation());
    }
}

struct LatePreemptionStartCsrTest : public CommandStreamReceiverHwTest {
    void SetUp() override {
        debugManager.flags.ContextGroupSize.set(0);
        CommandStreamReceiverHwTest::SetUp();
    }

    template <typename FamilyType>
    UltCommandStreamReceiver<FamilyType> *getCcsCsr(EngineUsage engineUsage) {
        if (auto *csr = getCcsCsr<FamilyType>(pDevice, engineUsage)) {
            return csr;
        }
        if (!ccsDevice) {
            HardwareInfo hwInfo = *defaultHwInfo;
            hwInfo.featureTable.flags.ftrCCSNode = true;
            hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
            hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
            hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
            ccsDevice.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
        }
        return getCcsCsr<FamilyType>(ccsDevice.get(), engineUsage);
    }

    template <typename FamilyType>
    UltCommandStreamReceiver<FamilyType> *getCcsCsr(Device *device, EngineUsage engineUsage) {
        auto *engineControl = device->tryGetEngine(aub_stream::EngineType::ENGINE_CCS, engineUsage);
        if (!engineControl || engineControl->getEngineType() != aub_stream::EngineType::ENGINE_CCS || engineControl->getEngineUsage() != engineUsage ||
            !engineControl->osContext->isLatePreemptionStartTarget()) {
            return nullptr;
        }
        ccsCsrDevice = device;
        return static_cast<UltCommandStreamReceiver<FamilyType> *>(engineControl->commandStreamReceiver);
    }

    DebugManagerStateRestore restorer;
    Device *ccsCsrDevice = nullptr;

    std::unique_ptr<MockDevice> ccsDevice;
};

HWTEST2_F(LatePreemptionStartCsrTest, givenOutOfMemoryFailureOnFlushWhenSubmittingLatePreemptionStartThenLateStartStaysPending, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    commandStreamReceiver.skipPreemptionAllocation = true;
    commandStreamReceiver.taskCount = 5u;
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_NO_THROW(commandStreamReceiver.submitLateMidThreadPreemptionStart());
    EXPECT_TRUE(commandStreamReceiver.skipPreemptionAllocation);
    EXPECT_FALSE(commandStreamReceiver.lateMidThreadPreemptionStartSubmissionPending);
    EXPECT_EQ(5u, commandStreamReceiver.peekTaskCount());

    commandStreamReceiver.flushReturnValue.reset();
    commandStreamReceiver.submitLateMidThreadPreemptionStart();
    EXPECT_FALSE(commandStreamReceiver.skipPreemptionAllocation);
    EXPECT_EQ(6u, commandStreamReceiver.peekTaskCount());
}

HWTEST2_F(LatePreemptionStartCsrTest, givenPendingTaskCountWhenSubmittingLatePreemptionStartThenLatestFlushedTaskCountIsNotUpdated, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    commandStreamReceiver.skipPreemptionAllocation = true;
    commandStreamReceiver.taskCount = 5u;
    commandStreamReceiver.latestFlushedTaskCount = 2u;

    commandStreamReceiver.submitLateMidThreadPreemptionStart();

    EXPECT_EQ(2u, commandStreamReceiver.peekLatestFlushedTaskCount());
    EXPECT_EQ(6u, commandStreamReceiver.peekTaskCount());
}

HWTEST2_F(LatePreemptionStartCsrTest, givenSkippedPreemptionAllocationWhenSubmittingLatePreemptionStartThenPreemptionAllocationIsMadeResidentInSameSubmission, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    if (!commandStreamReceiver.getPreemptionAllocation()) {
        commandStreamReceiver.createPreemptionAllocation();
    }
    auto preemptionAllocation = commandStreamReceiver.getPreemptionAllocation();
    if (!preemptionAllocation) {
        GTEST_SKIP();
    }

    commandStreamReceiver.skipPreemptionAllocation = true;
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.makeResidentAllocations.clear();

    commandStreamReceiver.submitLateMidThreadPreemptionStart();

    EXPECT_FALSE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(preemptionAllocation));
}

HWTEST2_F(LatePreemptionStartCsrTest, givenMidThreadPreemptionAlreadyEnabledWhenSubmittingLatePreemptionStartThenNothingIsSubmitted, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    commandStreamReceiver.skipPreemptionAllocation = false;
    commandStreamReceiver.taskCount = 5u;
    auto usedBefore = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.submitLateMidThreadPreemptionStart();

    EXPECT_FALSE(commandStreamReceiver.skipPreemptionAllocation);
    EXPECT_EQ(5u, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(usedBefore, commandStreamReceiver.commandStream.getUsed());
}

template <typename FamilyType>
size_t countLatePreemptionStartLris(LinearStream &commandStream, size_t startOffset = 0) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, startOffset);
    size_t count = 0;
    for (auto &it : findAll<MI_LOAD_REGISTER_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end())) {
        auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (lri->getRegisterOffset() == PreemptionConfig<FamilyType>::mmioAddress &&
            lri->getDataDword() == (PreemptionConfig<FamilyType>::mask | PreemptionConfig<FamilyType>::midThreadVal)) {
            count++;
        }
    }
    return count;
}

HWTEST2_F(LatePreemptionStartCsrTest, givenLatePreemptionStartPendingWhenFlushingTaskWithMidThreadPreemptionKernelThenLriIsProgrammedAndPreemptionAllocationIsMadeResident, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    if (!commandStreamReceiver.getPreemptionAllocation()) {
        commandStreamReceiver.createPreemptionAllocation();
    }
    auto preemptionAllocation = commandStreamReceiver.getPreemptionAllocation();
    if (!preemptionAllocation) {
        GTEST_SKIP();
    }

    commandStreamReceiver.skipPreemptionAllocation = true;
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.makeResidentAllocations.clear();

    flushTaskFlags.preemptionMode = PreemptionMode::MidThread;
    flushTaskFlags.hasMidThreadPreemptionKernel = true;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);

    EXPECT_FALSE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(preemptionAllocation));
    EXPECT_EQ(1u, countLatePreemptionStartLris<FamilyType>(commandStreamReceiver.commandStream));

    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);

    EXPECT_EQ(1u, countLatePreemptionStartLris<FamilyType>(commandStreamReceiver.commandStream));
}

HWTEST2_F(LatePreemptionStartCsrTest, givenLatePreemptionStartPendingWhenFlushingTaskWithoutMidThreadPreemptionKernelThenLriIsNotProgrammed, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.skipPreemptionAllocation = true;

    flushTaskFlags.preemptionMode = PreemptionMode::MidThread;
    flushTaskFlags.hasMidThreadPreemptionKernel = false;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);

    EXPECT_TRUE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_EQ(0u, countLatePreemptionStartLris<FamilyType>(commandStreamReceiver.commandStream));
}

HWTEST2_F(LatePreemptionStartCsrTest, givenLatePreemptionStartPendingWhenGettingCmdSizeForLateStartThenLriSizeIsReturnedOnlyForMidThreadPreemptionKernel, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    commandStreamReceiver.skipPreemptionAllocation = true;
    EXPECT_EQ(PreemptionHelper::getRequiredCmdStreamSizeForLateStart<FamilyType>(), commandStreamReceiver.getCmdSizeForLateMidThreadPreemptionStart(true));
    EXPECT_EQ(0u, commandStreamReceiver.getCmdSizeForLateMidThreadPreemptionStart(false));

    commandStreamReceiver.skipPreemptionAllocation = false;
    EXPECT_EQ(0u, commandStreamReceiver.getCmdSizeForLateMidThreadPreemptionStart(true));
}

HWTEST2_F(LatePreemptionStartCsrTest, givenLatePreemptionStartPendingWhenFlushingImmediateTaskWithMidThreadPreemptionKernelThenLriIsProgrammedAndPreemptionAllocationIsMadeResident, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    if (!commandStreamReceiver.getPreemptionAllocation()) {
        commandStreamReceiver.createPreemptionAllocation();
    }
    auto preemptionAllocation = commandStreamReceiver.getPreemptionAllocation();
    if (!preemptionAllocation) {
        GTEST_SKIP();
    }

    commandStreamReceiver.storeMakeResidentAllocations = true;

    for (auto dispatchOperation : {NEO::AppendOperations::kernel, NEO::AppendOperations::cmdList}) {
        commandStreamReceiver.skipPreemptionAllocation = true;
        commandStreamReceiver.makeResidentAllocations.clear();
        auto csrUsedBefore = commandStreamReceiver.commandStream.getUsed();

        immediateFlushTaskFlags.dispatchOperation = dispatchOperation;
        immediateFlushTaskFlags.hasMidThreadPreemptionKernel = true;
        commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *ccsCsrDevice);

        EXPECT_FALSE(commandStreamReceiver.getSkipPreemptionAllocation());
        EXPECT_TRUE(commandStreamReceiver.isMadeResident(preemptionAllocation));
        EXPECT_EQ(1u, countLatePreemptionStartLris<FamilyType>(commandStreamReceiver.commandStream, csrUsedBefore));
    }
}

HWTEST2_F(LatePreemptionStartCsrTest, givenLatePreemptionStartPendingWhenFlushingImmediateTaskWithoutMidThreadPreemptionKernelThenLriIsNotProgrammed, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.skipPreemptionAllocation = true;

    immediateFlushTaskFlags.hasMidThreadPreemptionKernel = false;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *ccsCsrDevice);

    EXPECT_TRUE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_EQ(0u, countLatePreemptionStartLris<FamilyType>(commandStreamReceiver.commandStream));
}

HWTEST2_F(LatePreemptionStartCsrTest, givenFailedSubmissionWhenFlushingTaskWithMidThreadPreemptionKernelThenLateStartIsProgrammedAgainOnNextFlush, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.overrideDispatchPolicy(DispatchMode::immediateDispatch);
    commandStreamReceiver.skipPreemptionAllocation = true;
    flushTaskFlags.preemptionMode = PreemptionMode::MidThread;
    flushTaskFlags.hasMidThreadPreemptionKernel = true;

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);
    EXPECT_TRUE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_FALSE(commandStreamReceiver.lateMidThreadPreemptionStartSubmissionPending);

    commandStreamReceiver.flushReturnValue.reset();
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);
    EXPECT_FALSE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_FALSE(commandStreamReceiver.lateMidThreadPreemptionStartSubmissionPending);
    EXPECT_EQ(2u, countLatePreemptionStartLris<FamilyType>(commandStreamReceiver.commandStream));
}

HWTEST2_F(LatePreemptionStartCsrTest, givenBatchedDispatchWhenFlushingTaskWithLateStartThenQueuedCommandBuffersAreSubmittedRightAway, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    commandStreamReceiver.skipPreemptionAllocation = true;
    flushTaskFlags.preemptionMode = PreemptionMode::MidThread;
    auto &commandBufferList = commandStreamReceiver.submissionAggregator->peekCmdBufferList();

    flushTaskFlags.hasMidThreadPreemptionKernel = false;
    commandStreamReceiver.newResources = false;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);
    EXPECT_FALSE(commandBufferList.empty());
    EXPECT_TRUE(commandStreamReceiver.getSkipPreemptionAllocation());

    flushTaskFlags.hasMidThreadPreemptionKernel = true;
    commandStreamReceiver.newResources = false;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);
    EXPECT_TRUE(commandBufferList.empty());
    EXPECT_FALSE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_FALSE(commandStreamReceiver.lateMidThreadPreemptionStartSubmissionPending);
    EXPECT_EQ(1u, countLatePreemptionStartLris<FamilyType>(commandStreamReceiver.commandStream));

    commandStream.getSpace(4);
    commandStreamReceiver.newResources = false;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);
    EXPECT_FALSE(commandBufferList.empty());
    EXPECT_EQ(1u, countLatePreemptionStartLris<FamilyType>(commandStreamReceiver.commandStream));

    EXPECT_TRUE(commandStreamReceiver.flushBatchedSubmissions());
}

HWTEST2_F(LatePreemptionStartCsrTest, givenBatchedDispatchAndFailedLateStartSubmissionWhenFlushingNextTaskWithMidThreadPreemptionKernelThenLateStartIsProgrammedAgain, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    commandStreamReceiver.skipPreemptionAllocation = true;
    flushTaskFlags.preemptionMode = PreemptionMode::MidThread;
    flushTaskFlags.hasMidThreadPreemptionKernel = true;
    auto &commandBufferList = commandStreamReceiver.submissionAggregator->peekCmdBufferList();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    commandStreamReceiver.newResources = false;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);
    EXPECT_TRUE(commandBufferList.empty());
    EXPECT_TRUE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_FALSE(commandStreamReceiver.lateMidThreadPreemptionStartSubmissionPending);

    commandStreamReceiver.flushReturnValue.reset();
    commandStreamReceiver.newResources = false;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);
    EXPECT_TRUE(commandBufferList.empty());
    EXPECT_FALSE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_EQ(2u, countLatePreemptionStartLris<FamilyType>(commandStreamReceiver.commandStream));
}

HWTEST2_F(LatePreemptionStartCsrTest, givenBatchedDispatchAndFailedEarlierBatchWhenFlushingTaskWithLateStartThenNoErrorIsReturnedAndTaskStaysQueued, IsAtLeastXe2HpgCore) {
    auto *ccsCsr = getCcsCsr<FamilyType>(EngineUsage::regular);
    if (!ccsCsr) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = *ccsCsr;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    commandStreamReceiver.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    commandStreamReceiver.skipPreemptionAllocation = true;
    flushTaskFlags.preemptionMode = PreemptionMode::MidThread;
    auto &commandBufferList = commandStreamReceiver.submissionAggregator->peekCmdBufferList();

    flushTaskFlags.hasMidThreadPreemptionKernel = false;
    flushTaskFlags.throttle = QueueThrottle::LOW;
    commandStreamReceiver.newResources = false;
    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);

    flushTaskFlags.hasMidThreadPreemptionKernel = true;
    flushTaskFlags.throttle = QueueThrottle::MEDIUM;
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    commandStreamReceiver.newResources = false;
    auto completionStamp = commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevel, flushTaskFlags, *ccsCsrDevice);
    EXPECT_EQ(commandStreamReceiver.peekTaskCount(), completionStamp.taskCount);
    EXPECT_FALSE(commandBufferList.empty());
    EXPECT_TRUE(commandStreamReceiver.getSkipPreemptionAllocation());
    EXPECT_FALSE(commandStreamReceiver.lateMidThreadPreemptionStartSubmissionPending);

    commandStreamReceiver.flushReturnValue.reset();
    EXPECT_TRUE(commandStreamReceiver.flushBatchedSubmissions());
    EXPECT_TRUE(commandBufferList.empty());
}

HWTEST_F(CommandStreamReceiverHwTest, givenPendingLateMidThreadPreemptionStartWhenSubmittingBatchBufferThenPreemptionAllocationIsSkippedAgainOnlyOnFailure) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = commandStreamReceiver.getCS();

    for (auto submissionStatus : {SubmissionStatus::outOfMemory, SubmissionStatus::success}) {
        commandStreamReceiver.skipPreemptionAllocation = false;
        commandStreamReceiver.lateMidThreadPreemptionStartSubmissionPending = true;
        commandStreamReceiver.flushReturnValue = submissionStatus;

        BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(csrStream.getGraphicsAllocation(), &csrStream, csrStream.getUsed());
        EXPECT_EQ(submissionStatus, commandStreamReceiver.submitBatchBuffer(batchBuffer, commandStreamReceiver.getResidencyAllocations()));

        EXPECT_EQ(submissionStatus != SubmissionStatus::success, commandStreamReceiver.skipPreemptionAllocation);
        EXPECT_FALSE(commandStreamReceiver.lateMidThreadPreemptionStartSubmissionPending);
    }
}

HWTEST_F(CommandStreamReceiverHwTest, whenFlushTagUpdateThenSetStallingCmdsFlag) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.recordFlushedBatchBuffer = true;

    EXPECT_EQ(SubmissionStatus::success, ultCsr.flushTagUpdate());

    EXPECT_TRUE(ultCsr.latestFlushedBatchBuffer.hasStallingCmds);
}

HWTEST_F(CommandStreamReceiverHwTest, givenTaskCountUpdateOnlyFlushWhenItFailsOrSucceedsThenItsTaskCountIsRecordedOnlyOnSuccess) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.taskCount = 5u;
    const TaskCountType initialTaskCountUpdateOnlyFlushTaskCount = ultCsr.latestTaskCountUpdateOnlyFlushTaskCount;

    ultCsr.flushReturnValue = SubmissionStatus::failed;
    EXPECT_EQ(SubmissionStatus::failed, ultCsr.flushTagUpdate());
    EXPECT_EQ(initialTaskCountUpdateOnlyFlushTaskCount, ultCsr.latestTaskCountUpdateOnlyFlushTaskCount);

    ultCsr.flushReturnValue = SubmissionStatus::success;
    EXPECT_EQ(SubmissionStatus::success, ultCsr.flushTagUpdate());
    EXPECT_EQ(6u, ultCsr.peekTaskCount());
    EXPECT_EQ(6u, ultCsr.latestTaskCountUpdateOnlyFlushTaskCount);
}

HWTEST_F(CommandStreamReceiverHwTest, whenFlushTagUpdateThenSetPassNumClients) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.recordFlushedBatchBuffer = true;

    int client1, client2;
    ultCsr.registerClient(&client1);
    ultCsr.registerClient(&client2);

    EXPECT_EQ(SubmissionStatus::success, ultCsr.flushTagUpdate());

    EXPECT_EQ(ultCsr.getNumClients(), ultCsr.latestFlushedBatchBuffer.numCsrClients);
}

HWTEST_F(CommandStreamReceiverHwTest, whenFlushTagUpdateWhenRequiredCalledThenFlushOnlyIfCounterIncreased) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.taskCount++;
    auto flushedTaskCount = ultCsr.flushTagUpdateIfRequired();
    EXPECT_EQ(ultCsr.peekLatestFlushedTaskCount(), flushedTaskCount);
    EXPECT_TRUE(ultCsr.flushTagUpdateCalled);

    ultCsr.flushTagUpdateCalled = false;
    EXPECT_EQ(flushedTaskCount, ultCsr.flushTagUpdateIfRequired());
    EXPECT_FALSE(ultCsr.flushTagUpdateCalled);
}

HWTEST_F(CommandStreamReceiverHwTest, whenFlushTaskCalledThenSetPassNumClients) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    int client1, client2;
    commandStreamReceiver.registerClient(&client1);
    commandStreamReceiver.registerClient(&client2);

    commandStreamReceiver.flushTask(commandStream,
                                    64,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_EQ(commandStreamReceiver.getNumClients(), commandStreamReceiver.latestFlushedBatchBuffer.numCsrClients);
}

HWTEST_F(CommandStreamReceiverHwTest, givenVariousCsrModeWhenGettingTbxModeThenExpectOnlyWhenModeIsTbxOrTbxWithAub) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::hardware;
    EXPECT_FALSE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::hardwareWithAub;
    EXPECT_FALSE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    EXPECT_FALSE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    EXPECT_TRUE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbxWithAub;
    EXPECT_TRUE(ultCsr.isTbxMode());
}

HWTEST_F(CommandStreamReceiverHwTest, givenVariousCsrModeWhenGettingAubModeThenReturnedValueIsCorrect) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::hardware;
    EXPECT_FALSE(ultCsr.isAubMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::hardwareWithAub;
    EXPECT_TRUE(ultCsr.isAubMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    EXPECT_TRUE(ultCsr.isAubMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    EXPECT_FALSE(ultCsr.isAubMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbxWithAub;
    EXPECT_TRUE(ultCsr.isAubMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::nullAub;
    EXPECT_TRUE(ultCsr.isAubMode());
}

HWTEST_F(CommandStreamReceiverHwTest, GivenTwoRootDevicesWhengetMultiRootDeviceTimestampPacketAllocatorCalledThenAllocatorForTwoDevicesCreated) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), true, 2u);
    auto devices = DeviceFactory::createDevices(*executionEnvironment.release());
    const RootDeviceIndicesContainer indices = {0u, 1u};
    auto csr = devices[0]->getDefaultEngine().commandStreamReceiver;
    auto allocator = csr->createMultiRootDeviceTimestampPacketAllocator(indices);
    class MockTagAllocatorBase : public TagAllocatorBase {
      public:
        using TagAllocatorBase::maxRootDeviceIndex;
    };
    EXPECT_EQ(reinterpret_cast<MockTagAllocatorBase *>(allocator.get())->maxRootDeviceIndex, 1u);
}
HWTEST_F(CommandStreamReceiverHwTest, GivenFiveRootDevicesWhengetMultiRootDeviceTimestampPacketAllocatorCalledThenAllocatorForFiveDevicesCreated) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), true, 4u);
    auto devices = DeviceFactory::createDevices(*executionEnvironment.release());
    const RootDeviceIndicesContainer indices = {0u, 1u, 2u, 3u};
    auto csr = devices[0]->getDefaultEngine().commandStreamReceiver;
    auto allocator = csr->createMultiRootDeviceTimestampPacketAllocator(indices);
    class MockTagAllocatorBase : public TagAllocatorBase {
      public:
        using TagAllocatorBase::maxRootDeviceIndex;
    };
    EXPECT_EQ(reinterpret_cast<MockTagAllocatorBase *>(allocator.get())->maxRootDeviceIndex, 3u);
}
HWTEST_F(CommandStreamReceiverHwTest, givenMultiRootDeviceSyncNodeWhenFlushBcsTaskThenMiFlushAdded) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(pDevice->getRootDeviceIndex(), pDevice->getExecutionEnvironment()->memoryManager.get(), 10u);

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          commandStreamReceiver, commandStreamReceiver.getTagAllocation(), nullptr,
                                                                          commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(),
                                                                          commandStreamReceiver.getTagAllocation()->getGpuAddress(), 0,
                                                                          0, 0, 0, 0, 0, 0, 0);
    auto tag = mockTagAllocator->getTag();
    blitProperties.multiRootDeviceEventSync = tag;

    BlitPropertiesContainer container;
    container.push_back(blitProperties);
    commandStreamReceiver.flushBcsTask(container, true, *pDevice);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    bool nodeAddressFound = false;
    while (cmdIterator != hwParser.cmdList.end()) {
        auto flush = genCmdCast<MI_FLUSH_DW *>(*cmdIterator);
        if (flush->getDestinationAddress() == tag->getGpuAddress() + tag->getContextEndOffset()) {
            nodeAddressFound = true;
            break;
        }
        cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(++cmdIterator, hwParser.cmdList.end());
    }
    EXPECT_TRUE(nodeAddressFound);
}
HWTEST_F(CommandStreamReceiverHwTest, givenNullPtrAsMultiRootDeviceSyncNodeWhenFlushBcsTAskThenMiFlushNotAdded) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(pDevice->getRootDeviceIndex(), pDevice->getExecutionEnvironment()->memoryManager.get(), 10u);

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          commandStreamReceiver, commandStreamReceiver.getTagAllocation(), nullptr,
                                                                          commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(),
                                                                          commandStreamReceiver.getTagAllocation()->getGpuAddress(), 0,
                                                                          0, 0, 0, 0, 0, 0, 0);
    auto tag = mockTagAllocator->getTag();

    BlitPropertiesContainer container;
    container.push_back(blitProperties);
    commandStreamReceiver.flushBcsTask(container, true, *pDevice);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    bool nodeAddressFound = false;
    while (cmdIterator != hwParser.cmdList.end()) {
        auto flush = genCmdCast<MI_FLUSH_DW *>(*cmdIterator);
        if (flush->getDestinationAddress() == tag->getGpuAddress() + tag->getContextEndOffset()) {
            nodeAddressFound = true;
            break;
        }
        cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(++cmdIterator, hwParser.cmdList.end());
    }
    EXPECT_FALSE(nodeAddressFound);
}

HWTEST_F(CommandStreamReceiverTest, givenL1CachePolicyInitializedInCsrWhenGettingPolicySettingsThenExpectValueMatchProductHelper) {
    auto &productHelper = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    auto l1CachePolicy = commandStreamReceiver->getStoredL1CachePolicy();
    EXPECT_EQ(productHelper.getL1CachePolicy(true), l1CachePolicy->getL1CacheValue(true));
    EXPECT_EQ(productHelper.getL1CachePolicy(false), l1CachePolicy->getL1CacheValue(false));
}

HWTEST_F(CommandStreamReceiverHwTest, givenCreateGlobalStatelessHeapAllocationWhenGettingIndirectHeapObjectThenHeapAndAllocationAreInitialized) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(nullptr, commandStreamReceiver.getGlobalStatelessHeap());

    commandStreamReceiver.createGlobalStatelessHeap();
    auto heap = commandStreamReceiver.getGlobalStatelessHeap();
    ASSERT_NE(nullptr, heap);
    EXPECT_EQ(commandStreamReceiver.getGlobalStatelessHeapAllocation(), heap->getGraphicsAllocation());

    auto heapAllocation = commandStreamReceiver.getGlobalStatelessHeapAllocation();
    commandStreamReceiver.createGlobalStatelessHeap();
    EXPECT_EQ(commandStreamReceiver.getGlobalStatelessHeap(), heap);
    EXPECT_EQ(commandStreamReceiver.getGlobalStatelessHeapAllocation(), heap->getGraphicsAllocation());
    EXPECT_EQ(commandStreamReceiver.getGlobalStatelessHeapAllocation(), heapAllocation);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenCreateGlobalStatelessHeapAllocationWhenFlushingTaskThenGlobalStatelessHeapAllocationIsResidentAndNoBindingTableCommandDispatched,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    commandStreamReceiver.storeMakeResidentAllocations = true;
    EXPECT_EQ(nullptr, commandStreamReceiver.getGlobalStatelessHeap());

    commandStreamReceiver.createGlobalStatelessHeap();
    auto statelessHeap = commandStreamReceiver.getGlobalStatelessHeap();
    ASSERT_NE(nullptr, statelessHeap);

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    statelessHeap,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getGlobalStatelessHeapAllocation()));

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    EXPECT_EQ(nullptr, hwParserCsr.cmdBindingTableBaseAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenStateComputeModeDirtyWhenFlushingFirstTimeThenCleanDirtyFlagToDispatchStateComputeMode,
          IsAtLeastXeCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);
    EXPECT_FALSE(commandStreamReceiver.getStateComputeModeDirty());

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    auto scmCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();
    EXPECT_NE(nullptr, scmCmd);
}

HWTEST_F(CommandStreamReceiverHwTest, givenFlushPipeControlWhenFlushWithoutStateCacheFlushThenExpectNoStateCacheFlushFlagsSet) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushPipeControl(false);

    EXPECT_FALSE(UnitTestHelper<FamilyType>::findStateCacheFlushPipeControl(commandStreamReceiver, commandStreamReceiver.commandStream));
}

HWTEST_F(CommandStreamReceiverHwTest, givenFCommandStreamWhenSubmitingDependencyUpdateThenPCWithTagAddresIsDispatched) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(pDevice->getRootDeviceIndex(), pDevice->getExecutionEnvironment()->memoryManager.get(), 10u);
    auto tag = mockTagAllocator->getTag();
    auto usedSizeBeforeSubmit = commandStreamReceiver.commandStream.getUsed();
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(commandStreamReceiver.peekRootDeviceEnvironment())) {
        usedSizeBeforeSubmit += sizeof(PIPE_CONTROL);
    }
    commandStreamReceiver.submitDependencyUpdate(tag);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSizeBeforeSubmit);
    const auto pipeControlItor = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    const auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_NE(nullptr, pipeControl);
    auto cacheFlushTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tag);
    EXPECT_EQ(UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl), cacheFlushTimestampPacketGpuAddress);
    EXPECT_EQ(pipeControl->getDcFlushEnable(), MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, commandStreamReceiver.peekRootDeviceEnvironment()));
}

HWTEST_F(CommandStreamReceiverHwTest, givenCsrWhenSubmittingDependencyUpdateThenCommandStreamAllocationTaskCountIsUpdated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(pDevice->getRootDeviceIndex(), pDevice->getExecutionEnvironment()->memoryManager.get(), 10u);
    auto tag = mockTagAllocator->getTag();

    EXPECT_TRUE(commandStreamReceiver.submitDependencyUpdate(tag));

    auto commandStreamAllocation = commandStreamReceiver.commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, commandStreamAllocation);
    EXPECT_EQ(commandStreamReceiver.peekTaskCount(), commandStreamAllocation->getTaskCount(commandStreamReceiver.getOsContext().getContextId()));
}

HWTEST_F(CommandStreamReceiverHwTest, givenFlushPipeControlWhenFlushWithStateCacheFlushThenExpectStateCacheFlushFlagsSet) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.sendRenderStateCacheFlush();

    EXPECT_TRUE(UnitTestHelper<FamilyType>::findStateCacheFlushPipeControl(commandStreamReceiver, commandStreamReceiver.commandStream));
}

HWTEST_F(CommandStreamReceiverHwTest, givenBcsWhenSubmitDependencyUpdateThenMiFlushDwWithTagAddressIsDispatched) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}));
    commandStreamReceiver.setupContext(mockOsContext);

    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(pDevice->getRootDeviceIndex(), pDevice->getExecutionEnvironment()->memoryManager.get(), 10u);
    auto tag = mockTagAllocator->getTag();

    auto usedSizeBeforeSubmit = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.submitDependencyUpdate(tag);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSizeBeforeSubmit);

    auto cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    bool nodeAddressFound = false;
    while (cmdIterator != hwParser.cmdList.end()) {
        auto flush = genCmdCast<MI_FLUSH_DW *>(*cmdIterator);
        if (flush->getDestinationAddress() == TimestampPacketHelper::getContextEndGpuAddress(*tag)) {
            nodeAddressFound = true;
            break;
        }
        cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(++cmdIterator, hwParser.cmdList.end());
    }
    EXPECT_TRUE(nodeAddressFound);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenRayTracingAllocationPresentWhenFlushingTaskThenDispatchBtdStateCommandOnceAndResidentAlways,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(commandStreamReceiver.isRayTracingStateProgramingNeeded(*pDevice));

    pDevice->initializeRayTracing(8);

    auto rtAllocation = pDevice->getRTMemoryBackedBuffer();
    auto rtAllocationAddress = rtAllocation->getGpuAddress();

    EXPECT_TRUE(commandStreamReceiver.isRayTracingStateProgramingNeeded(*pDevice));

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    ASSERT_NE(nullptr, btdStateCmd);

    EXPECT_EQ(rtAllocationAddress, btdStateCmd->getMemoryBackedBufferBasePointer());

    uint32_t residentCount = 1;
    commandStreamReceiver.isMadeResident(rtAllocation, residentCount);

    EXPECT_FALSE(commandStreamReceiver.isRayTracingStateProgramingNeeded(*pDevice));

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    EXPECT_EQ(nullptr, btdStateCmd);

    residentCount++;
    commandStreamReceiver.isMadeResident(rtAllocation, residentCount);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenPlatformNotSupportingRayTracingWhenDispatchingCommandThenNothingDispatched,
          IsGen12LP) {
    pDevice->initializeRayTracing(8);

    constexpr size_t size = 64;
    uint8_t buffer[size];
    LinearStream cmdStream(buffer, size);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(0u, commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo()));

    commandStreamReceiver.dispatchRayTracingStateCommand(cmdStream, *pDevice);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

using RayTracingForcedWaPlatform = IsAnyProducts<IGFX_DG2, IGFX_PVC>;

HWTEST2_F(CommandStreamReceiverHwTest,
          givenPlatformSupportingRayTracingWhenForcedPipeControlPriorBtdStateCommandThenPipeControlDispatched,
          RayTracingForcedWaPlatform) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore stateRestore;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(1);

    pDevice->initializeRayTracing(8);

    constexpr size_t size = 256;
    uint8_t buffer[size];
    LinearStream cmdStream(buffer, size);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.dispatchRayTracingStateCommand(cmdStream, *pDevice);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream.getCpuBase(),
        cmdStream.getUsed()));

    ASSERT_EQ(2u, cmdList.size());

    auto itCmd = cmdList.begin();

    auto cmdPipeControl = genCmdCast<PIPE_CONTROL *>(*itCmd);
    EXPECT_NE(nullptr, cmdPipeControl);

    itCmd++;
    auto cmdBtdState = genCmdCast<_3DSTATE_BTD *>(*itCmd);
    EXPECT_NE(nullptr, cmdBtdState);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenPipelineSelectNotInitializedThenDispatchPipelineSelectCommand,
          IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();
    ASSERT_NE(nullptr, pipelineSelectCmd);
    EXPECT_TRUE(commandStreamReceiver.getPreambleSetFlag());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();

    EXPECT_EQ(nullptr, pipelineSelectCmd);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskCmdListDispatchWhenPipelineSelectNotInitializedThenDoNotDispatchPipelineSelectCommand,
          IsXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskOnSystolicPlatformWhenPipelineSelectAlreadyInitializedAndSystolicRequiredThenDispatchPipelineSelectCommandForSystolic,
          IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();
    ASSERT_NE(nullptr, pipelineSelectCmd);
    EXPECT_TRUE(commandStreamReceiver.getPreambleSetFlag());

    this->requiredStreamProperties.pipelineSelect.setPropertySystolicMode(true);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();

    if (commandStreamReceiver.pipelineSupportFlags.systolicMode) {
        ASSERT_NE(nullptr, pipelineSelectCmd);
        EXPECT_TRUE(UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));
    } else {
        EXPECT_EQ(nullptr, pipelineSelectCmd);
    }
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskNonKernelDispatchWhenPipelineSelectAlreadyInitializedAndSystolicRequiredThenIgnoreRequiredStreamProperties,
          IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::nonKernel;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();
    ASSERT_NE(nullptr, pipelineSelectCmd);
    EXPECT_TRUE(commandStreamReceiver.getPreambleSetFlag());

    this->requiredStreamProperties.pipelineSelect.setPropertySystolicMode(true);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenFrontEndNotInitializedThenDispatchFrontEndCommand,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();

    EXPECT_EQ(nullptr, frontEndCmd);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskCmdListDispatchWhenFrontEndNotInitializedThenDoNotDispatchFrontEndCommand,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskOnChangingFrontEndPropertiesPlatformWhenFrontEndAlreadyInitializedAndFrontEndPropertyChangeRequiredThenDispatchFrontEndCommand,
          IsXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    this->requiredStreamProperties.frontEndState.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(true, true);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();

    if (commandStreamReceiver.feSupportFlags.computeDispatchAllWalker || commandStreamReceiver.feSupportFlags.disableEuFusion) {
        ASSERT_NE(nullptr, frontEndCmd);
        if (commandStreamReceiver.feSupportFlags.computeDispatchAllWalker) {
            EXPECT_TRUE(UnitTestHelperWithHeap<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(*frontEndCmd));
        } else {
            EXPECT_FALSE(UnitTestHelperWithHeap<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(*frontEndCmd));
        }
        if (commandStreamReceiver.feSupportFlags.disableEuFusion) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(*frontEndCmd));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(*frontEndCmd));
        }
    } else {
        EXPECT_EQ(nullptr, frontEndCmd);
    }
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskNonKernelDispatchWhenFrontEndAlreadyInitializedAndFrontEndPropertyChangeRequiredThenIgnoreRequiredStreamProperties,
          IsXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::nonKernel;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    this->requiredStreamProperties.frontEndState.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(true, true);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenStateComputeModeNotInitializedThenDispatchStateComputeModeCommand,
          IsAtLeastXeCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();
    ASSERT_NE(nullptr, stateComputeModeCmd);
    EXPECT_FALSE(commandStreamReceiver.getStateComputeModeDirty());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();

    EXPECT_EQ(nullptr, stateComputeModeCmd);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskCmdListDispatchWhenStateComputeModeNotInitializedThenDoNotDispatchStateComputeModeCommand,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskOnChangingStateComputeModePropertiesPlatformWhenStateComputeModeAlreadyInitializedAndStateComputeModePropertyChangeRequiredThenDispatchStateComputeModeCommand,
          IsAtLeastXeCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    this->requiredStreamProperties.stateComputeMode.setPropertiesAll(GrfConfig::defaultGrfNumber, ThreadArbitrationPolicy::AgeBased, false);

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();
    ASSERT_NE(nullptr, stateComputeModeCmd);
    EXPECT_FALSE(commandStreamReceiver.getStateComputeModeDirty());

    this->requiredStreamProperties.stateComputeMode.setPropertiesGrfNumberThreadArbitration(GrfConfig::largeGrfNumber, ThreadArbitrationPolicy::RoundRobin);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();

    StateComputeModePropertiesSupport scmPropertiesSupport;
    auto &productHelper = commandStreamReceiver.getProductHelper();
    productHelper.fillScmPropertiesSupportStructure(scmPropertiesSupport);

    if (scmPropertiesSupport.largeGrfMode || scmPropertiesSupport.threadArbitrationPolicy) {
        ASSERT_NE(nullptr, stateComputeModeCmd);
    } else {
        EXPECT_EQ(nullptr, stateComputeModeCmd);
    }
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskNonKernelDispatchWhenStateComputeModeAlreadyInitializedAndStateComputeModePropertyChangeRequiredThenIgnoreRequiredStreamProperties,
          IsAtLeastXeCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    this->requiredStreamProperties.stateComputeMode.setPropertiesAll(GrfConfig::defaultGrfNumber, ThreadArbitrationPolicy::AgeBased, false);

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::nonKernel;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();
    ASSERT_NE(nullptr, stateComputeModeCmd);
    EXPECT_FALSE(commandStreamReceiver.getStateComputeModeDirty());

    this->requiredStreamProperties.stateComputeMode.setPropertiesGrfNumberThreadArbitration(GrfConfig::largeGrfNumber, ThreadArbitrationPolicy::RoundRobin);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndBindingPoolBaseAddressNeededWhenStateBaseAddressNotInitializedThenDispatchStateBaseAddressAndBindingPoolBaseAddressCommand,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    EXPECT_EQ(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskCmdListDispatchWhenStateBaseAddressNotInitializedThenDoNotDispatchStateBaseAddressAndBindingPoolBaseAddressCommand,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndBindingPoolBaseAddressNeededWhenStateBaseAddressInitializedAndHeapsChangedThenDispatchStateBaseAddressAndBindingPoolBaseAddressCommandTwice,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesBindingTableSurfaceState(0x5000, 0x100, 0x6000, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskNonKernelDispatchWhenStateBaseAddressInitializedThenDispatchInitialStateBaseAddressAndIgnoreRequiredStreamProperties,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::nonKernel;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesBindingTableSurfaceState(0x5000, 0x100, 0x6000, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndBindingPoolBaseAddressNeededWhenStateBaseAddressPropertiesNotProvidedForFirstFlushThenDispatchSecondSbaCommandWhenProvided,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);
    EXPECT_EQ(0x1000u, bindingTableAddress->getBindingTablePoolBaseAddress());

    usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    EXPECT_EQ(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndGlobalStatelessHeapWhenStateBaseAddressNotInitializedThenDispatchStateBaseAddressAndNoBindingPoolBaseAddressCommand,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    commandStreamReceiver.createGlobalStatelessHeap();

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, -1, -1, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(0x2000, commandStreamReceiver.streamProperties.stateBaseAddress.surfaceStateBaseAddress.value);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    EXPECT_EQ(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndGlobalStatelessHeapWhenStateBaseAddressNotInitializedThenDispatchStateBaseAddressCommandTwice,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }
    commandStreamReceiver.createGlobalStatelessHeap();

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, -1, -1, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(0x2000, commandStreamReceiver.streamProperties.stateBaseAddress.surfaceStateBaseAddress.value);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesBindingTableSurfaceState(-1, -1, 0x6000, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(0x6000, commandStreamReceiver.streamProperties.stateBaseAddress.surfaceStateBaseAddress.value);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenNextDispatchRequiresScratchSpaceThenFrontEndCommandIsDispatched,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.heaplessModeEnabled = false;

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.setRequiredScratchSizes(0x100, 0);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);

    size_t expectedScratchOffset = 2 * sizeof(RENDER_SURFACE_STATE);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation()));

    commandStreamReceiver.setRequiredScratchSizes(0x400, 0);

    usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);

    expectedScratchOffset = 4 * sizeof(RENDER_SURFACE_STATE);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenNextDispatchRequiresPrivateScratchSpaceThenFrontEndCommandIsDispatched,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.heaplessModeEnabled = false;

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.setRequiredScratchSizes(0, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);

    constexpr size_t expectedScratchOffset = 2 * sizeof(RENDER_SURFACE_STATE);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot1Allocation()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenOneTimeContextSystemFenceRequiredThenExpectOneTimeSystemFenceCommand,
          IsHeapfulRequiredAndAtLeastXeHpcCore) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    if (pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto systemFenceCmd = hwParserCsr.getCommand<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
    ASSERT_NE(nullptr, systemFenceCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getGlobalFenceAllocation()));

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    systemFenceCmd = hwParserCsr.getCommand<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
    EXPECT_EQ(nullptr, systemFenceCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getGlobalFenceAllocation()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenOneTimeContextPartitionConfigRequiredThenExpectOneTimeWparidRegisterCommand,
          IsXeHpcCore) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.staticWorkPartitioningEnabled = true;
    commandStreamReceiver.activePartitions = 2;

    commandStreamReceiver.workPartitionAllocation = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto loadRegisterMemCmd = hwParserCsr.getCommand<MI_LOAD_REGISTER_MEM>();
    ASSERT_NE(nullptr, loadRegisterMemCmd);
    constexpr uint32_t wparidRegister = 0x221C;
    EXPECT_EQ(wparidRegister, loadRegisterMemCmd->getRegisterAddress());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getWorkPartitionAllocation()));

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    loadRegisterMemCmd = hwParserCsr.getCommand<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(nullptr, loadRegisterMemCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getWorkPartitionAllocation()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenRayTracingAllocationCreatedThenOneTimeRayTracingCommandDispatched,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    EXPECT_EQ(nullptr, btdStateCmd);

    pDevice->initializeRayTracing(8);
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    ASSERT_NE(nullptr, btdStateCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(pDevice->getRTMemoryBackedBuffer()));

    usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    EXPECT_EQ(nullptr, btdStateCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(pDevice->getRTMemoryBackedBuffer()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenCsrHasPreambleCommandsThenDispatchIndirectJumpToImmediateBatchBuffer,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto startOffset = commandStream.getUsed();
    *commandStream.getSpaceForCmd<DefaultWalkerType>() = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    uint64_t immediateStartAddress = commandStream.getGpuBase() + startOffset;

    commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto bbStartCmd = hwParserCsr.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStartCmd);
    EXPECT_EQ(immediateStartAddress, bbStartCmd->getBatchBufferStartAddress());

    startOffset = commandStream.getUsed();
    *commandStream.getSpaceForCmd<DefaultWalkerType>() = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    bbStartCmd = hwParserCsr.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_EQ(nullptr, bbStartCmd);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenBlockingCallSelectedThenDispatchPipeControlPostSyncToImmediateBatchBuffer,
          IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heapless = commandStreamReceiver.heaplessModeEnabled;
    auto heaplessPrologProgrammed = commandStreamReceiver.heaplessPrologProgrammed;

    bool additionalSyncCmd = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, commandStreamReceiver.peekRootDeviceEnvironment()) > 0;
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto startOffset = commandStream.getUsed();
    auto immediateListCmdBufferAllocation = commandStream.getGraphicsAllocation();
    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);

    auto csrTagAllocation = commandStreamReceiver.getTagAllocation();
    uint64_t postsyncAddress = csrTagAllocation->getGpuAddress();

    immediateFlushTaskFlags.blockingAppend = true;

    auto completionStamp = heaplessPrologProgrammed ? commandStreamReceiver.flushImmediateTaskHeapless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice)
                                                    : commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    auto expectedCount = heaplessPrologProgrammed ? 2u : 1u;

    EXPECT_EQ(expectedCount, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStream, 0);
    auto cmdItor = hwParserCsr.getCommandItor<PIPE_CONTROL>();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(postsyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd));
    EXPECT_EQ(expectedCount, pipeControlCmd->getImmediateData());

    cmdItor++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    if (additionalSyncCmd) {
        cmdItor++;
        ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    }
    auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdItor);
    ASSERT_NE(nullptr, bbEndCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrTagAllocation));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation));

    startOffset = commandStream.getUsed();
    EXPECT_EQ(0u, (startOffset % MemoryConstants::cacheLineSize));

    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);

    if (heaplessPrologProgrammed) {
        completionStamp = commandStreamReceiver.flushImmediateTaskHeapless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);
    } else {
        completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);
    }

    expectedCount++;
    EXPECT_EQ(expectedCount, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStream, startOffset);
    cmdItor = hwParserCsr.getCommandItor<PIPE_CONTROL>();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(postsyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd));
    EXPECT_EQ(expectedCount, pipeControlCmd->getImmediateData());

    cmdItor++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    if (additionalSyncCmd) {
        cmdItor++;
        ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    }
    bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdItor);
    ASSERT_NE(nullptr, bbEndCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrTagAllocation));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation));

    startOffset = commandStream.getUsed();
    EXPECT_EQ(0u, (startOffset % MemoryConstants::cacheLineSize));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenRequireTaskCountUpdateSelectedThenDispatchPipeControlPostSyncToImmediateBatchBuffer,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heapless = commandStreamReceiver.heaplessModeEnabled;
    auto heaplessPrologProgrammed = commandStreamReceiver.heaplessPrologProgrammed;

    bool additionalSyncCmd = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, commandStreamReceiver.peekRootDeviceEnvironment()) > 0;
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto startOffset = commandStream.getUsed();
    auto immediateListCmdBufferAllocation = commandStream.getGraphicsAllocation();
    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);
    auto csrTagAllocation = commandStreamReceiver.getTagAllocation();
    uint64_t postsyncAddress = csrTagAllocation->getGpuAddress();

    immediateFlushTaskFlags.requireTaskCountUpdate = true;
    auto completionStamp = heaplessPrologProgrammed ? commandStreamReceiver.flushImmediateTaskHeapless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice)
                                                    : commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    auto expectedCount = heaplessPrologProgrammed ? 2u : 1u;
    EXPECT_EQ(expectedCount, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStream, 0);
    auto cmdItor = hwParserCsr.getCommandItor<PIPE_CONTROL>();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(postsyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd));
    EXPECT_EQ(expectedCount, pipeControlCmd->getImmediateData());

    cmdItor++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    if (additionalSyncCmd) {
        cmdItor++;
        ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    }
    auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdItor);
    ASSERT_NE(nullptr, bbEndCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrTagAllocation));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation));

    startOffset = commandStream.getUsed();
    EXPECT_EQ(0u, (startOffset % MemoryConstants::cacheLineSize));

    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);

    expectedCount++;

    completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);
    EXPECT_EQ(expectedCount, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStream, startOffset);
    cmdItor = hwParserCsr.getCommandItor<PIPE_CONTROL>();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(postsyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd));
    EXPECT_EQ(expectedCount, pipeControlCmd->getImmediateData());

    cmdItor++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    if (additionalSyncCmd) {
        cmdItor++;
        ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    }
    bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdItor);
    ASSERT_NE(nullptr, bbEndCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrTagAllocation));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation));

    startOffset = commandStream.getUsed();
    EXPECT_EQ(0u, (startOffset % MemoryConstants::cacheLineSize));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenPreambleIsUsedOrNotThenCsrBufferIsUsedOrImmediateBufferIsUsed,
          IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    auto startOffset = commandStream.getUsed();
    auto immediateListCmdBufferAllocation = commandStream.getGraphicsAllocation();

    *commandStream.getSpaceForCmd<DefaultWalkerType>() = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    immediateFlushTaskFlags.hasStallingCmds = true;
    auto completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    auto csrCmdBufferAllocation = commandStreamReceiver.commandStream.getGraphicsAllocation();

    TaskCountType currentTaskCountType = 1u;

    EXPECT_EQ(currentTaskCountType, completionStamp.taskCount);
    EXPECT_EQ(currentTaskCountType, commandStreamReceiver.taskCount);
    EXPECT_EQ(currentTaskCountType, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(0u, commandStreamReceiver.latestFlushedTaskCount);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrCmdBufferAllocation, currentTaskCountType));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));

    BatchBuffer &recordedBatchBuffer = commandStreamReceiver.latestFlushedBatchBuffer;
    EXPECT_EQ(csrCmdBufferAllocation, recordedBatchBuffer.commandBufferAllocation);
    EXPECT_EQ(0u, recordedBatchBuffer.startOffset);
    EXPECT_EQ(true, recordedBatchBuffer.hasStallingCmds);
    EXPECT_EQ(false, recordedBatchBuffer.hasRelaxedOrderingDependencies);

    startOffset = commandStream.getUsed();

    *commandStream.getSpaceForCmd<DefaultWalkerType>() = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    immediateFlushTaskFlags.hasRelaxedOrderingDependencies = true;
    completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    currentTaskCountType = 2u;

    EXPECT_EQ(currentTaskCountType, completionStamp.taskCount);
    EXPECT_EQ(currentTaskCountType, commandStreamReceiver.taskCount);
    EXPECT_EQ(currentTaskCountType, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(0u, commandStreamReceiver.latestFlushedTaskCount);

    EXPECT_FALSE(commandStreamReceiver.isMadeResident(csrCmdBufferAllocation, currentTaskCountType));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));

    recordedBatchBuffer = commandStreamReceiver.latestFlushedBatchBuffer;
    EXPECT_EQ(immediateListCmdBufferAllocation, recordedBatchBuffer.commandBufferAllocation);
    EXPECT_EQ(startOffset, recordedBatchBuffer.startOffset);
    EXPECT_EQ(true, recordedBatchBuffer.hasStallingCmds);
    EXPECT_EQ(true, recordedBatchBuffer.hasRelaxedOrderingDependencies);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenFlushOperationFailsThenExpectNoBatchBufferSentAndCorrectFailCompletionReturned,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    bool heapless = commandStreamReceiver.heaplessModeEnabled;
    bool heaplessPrologProgrammed = commandStreamReceiver.heaplessPrologProgrammed;
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    auto startOffset = commandStream.getUsed();
    auto immediateListCmdBufferAllocation = commandStream.getGraphicsAllocation();

    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);

    immediateFlushTaskFlags.blockingAppend = true;
    commandStreamReceiver.flushReturnValue = NEO::SubmissionStatus::failed;

    auto completionStamp = heaplessPrologProgrammed ? commandStreamReceiver.flushImmediateTaskHeapless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice)
                                                    : commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    auto csrCmdBufferAllocation = commandStreamReceiver.commandStream.getGraphicsAllocation();

    TaskCountType currentTaskCountType = 1u;

    auto expectedCount = heaplessPrologProgrammed ? 1u : 0u;

    EXPECT_EQ(NEO::CompletionStamp::failed, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    EXPECT_FALSE(commandStreamReceiver.isMadeResident(csrCmdBufferAllocation, currentTaskCountType));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));

    BatchBuffer &recordedBatchBuffer = commandStreamReceiver.latestFlushedBatchBuffer;
    EXPECT_EQ(nullptr, recordedBatchBuffer.commandBufferAllocation);
    EXPECT_EQ(0u, recordedBatchBuffer.startOffset);
    EXPECT_EQ(false, recordedBatchBuffer.hasStallingCmds);
    EXPECT_EQ(false, recordedBatchBuffer.hasRelaxedOrderingDependencies);

    completionStamp = heaplessPrologProgrammed ? commandStreamReceiver.flushImmediateTaskHeapless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice)
                                               : commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(NEO::CompletionStamp::failed, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    if (heaplessPrologProgrammed) {
        EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));
    } else {
        EXPECT_FALSE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));
    }
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenPreemptionModeProgrammingNeededThenOneTimePreemptionModeDispatchedOnSupportingPlatform,
          IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heaplessPrologProgrammed = commandStreamReceiver.heaplessPrologProgrammed;

    bool preemptionModeProgramming = NEO::PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(pDevice->getPreemptionMode(), commandStreamReceiver.getPreemptionMode()) > 0;
    auto preemptionDetails = getPreemptionTestHwDetails<FamilyType>();

    if (!heaplessPrologProgrammed) {
        EXPECT_EQ(NEO::PreemptionMode::Initial, commandStreamReceiver.getPreemptionMode());
    }

    heaplessPrologProgrammed ? commandStreamReceiver.flushImmediateTaskHeapless(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice)
                             : commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(pDevice->getPreemptionMode(), commandStreamReceiver.getPreemptionMode());

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto loadRegisterImmList = hwParserCsr.getCommandsList<MI_LOAD_REGISTER_IMM>();
    bool foundPreemptionMode = false;
    for (const auto &it : loadRegisterImmList) {
        auto loadRegisterImmCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(it);
        if (loadRegisterImmCmd->getRegisterOffset() == preemptionDetails.regAddress) {
            foundPreemptionMode = true;
        }
    }
    EXPECT_EQ(preemptionModeProgramming, foundPreemptionMode);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();

    heaplessPrologProgrammed ? commandStreamReceiver.flushImmediateTaskHeapless(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice)
                             : commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    loadRegisterImmList = hwParserCsr.getCommandsList<MI_LOAD_REGISTER_IMM>();

    foundPreemptionMode = false;
    for (const auto &it : loadRegisterImmList) {
        auto loadRegisterImmCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(it);
        if (loadRegisterImmCmd->getRegisterOffset() == preemptionDetails.regAddress) {
            foundPreemptionMode = true;
        }
    }
    EXPECT_EQ(false, foundPreemptionMode);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenCsrSurfaceProgrammingNeededThenOneTimeCsrSurfaceDispatchedOnSupportingPlatform,
          IsAtLeastXeCore) {

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }
    commandStreamReceiver.storeMakeResidentAllocations = true;
    if (commandStreamReceiver.getPreemptionAllocation() == nullptr) {
        auto createdPreemptionAllocation = commandStreamReceiver.createPreemptionAllocation();
        ASSERT_TRUE(createdPreemptionAllocation);
    }

    bool csrSurfaceProgramming = NEO::PreemptionHelper::getRequiredPreambleSize<FamilyType>(*pDevice) > 0;

    EXPECT_EQ(NEO::PreemptionMode::Initial, commandStreamReceiver.getPreemptionMode());
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    EXPECT_EQ(pDevice->getPreemptionMode(), commandStreamReceiver.getPreemptionMode());

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto itCsrCommand = NEO::UnitTestHelper<FamilyType>::findCsrBaseAddressCommand(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    if (csrSurfaceProgramming) {
        EXPECT_NE(hwParserCsr.cmdList.end(), itCsrCommand);
    } else {
        EXPECT_EQ(hwParserCsr.cmdList.end(), itCsrCommand);
    }

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getPreemptionAllocation()));

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    itCsrCommand = NEO::UnitTestHelper<FamilyType>::findCsrBaseAddressCommand(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    EXPECT_EQ(hwParserCsr.cmdList.end(), itCsrCommand);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getPreemptionAllocation()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenSipProgrammingNeededThenOneTimeSipStateDispatched,
          IsAtLeastXeCore) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);
    pDevice->getL0Debugger()->initialize();

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    commandStreamReceiver.storeMakeResidentAllocations = true;

    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    commandStreamReceiver.setSipSentFlag(true);
    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());
    commandStreamReceiver.setSipSentFlag(false);
    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    auto debugSurfaceSize = 64u;
    NEO::GraphicsAllocation *debugSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(), true,
         debugSurfaceSize,
         NEO::AllocationType::debugContextSaveArea,
         false,
         false,
         pDevice->getDeviceBitfield()});
    pDevice->setDebugSurface(debugSurface);

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    auto sipAllocation = NEO::SipKernel::getSipKernel(*pDevice, nullptr).getSipAllocation();

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(sipAllocation));

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateSipCmd = hwParserCsr.getCommand<STATE_SIP>();
    ASSERT_NE(nullptr, stateSipCmd);

    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateSipCmd = hwParserCsr.getCommand<STATE_SIP>();
    EXPECT_EQ(nullptr, stateSipCmd);

    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(sipAllocation));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenOfflineDebuggingModeWhenFlushTaskImmediateCalledThenCorrectContextSipIsResident,
          IsAtLeastXeCore) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);
    pDevice->getL0Debugger()->initialize();
    pDevice->getExecutionEnvironment()->setDebuggingMode(DebuggingMode::offline);
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }
    commandStreamReceiver.storeMakeResidentAllocations = true;

    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    commandStreamReceiver.setSipSentFlag(true);
    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());
    commandStreamReceiver.setSipSentFlag(false);
    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    auto debugSurfaceSize = 64u;
    NEO::GraphicsAllocation *debugSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(), true,
         debugSurfaceSize,
         NEO::AllocationType::debugContextSaveArea,
         false,
         false,
         pDevice->getDeviceBitfield()});
    pDevice->setDebugSurface(debugSurface);

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    auto sipAllocation = NEO::SipKernel::getSipKernel(*pDevice, &commandStreamReceiver.getOsContext()).getSipAllocation();

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(sipAllocation));

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateSipCmd = hwParserCsr.getCommand<STATE_SIP>();
    ASSERT_NE(nullptr, stateSipCmd);

    auto expectedAddress = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo) ? sipAllocation->getGpuAddress() : sipAllocation->getGpuAddressToPatch();
    EXPECT_EQ(expectedAddress, stateSipCmd->getSystemInstructionPointer());

    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());

    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(sipAllocation));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTest, givenScratchSpaceSurfaceStateEnabledWhenRequiredScratchSpaceIsSetThenPerThreadScratchSizeIsAlignedNextPow2) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());

    uint32_t perThreadScratchSize = 65;
    uint32_t expectedValue = Math::nextPowerOfTwo(perThreadScratchSize);

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    uint8_t surfaceHeap[1000];
    scratchController->setRequiredScratchSpace(surfaceHeap, 0u, perThreadScratchSize, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_EQ(expectedValue, scratchController->perThreadScratchSpaceSlot0Size);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTest, givenScratchSpaceSurfaceStateEnabledWhenSizeForPrivateScratchSpaceIsMisalignedThenAlignItNextPow2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[4];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    uint32_t misalignedSizeForPrivateScratch = MemoryConstants::pageSize + 1;
    uint32_t alignedSizeForPrivateScratch = Math::nextPowerOfTwo(misalignedSizeForPrivateScratch);

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;
    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, misalignedSizeForPrivateScratch,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);

    EXPECT_NE(scratchController->perThreadScratchSpaceSlot1Size, misalignedSizeForPrivateScratch);
    EXPECT_EQ(scratchController->perThreadScratchSpaceSlot1Size, alignedSizeForPrivateScratch);

    size_t scratchSlot1SizeInBytes = alignedSizeForPrivateScratch * scratchController->computeUnitsUsedForScratch;
    auto &productHelper = pDevice->getProductHelper();
    productHelper.adjustScratchSize(scratchSlot1SizeInBytes);

    EXPECT_EQ(scratchController->scratchSlot1SizeInBytes, scratchSlot1SizeInBytes);

    EXPECT_EQ(scratchController->scratchSlot1SizeInBytes, scratchController->getScratchSpaceSlot1Allocation()->getUnderlyingBufferSize());
}

HWTEST_F(CommandStreamReceiverHwTest, givenDcFlushRequiredWhenProgramStallingPostSyncCommandsForBarrierCalledThenDcFlushSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.dcFlushSupport = true;
    if (ultCsr.isMultiTileOperationEnabled()) {
        GTEST_SKIP();
    }
    char commandBuffer[MemoryConstants::pageSize];
    LinearStream commandStream(commandBuffer, MemoryConstants::pageSize);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();
    constexpr bool dcFlushRequired = true;
    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, dcFlushRequired);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandStream.getCpuBase(),
        commandStream.getUsed()));
    auto pipeControlIteratorVector = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_GE(pipeControlIteratorVector.size(), 1u);
    auto pipeControlIterator = pipeControlIteratorVector[0];
    const bool barrierWaRequired = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment());
    if (barrierWaRequired) {
        ASSERT_GE(pipeControlIteratorVector.size(), 2u);
        pipeControlIterator = pipeControlIteratorVector[1];
    }
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverHwTest, givenDcFlushRequiredButNoDcFlushSupportWhenProgramStallingPostSyncCommandsForBarrierCalledThenDcFlushNotSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.dcFlushSupport = false;
    if (ultCsr.isMultiTileOperationEnabled()) {
        GTEST_SKIP();
    }
    char commandBuffer[MemoryConstants::pageSize];
    LinearStream commandStream(commandBuffer, MemoryConstants::pageSize);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();
    constexpr bool dcFlushRequired = true;
    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, dcFlushRequired);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandStream.getCpuBase(),
        commandStream.getUsed()));
    auto pipeControlIteratorVector = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_GE(pipeControlIteratorVector.size(), 1u);
    auto pipeControlIterator = pipeControlIteratorVector[0];
    const bool barrierWaRequired = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment());
    if (barrierWaRequired) {
        ASSERT_GE(pipeControlIteratorVector.size(), 2u);
        pipeControlIterator = pipeControlIteratorVector[1];
    }
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverHwTest, givenDcFlushRequiredFalseWhenProgramStallingPostSyncCommandsForBarrierCalledThenDcFlushNotSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.dcFlushSupport = true;
    if (ultCsr.isMultiTileOperationEnabled()) {
        GTEST_SKIP();
    }
    char commandBuffer[MemoryConstants::pageSize];
    LinearStream commandStream(commandBuffer, MemoryConstants::pageSize);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();
    constexpr bool dcFlushRequired = false;
    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, dcFlushRequired);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandStream.getCpuBase(),
        commandStream.getUsed()));
    auto pipeControlIteratorVector = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_GE(pipeControlIteratorVector.size(), 1u);
    auto pipeControlIterator = pipeControlIteratorVector[0];
    const bool barrierWaRequired = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment());
    if (barrierWaRequired) {
        ASSERT_GE(pipeControlIteratorVector.size(), 2u);
        pipeControlIterator = pipeControlIteratorVector[1];
    }
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenNextDispatchRequiresScratchSpaceAndSshPointerIsNullThenFrontEndCommandIsNotDispatched,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    commandStreamReceiver.setRequiredScratchSizes(0x100, 0);
    immediateFlushTaskFlags.sshCpuBase = nullptr;

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    EXPECT_EQ(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenCleanUpResourcesThenOwnedPrivateAllocationsAreFreed) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockGA = std::make_unique<MockGraphicsAllocation>();

    auto mapForReuse = &csr.getOwnedPrivateAllocations();
    mapForReuse->push_back({0x100, mockGA.release()});
    csr.cleanupResources();
    EXPECT_EQ(mapForReuse->size(), 0u);
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenDirtyFlagForContextInBindlessHelperWhenFlushTaskCalledThenStateCacheInvalidateIsSent, IsHeapfulRequired) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.at(commandStreamReceiver.getOsContext().getContextId()) = true;

    flushTaskFlags.implicitFlush = true;
    auto usedSpaceBefore = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    auto usedSpaceAfter = commandStreamReceiver.commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandStreamReceiver.commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControls.size());

    bool pcFound = false;
    for (size_t i = 0; i < pipeControls.size(); i++) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[i]);
        bool csStall = pipeControl->getCommandStreamerStallEnable();
        bool stateCache = pipeControl->getStateCacheInvalidationEnable();
        bool texCache = pipeControl->getTextureCacheInvalidationEnable();
        bool renderTargetCache = true;
        if constexpr (UsesRenderPipeControlFlags<FamilyType>) {
            renderTargetCache = pipeControl->getRenderTargetCacheFlushEnable();
        }

        if (csStall && stateCache && texCache && renderTargetCache) {
            pcFound = true;
            break;
        }
    }
    EXPECT_TRUE(pcFound);
    EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenDirtyFlagForContextInBindlessHelperWhenHeaplessFlushTaskCalledThenStateCacheInvalidateIsSent, IsHeaplessRequired) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    flushTaskFlags.implicitFlush = true;

    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);
    auto usedAfterFirstFlush = commandStreamReceiver.commandStream.getUsed();

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.at(commandStreamReceiver.getOsContext().getContextId()) = true;

    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);
    auto usedSpaceAfter = commandStreamReceiver.commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedAfterFirstFlush);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandStreamReceiver.commandStream.getCpuBase(), usedAfterFirstFlush), usedSpaceAfter - usedAfterFirstFlush));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControls.size());

    bool pcFound = false;
    for (size_t i = 0; i < pipeControls.size(); i++) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[i]);
        bool renderTargetCache = true;
        if constexpr (UsesRenderPipeControlFlags<FamilyType>) {
            renderTargetCache = pipeControl->getRenderTargetCacheFlushEnable();
        }
        if (pipeControl->getCommandStreamerStallEnable() &&
            pipeControl->getStateCacheInvalidationEnable() &&
            pipeControl->getTextureCacheInvalidationEnable() &&
            renderTargetCache) {
            pcFound = true;
            break;
        }
    }
    EXPECT_TRUE(pcFound);
    EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenNoDirtyFlagForContextInBindlessHelperWhenHeaplessFlushTaskCalledThenStateCacheInvalidateIsNotSent, IsHeaplessRequired) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.at(commandStreamReceiver.getOsContext().getContextId()) = false;

    flushTaskFlags.implicitFlush = true;

    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);
    auto usedAfterFirstFlush = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);
    auto usedSpaceAfter = commandStreamReceiver.commandStream.getUsed();

    EXPECT_EQ(usedAfterFirstFlush, usedSpaceAfter);
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenDirtyFlagForContextInBindlessHelperWhenFlushImmediateTaskCalledThenStateCacheInvalidateIsSent, IsHeapfulRequired) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.at(commandStreamReceiver.getOsContext().getContextId()) = true;

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pcCmd = hwParserCsr.getCommand<PIPE_CONTROL>();
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_TRUE(pcCmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(pcCmd->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pcCmd->getTextureCacheInvalidationEnable());
    if constexpr (UsesRenderPipeControlFlags<FamilyType>) {
        EXPECT_TRUE(pcCmd->getRenderTargetCacheFlushEnable());
    }

    EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));
}

template <typename FamilyType>
bool findStateCacheInvalidationWithDcFlush(LinearStream &commandStream, size_t startOffset, bool dcFlush) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, startOffset);
    for (auto &cmd : hwParser.cmdList) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(cmd);
        if (pipeControl != nullptr &&
            pipeControl->getStateCacheInvalidationEnable() &&
            pipeControl->getTextureCacheInvalidationEnable() &&
            pipeControl->getDcFlushEnable() == dcFlush) {
            return true;
        }
    }
    return false;
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenDirtyFlagForContextInBindlessHelperAndDcFlushAllowedOrNotWhenFlushTaskCalledThenStateCacheInvalidationFlushesDataCacheWhenAllowed, IsHeapfulRequired) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    flushTaskFlags.implicitFlush = true;

    DebugManagerStateRestore restorer;
    for (const bool dcFlushAllowed : {false, true}) {
        debugManager.flags.AllowDcFlush.set(dcFlushAllowed);
        bindlessHeapsHelperPtr->stateCacheDirtyForContext.at(commandStreamReceiver.getOsContext().getContextId()) = true;
        auto usedBefore = commandStreamReceiver.commandStream.getUsed();

        commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);

        EXPECT_TRUE(findStateCacheInvalidationWithDcFlush<FamilyType>(commandStreamReceiver.commandStream, usedBefore, dcFlushAllowed));
        EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));
    }
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenDirtyFlagForContextInBindlessHelperAndDcFlushAllowedOrNotWhenHeaplessFlushTaskCalledThenStateCacheInvalidationFlushesDataCacheWhenAllowed, IsHeaplessRequired) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    flushTaskFlags.implicitFlush = true;

    commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);

    DebugManagerStateRestore restorer;
    for (const bool dcFlushAllowed : {false, true}) {
        debugManager.flags.AllowDcFlush.set(dcFlushAllowed);
        bindlessHeapsHelperPtr->stateCacheDirtyForContext.at(commandStreamReceiver.getOsContext().getContextId()) = true;
        auto usedBefore = commandStreamReceiver.commandStream.getUsed();

        commandStreamReceiver.flushTask(commandStream, 0, &dsh, &ioh, nullptr, taskLevel, flushTaskFlags, *pDevice);

        EXPECT_TRUE(findStateCacheInvalidationWithDcFlush<FamilyType>(commandStreamReceiver.commandStream, usedBefore, dcFlushAllowed));
        EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));
    }
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenDirtyFlagForContextInBindlessHelperAndDcFlushAllowedOrNotWhenFlushImmediateTaskCalledThenStateCacheInvalidationFlushesDataCacheWhenAllowed, IsHeapfulRequired) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    DebugManagerStateRestore restorer;
    for (const bool dcFlushAllowed : {false, true}) {
        debugManager.flags.AllowDcFlush.set(dcFlushAllowed);
        bindlessHeapsHelperPtr->stateCacheDirtyForContext.at(commandStreamReceiver.getOsContext().getContextId()) = true;
        auto usedBefore = commandStreamReceiver.commandStream.getUsed();

        commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

        EXPECT_TRUE(findStateCacheInvalidationWithDcFlush<FamilyType>(commandStreamReceiver.commandStream, usedBefore, dcFlushAllowed));
        EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));
    }
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenContextInitializedAndDirtyFlagForContextInBindlessHelperWhenFlushImmediateTaskCalledThenStateCacheInvalidateIsSentBeforeBbStartJumpingToImmediateBuffer, IsHeapfulRequired) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // 1st flush, dispatch all required n-p state commands
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    auto usedAfterFirstFlush = commandStreamReceiver.commandStream.getUsed();

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.at(commandStreamReceiver.getOsContext().getContextId()) = true;

    // only state cache flush is dispatched in dynamic preamble
    auto immediateBufferStartOffset = commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, immediateBufferStartOffset, immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedAfterFirstFlush);

    auto cmdPtrIt = hwParserCsr.cmdList.begin();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdPtrIt);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdPtrIt);
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    if constexpr (UsesRenderPipeControlFlags<FamilyType>) {
        EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());
    }

    EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));

    cmdPtrIt++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdPtrIt);

    auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmdPtrIt);
    ASSERT_NE(nullptr, bbStart);

    EXPECT_EQ(cmdBufferGpuAddress + immediateBufferStartOffset, bbStart->getBatchBufferStartAddress());
}

HWTEST_F(CommandStreamReceiverHwTest, givenRequiresInstructionCacheFlushWhenFlushImmediateThenInstructionCacheInvalidateEnableIsSent) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }
    commandStreamReceiver.registerInstructionCacheFlush();

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pcCmd = hwParserCsr.getCommand<PIPE_CONTROL>();
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_TRUE(pcCmd->getInstructionCacheInvalidateEnable());
    EXPECT_FALSE(commandStreamReceiver.requiresInstructionCacheFlush);
}
