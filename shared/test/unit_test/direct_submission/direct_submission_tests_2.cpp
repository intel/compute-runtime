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
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_direct_submission_diagnostic_collector.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_io_functions.h"

#include "test.h"

using DirectSubmissionTest = Test<DirectSubmissionFixture>;

using DirectSubmissionDispatchBufferTest = Test<DirectSubmissionDispatchBufferFixture>;

HWCMDTEST_F(IGFX_GEN12_CORE, DirectSubmissionDispatchBufferTest,
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
    directSubmission.activeTiles = 2;
    directSubmission.partitionedMode = true;
    directSubmission.workPartitionAllocation = ultCsr->getWorkPartitionAllocation();

    bool ret = directSubmission.initialize(true);
    EXPECT_TRUE(ret);
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
    auto loadRegisterImm = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*hwParse.lriList.begin());
    EXPECT_EQ(0x23B4u, loadRegisterImm->getRegisterOffset());
    EXPECT_EQ(8u, loadRegisterImm->getDataDword());

    auto loadRegisterMemItor = find<MI_LOAD_REGISTER_MEM *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.lriList.end(), loadRegisterMemItor);
    auto loadRegisterMem = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(*loadRegisterMemItor);
    EXPECT_EQ(0x23B4u, loadRegisterMem->getRegisterOffset());
    uint64_t gpuAddress = ultCsr->getWorkPartitionAllocation()->getGpuAddress();
    EXPECT_EQ(gpuAddress, loadRegisterMem->getMemoryAddress());
}
