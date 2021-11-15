/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/direct_submission_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_direct_submission_diagnostic_collector.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_io_functions.h"

#include "test.h"

using DirectSubmissionTest = Test<DirectSubmissionFixture>;

using DirectSubmissionDispatchBufferTest = Test<DirectSubmissionDispatchBufferFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, DirectSubmissionDispatchBufferTest,
            givenDirectSubmissionInPartitionModeWhenDispatchingCommandBufferThenExpectDispatchPartitionedPipeControlInCommandBuffer) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    FlushStampTracker flushStamp(true);

    pDevice->rootCsrCreated = true;
    pDevice->numSubDevices = 2;

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*pDevice);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice,
                                                                                      *osContext.get());
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    directSubmission.partitionConfigSet = false;
    directSubmission.disableMonitorFence = false;
    directSubmission.partitionedMode = true;
    directSubmission.workPartitionAllocation = ultCsr->getWorkPartitionAllocation();

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());
    GraphicsAllocation *oldRingAllocation = directSubmission.ringCommandStream.getGraphicsAllocation();

    EXPECT_EQ(0u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(1u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection() +
                        sizeof(MI_LOAD_REGISTER_IMM) +
                        sizeof(MI_LOAD_REGISTER_MEM);
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(oldRingAllocation->getGpuAddress(), directSubmission.submitGpuAddress);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);

    directSubmission.ringCommandStream.getSpace(directSubmission.ringCommandStream.getAvailableSpace() -
                                                directSubmission.getSizeSwitchRingBufferSection());

    directSubmission.tagValueSetValue = 0x4343123ull;
    directSubmission.tagAddressSetValue = 0xBEEF00000ull;
    ret = directSubmission.dispatchCommandBuffer(batchBuffer, flushStamp);
    EXPECT_TRUE(ret);
    EXPECT_NE(oldRingAllocation, directSubmission.ringCommandStream.getGraphicsAllocation());
    EXPECT_EQ(1u, directSubmission.semaphoreData->QueueWorkCount);
    EXPECT_EQ(2u, directSubmission.currentQueueWorkCount);
    EXPECT_EQ(1u, directSubmission.submitCount);
    EXPECT_EQ(2u, directSubmission.handleResidencyCount);

    EXPECT_EQ(directSubmission.getSizeDispatch(), directSubmission.ringCommandStream.getUsed());
    EXPECT_TRUE(directSubmission.ringStart);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();

    bool foundFenceUpdate = false;
    for (auto &it : hwParse.pipeControlList) {
        PIPE_CONTROL *pipeControl = reinterpret_cast<PIPE_CONTROL *>(it);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            foundFenceUpdate = true;
            EXPECT_EQ(directSubmission.tagAddressSetValue, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
            uint64_t data = pipeControl->getImmediateData();
            EXPECT_EQ(directSubmission.tagValueSetValue, data);
            EXPECT_TRUE(pipeControl->getWorkloadPartitionIdOffsetEnable());
            break;
        }
    }
    EXPECT_TRUE(foundFenceUpdate);
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingStartWhenMultiTileSupportedThenExpectMultiTileConfigSetAndWorkPartitionResident) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    pDevice->rootCsrCreated = true;
    pDevice->numSubDevices = 2;

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*pDevice);

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice,
                                                                                      *osContext.get());
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    directSubmission.activeTiles = 2;
    directSubmission.partitionedMode = true;
    directSubmission.partitionConfigSet = false;
    directSubmission.workPartitionAllocation = ultCsr->getWorkPartitionAllocation();

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    EXPECT_NE(0x0u, directSubmission.ringCommandStream.getUsed());

    size_t submitSize = RenderDispatcher<FamilyType>::getSizePreemption() +
                        directSubmission.getSizeSemaphoreSection() +
                        sizeof(MI_LOAD_REGISTER_IMM) +
                        sizeof(MI_LOAD_REGISTER_MEM);
    EXPECT_EQ(submitSize, directSubmission.submitSize);
    EXPECT_EQ(1u, directSubmission.handleResidencyCount);
    EXPECT_EQ(4u, directSubmission.makeResourcesResidentVectorSize);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();

    ASSERT_NE(hwParse.lriList.end(), hwParse.lriList.begin());
    bool partitionRegisterFound = false;
    for (auto &it : hwParse.lriList) {
        auto loadRegisterImm = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(it);
        if (loadRegisterImm->getRegisterOffset() == 0x23B4u) {

            EXPECT_EQ(8u, loadRegisterImm->getDataDword());
            partitionRegisterFound = true;
        }
    }
    EXPECT_TRUE(partitionRegisterFound);

    auto loadRegisterMemItor = find<MI_LOAD_REGISTER_MEM *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), loadRegisterMemItor);
    auto loadRegisterMem = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(*loadRegisterMemItor);
    EXPECT_EQ(0x221Cu, loadRegisterMem->getRegisterAddress());
    uint64_t gpuAddress = ultCsr->getWorkPartitionAllocation()->getGpuAddress();
    EXPECT_EQ(gpuAddress, loadRegisterMem->getMemoryAddress());
}

HWTEST_F(DirectSubmissionDispatchBufferTest,
         givenDirectSubmissionRingNotStartOnInitWhenMultiTileSupportedThenExpectMultiTileConfigSetDuringExplicitRingStart) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    pDevice->rootCsrCreated = true;
    pDevice->numSubDevices = 2;

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*pDevice);

    FlushStampTracker flushStamp(true);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice,
                                                                                      *osContext.get());
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    directSubmission.activeTiles = 2;
    directSubmission.partitionedMode = true;
    directSubmission.partitionConfigSet = false;
    directSubmission.workPartitionAllocation = ultCsr->getWorkPartitionAllocation();

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(directSubmission.partitionConfigSet);
    EXPECT_FALSE(directSubmission.ringStart);
    EXPECT_EQ(0x0u, directSubmission.ringCommandStream.getUsed());

    ret = directSubmission.startRingBuffer();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(directSubmission.partitionConfigSet);
    EXPECT_TRUE(directSubmission.ringStart);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();

    ASSERT_NE(hwParse.lriList.end(), hwParse.lriList.begin());
    bool partitionRegisterFound = false;
    for (auto &it : hwParse.lriList) {
        auto loadRegisterImm = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(it);
        if (loadRegisterImm->getRegisterOffset() == 0x23B4u) {

            EXPECT_EQ(8u, loadRegisterImm->getDataDword());
            partitionRegisterFound = true;
        }
    }
    EXPECT_TRUE(partitionRegisterFound);

    auto loadRegisterMemItor = find<MI_LOAD_REGISTER_MEM *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), loadRegisterMemItor);
    auto loadRegisterMem = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(*loadRegisterMemItor);
    EXPECT_EQ(0x221Cu, loadRegisterMem->getRegisterAddress());
    uint64_t gpuAddress = ultCsr->getWorkPartitionAllocation()->getGpuAddress();
    EXPECT_EQ(gpuAddress, loadRegisterMem->getMemoryAddress());
}
