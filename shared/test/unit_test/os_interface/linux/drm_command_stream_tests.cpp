/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/os_interface/linux/drm_buffer_object_fixture.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} //namespace NEO
using namespace NEO;

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenL0ApiConfigWhenCreatingDrmCsrThenEnableImmediateDispatch) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    MockDrmCsr<FamilyType> csr(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, whenGettingCompletionValueThenTaskCountOfAllocationIsReturned) {
    MockGraphicsAllocation allocation{};
    uint32_t expectedValue = 0x1234;
    allocation.updateTaskCount(expectedValue, osContext->getContextId());
    EXPECT_EQ(expectedValue, csr->getCompletionValue(allocation));
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenEnabledDirectSubmissionWhenGettingCompletionValueThenCompletionFenceValueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDrmCompletionFence.set(1);
    DebugManager.flags.EnableDirectSubmission.set(1);
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(0);
    MockDrmCsr<FamilyType> csr(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
    csr.setupContext(*osContext);
    EXPECT_EQ(nullptr, csr.completionFenceValuePointer);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[osContext->getEngineType()].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[osContext->getEngineType()].submitOnInit = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[osContext->getEngineType()].useNonDefault = true;
    csr.createGlobalFenceAllocation();
    csr.initializeTagAllocation();
    csr.initDirectSubmission();

    EXPECT_NE(nullptr, csr.completionFenceValuePointer);

    uint32_t expectedValue = 0x5678;
    *csr.completionFenceValuePointer = expectedValue;
    MockGraphicsAllocation allocation{};
    uint32_t notExpectedValue = 0x1234;
    allocation.updateTaskCount(notExpectedValue, osContext->getContextId());
    EXPECT_EQ(expectedValue, csr.getCompletionValue(allocation));
    *csr.completionFenceValuePointer = 0;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, whenGettingCompletionAddressThenOffsettedTagAddressIsReturned) {
    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAddress());
    uint64_t tagAddress = castToUint64(const_cast<uint32_t *>(csr->getTagAddress()));
    auto expectedAddress = tagAddress + Drm::completionFenceOffset;
    EXPECT_EQ(expectedAddress, csr->getCompletionAddress());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenNoTagAddressWhenGettingCompletionAddressThenZeroIsReturned) {
    EXPECT_EQ(nullptr, csr->getTagAddress());
    EXPECT_EQ(0u, csr->getCompletionAddress());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenExecBufferErrorWhenFlushInternalThenProperErrorIsReturned) {
    mock->execBufferResult = -1;
    mock->baseErrno = false;
    mock->errnoRetVal = EWOULDBLOCK;
    TestedBufferObject bo(mock, 128);
    MockDrmAllocation cmdBuffer(AllocationType::COMMAND_BUFFER, MemoryPool::System4KBPages);
    cmdBuffer.bufferObjects[0] = &bo;
    uint8_t buff[128]{};

    LinearStream cs(&cmdBuffer, buff, 128);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    auto ret = static_cast<MockDrmCsr<FamilyType> *>(csr)->flushInternal(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(SubmissionStatus::OUT_OF_HOST_MEMORY, ret);
}
