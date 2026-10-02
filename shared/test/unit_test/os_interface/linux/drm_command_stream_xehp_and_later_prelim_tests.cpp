/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture_prelim.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_prelim_fixtures.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <sstream>

using namespace NEO;

struct DrmCommandStreamEnhancedTestDrmPrelim : public DrmCommandStreamEnhancedTemplate<DrmMockCustomPrelim> {
    void SetUp() override {
        debugManager.flags.UseVmBind.set(1u);
        DrmCommandStreamEnhancedTemplate::SetUp();
    }
    void TearDown() override {
        DrmCommandStreamEnhancedTemplate::TearDown();
        dbgState.reset();
    }

    DebugManagerStateRestore restorer;
};

HWTEST2_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenEnableImmediateVmBindExtSetWhenFlushThenWaitUserFenceIoctlIsCalled, IsXeCore) {
    debugManager.flags.EnableImmediateVmBindExt.set(1);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(*allocation);

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_NE(0u, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(2u, mock->context.receivedGemWaitUserFence.value);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(DrmPrelimHelper::getSoftWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU64WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST2_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenDirectSubmissionEnabledWhenFlushThenWaitUserFenceIoctlIsCalled, IsXeCore) {
    this->mock->setDirectSubmissionActive(true);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(*allocation);

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_NE(0u, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(2u, mock->context.receivedGemWaitUserFence.value);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(DrmPrelimHelper::getSoftWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU64WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);

    this->mock->setDirectSubmissionActive(false);

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST2_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenWaitUserFenceEnabledWhenUseCtxIdSelectedThenExpectNonZeroContextId, IsXeCore) {
    auto osContextLinux = static_cast<const OsContextLinux *>(device->getDefaultEngine().osContext);
    std::vector<uint32_t> &drmCtxIds = const_cast<std::vector<uint32_t> &>(osContextLinux->getDrmContextIds());
    size_t drmCtxSize = drmCtxIds.size();
    for (uint32_t i = 0; i < drmCtxSize; i++) {
        drmCtxIds[i] = 5u + i;
    }

    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testDrmCsr->useUserFenceWait = true;
    testDrmCsr->activePartitions = static_cast<uint32_t>(drmCtxSize);

    const auto hasFirstSubmission = device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo) ? 1 : 0;
    auto tagPtr = const_cast<TagAddressType *>(testDrmCsr->getTagAddress());
    *tagPtr = hasFirstSubmission;
    uint64_t tagAddress = castToUint64(tagPtr);
    FlushStamp handleToWait = 123;
    testDrmCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1u, testDrmCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testDrmCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(drmCtxSize, mock->context.gemWaitUserFenceCalled);
    EXPECT_EQ(tagAddress, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(handleToWait, mock->context.receivedGemWaitUserFence.value);
    EXPECT_NE(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU64WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);
}

HWTEST2_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenWaitUserFenceEnabledWhenUseCtxIdNotSelectedAndMultiplePartitionsThenExpectZeroContextIdAndEqualWaitCalls, IsXeCore) {
    auto osContextLinux = static_cast<const OsContextLinux *>(device->getDefaultEngine().osContext);
    std::vector<uint32_t> &drmCtxIds = const_cast<std::vector<uint32_t> &>(osContextLinux->getDrmContextIds());
    size_t drmCtxSize = drmCtxIds.size();
    for (uint32_t i = 0; i < drmCtxSize; i++) {
        drmCtxIds[i] = 5u + i;
    }

    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testDrmCsr->useUserFenceWait = true;
    testDrmCsr->activePartitions = 2u;
    EXPECT_NE(0u, testDrmCsr->immWritePostSyncWriteOffset);

    auto rootExecEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    auto &gfxCoreHelper = rootExecEnvironment->getHelper<GfxCoreHelper>();
    auto hwInfo = rootExecEnvironment->getHardwareInfo();

    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*rootExecEnvironment)[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*hwInfo), DeviceBitfield(3)));

    osContext->ensureContextInitialized();
    osContext->incRefInternal();

    device->getMemoryManager()->unregisterEngineForCsr(testDrmCsr);

    device->allEngines[0].osContext = osContext.get();

    testDrmCsr->setupContext(*osContext);

    auto tagPtr = testDrmCsr->getTagAddress();
    *tagPtr = 0;

    tagPtr = ptrOffset(tagPtr, testDrmCsr->immWritePostSyncWriteOffset);
    *tagPtr = 0;

    uint64_t tagAddress = castToUint64(const_cast<TagAddressType *>(testDrmCsr->getTagAddress()));
    FlushStamp handleToWait = 123;
    testDrmCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1u, testDrmCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testDrmCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(2u, mock->context.gemWaitUserFenceCalled);
    EXPECT_EQ(tagAddress + testDrmCsr->immWritePostSyncWriteOffset, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(handleToWait, mock->context.receivedGemWaitUserFence.value);
    EXPECT_NE(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU64WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);
}

HWTEST2_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenExternalInterruptIdWhenWaitingTheExecuteFenceWaitOnce, IsXeCore) {
    auto osContextLinux = static_cast<const OsContextLinux *>(device->getDefaultEngine().osContext);
    std::vector<uint32_t> &drmCtxIds = const_cast<std::vector<uint32_t> &>(osContextLinux->getDrmContextIds());
    size_t drmCtxSize = drmCtxIds.size();
    for (uint32_t i = 0; i < drmCtxSize; i++) {
        drmCtxIds[i] = 5u + i;
    }

    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testDrmCsr->useUserFenceWait = true;
    testDrmCsr->activePartitions = 2u;
    EXPECT_NE(0u, testDrmCsr->immWritePostSyncWriteOffset);
    EXPECT_TRUE(testDrmCsr->waitUserFenceSupported(nullptr));

    auto rootExecEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    auto &gfxCoreHelper = rootExecEnvironment->getHelper<GfxCoreHelper>();
    auto hwInfo = rootExecEnvironment->getHardwareInfo();

    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*rootExecEnvironment)[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*hwInfo), DeviceBitfield(3)));

    osContext->ensureContextInitialized();
    osContext->incRefInternal();

    device->getMemoryManager()->unregisterEngineForCsr(testDrmCsr);

    device->allEngines[0].osContext = osContext.get();

    testDrmCsr->setupContext(*osContext);

    auto tagPtr = testDrmCsr->getTagAddress();
    *tagPtr = 0;

    tagPtr = ptrOffset(tagPtr, testDrmCsr->immWritePostSyncWriteOffset);
    *tagPtr = 0;

    uint64_t tagAddress = castToUint64(const_cast<TagAddressType *>(testDrmCsr->getTagAddress()));

    EXPECT_EQ(0u, mock->context.gemWaitUserFenceCalled);

    testDrmCsr->waitUserFence(123, tagAddress, 1, true, NEO::InterruptId::notUsed, nullptr, nullptr);
    EXPECT_EQ(2u, mock->context.gemWaitUserFenceCalled);

    testDrmCsr->waitUserFence(123, tagAddress, 1, true, 0x678, nullptr, nullptr);
    EXPECT_EQ(3u, mock->context.gemWaitUserFenceCalled);
}

HWTEST2_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenFailingIoctlWhenWaitingThenDoEarlyReturn, IsXeCore) {
    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testDrmCsr->useUserFenceWait = true;
    testDrmCsr->activePartitions = 3u;

    auto rootExecEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    auto &gfxCoreHelper = rootExecEnvironment->getHelper<GfxCoreHelper>();
    auto hwInfo = rootExecEnvironment->getHardwareInfo();

    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*rootExecEnvironment)[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*hwInfo), DeviceBitfield(7)));

    osContext->ensureContextInitialized();
    osContext->incRefInternal();

    device->getMemoryManager()->unregisterEngineForCsr(testDrmCsr);

    device->allEngines[0].osContext = osContext.get();

    testDrmCsr->setupContext(*osContext);

    mock->waitUserFenceCall.failSpecificCall = 2;

    FlushStamp handleToWait = 123;

    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    auto tagPtr = testDrmCsr->getTagAddress();
    *tagPtr = 0;

    tagPtr = ptrOffset(tagPtr, testDrmCsr->immWritePostSyncWriteOffset);
    *tagPtr = 0;

    tagPtr = ptrOffset(tagPtr, testDrmCsr->immWritePostSyncWriteOffset);
    *tagPtr = 0;

    testDrmCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(2u, mock->waitUserFenceCall.called);
}
