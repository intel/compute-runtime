/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/api/api.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/cl_blit_properties.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include <cinttypes>

using namespace NEO;
struct BcsBufferTests : public ::testing::Test {
    template <typename FamilyType>
    class MyMockCsr : public UltCommandStreamReceiver<FamilyType> {
      public:
        using UltCommandStreamReceiver<FamilyType>::UltCommandStreamReceiver;

        WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait,
                                                         bool useQuickKmdSleep, QueueThrottle throttle) override {
            EXPECT_EQ(this->latestFlushedTaskCount, taskCountToWait);
            EXPECT_EQ(0u, flushStampToWait);
            EXPECT_FALSE(useQuickKmdSleep);
            EXPECT_EQ(throttle, QueueThrottle::MEDIUM);
            EXPECT_EQ(1u, this->activePartitions);
            waitForTaskCountWithKmdNotifyFallbackCalled++;

            return WaitStatus::Ready;
        }

        WaitStatus waitForTaskCountAndCleanTemporaryAllocationList(uint32_t requiredTaskCount) override {
            EXPECT_EQ(1u, waitForTaskCountWithKmdNotifyFallbackCalled);
            EXPECT_EQ(this->latestFlushedTaskCount, requiredTaskCount);
            waitForTaskCountAndCleanAllocationListCalled++;

            return WaitStatus::Ready;
        }

        uint32_t waitForTaskCountAndCleanAllocationListCalled = 0;
        uint32_t waitForTaskCountWithKmdNotifyFallbackCalled = 0;
        CommandStreamReceiver *gpgpuCsr = nullptr;
    };

    template <typename FamilyType>
    void setUpT() {
        if (is32bit) {
            GTEST_SKIP();
        }
        REQUIRE_SVM_OR_SKIP(defaultHwInfo);
        DebugManager.flags.EnableTimestampPacket.set(1);
        DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);
        DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(1);
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
        auto hwInfo = *defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = true;
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

        if (!HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isBlitterFullySupported(device->getHardwareInfo())) {
            GTEST_SKIP();
        }

        bcsMockContext = std::make_unique<BcsMockContext>(device.get());
        commandQueue.reset(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
        bcsCsr = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get())->bcsEngines[0]->commandStreamReceiver;
    }

    template <typename FamilyType>
    void tearDownT() {}

    template <typename FamilyType>
    void waitForCacheFlushFromBcsTest(MockCommandQueueHw<FamilyType> &commandQueue);

    DebugManagerStateRestore restore;

    std::unique_ptr<OsContext> bcsOsContext;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<BcsMockContext> bcsMockContext;
    std::unique_ptr<CommandQueue> commandQueue;
    CommandStreamReceiver *bcsCsr;
    uint32_t hostPtr = 0;
    cl_int retVal = CL_SUCCESS;
};

HWTEST_TEMPLATED_F(BcsBufferTests, givenBufferWithInitializationDataAndBcsCsrWhenCreatingThenUseBlitOperation) {
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsMockContext->bcsCsr.get());

    static_cast<MockMemoryManager *>(device->getExecutionEnvironment()->memoryManager.get())->enable64kbpages[0] = true;
    static_cast<MockMemoryManager *>(device->getExecutionEnvironment()->memoryManager.get())->localMemorySupported[0] = true;

    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);
    auto bufferForBlt = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_COPY_HOST_PTR, 2000, &hostPtr, retVal));
    EXPECT_EQ(1u, bcsCsr->blitBufferCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBufferWithNotDefaultRootDeviceIndexAndBcsCsrWhenCreatingThenUseBlitOperation) {
    auto rootDeviceIndex = 1u;
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    REQUIRE_FULL_BLITTER_OR_SKIP(&hwInfo);
    std::unique_ptr<MockClDevice> newDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, rootDeviceIndex));
    std::unique_ptr<BcsMockContext> newBcsMockContext = std::make_unique<BcsMockContext>(newDevice.get());

    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(newBcsMockContext->bcsCsr.get());

    static_cast<MockMemoryManager *>(newDevice->getExecutionEnvironment()->memoryManager.get())->enable64kbpages[rootDeviceIndex] = true;
    static_cast<MockMemoryManager *>(newDevice->getExecutionEnvironment()->memoryManager.get())->localMemorySupported[rootDeviceIndex] = true;

    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);
    auto bufferForBlt = clUniquePtr(Buffer::create(newBcsMockContext.get(), CL_MEM_COPY_HOST_PTR, 2000, &hostPtr, retVal));
    EXPECT_EQ(1u, bcsCsr->blitBufferCalled);
}

using NoBcsBufferTests = ::testing::Test;
HWTEST_F(NoBcsBufferTests, givenProductWithNoFullyBlitterSupportWhenCreatingBufferWithCopyHostPtrThenDontUseBlitOperation) {
    uint32_t hostPtr = 0;
    auto rootDeviceIndex = 1u;
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = false;

    EXPECT_FALSE(HwInfoConfig::get(hwInfo.platform.eProductFamily)->isBlitterFullySupported(hwInfo));
    std::unique_ptr<MockClDevice> newDevice = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, rootDeviceIndex));
    std::unique_ptr<BcsMockContext> newBcsMockContext = std::make_unique<BcsMockContext>(newDevice.get());

    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(newBcsMockContext->bcsCsr.get());

    static_cast<MockMemoryManager *>(newDevice->getExecutionEnvironment()->memoryManager.get())->enable64kbpages[rootDeviceIndex] = true;
    static_cast<MockMemoryManager *>(newDevice->getExecutionEnvironment()->memoryManager.get())->localMemorySupported[rootDeviceIndex] = true;

    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);
    cl_int retVal = 0;
    auto bufferForBlt = clUniquePtr(Buffer::create(newBcsMockContext.get(), CL_MEM_COPY_HOST_PTR, sizeof(hostPtr), &hostPtr, retVal));
    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBcsSupportedWhenEnqueueBufferOperationIsCalledThenUseBcsCsr) {
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(0);
    auto mockCmdQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    auto bcsEngine = mockCmdQueue->bcsEngines[0];
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine->commandStreamReceiver);

    auto bufferForBlt0 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto bufferForBlt1 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    bufferForBlt0->forceDisallowCPUCopy = true;
    bufferForBlt1->forceDisallowCPUCopy = true;

    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {1, 2, 1};

    DebugManager.flags.EnableBlitterForEnqueueOperations.set(0);
    mockCmdQueue->clearBcsEngines();
    mockCmdQueue->clearBcsStates();
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);
    commandQueue->enqueueReadBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueWriteBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                         MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                         MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBufferRect(bufferForBlt0.get(), bufferForBlt1.get(), bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, 0, nullptr, nullptr);

    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    mockCmdQueue->clearBcsEngines();
    mockCmdQueue->clearBcsStates();
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);
    commandQueue->enqueueReadBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueWriteBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                         MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                         MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBufferRect(bufferForBlt0.get(), bufferForBlt1.get(), bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, 0, nullptr, nullptr);

    DebugManager.flags.EnableBlitterForEnqueueOperations.set(0);
    mockCmdQueue->bcsEngines[0] = bcsEngine;
    mockCmdQueue->clearBcsStates();
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);
    commandQueue->enqueueReadBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueWriteBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                         MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                         MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBufferRect(bufferForBlt0.get(), bufferForBlt1.get(), bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, 0, nullptr, nullptr);
    commandQueue->enqueueSVMMemcpy(CL_TRUE, bufferForBlt0.get(), bufferForBlt1.get(), 1, 0, nullptr, nullptr);

    DebugManager.flags.EnableBlitterForEnqueueOperations.set(-1);
    mockCmdQueue->bcsEngines[0] = bcsEngine;
    mockCmdQueue->clearBcsStates();
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);
    commandQueue->enqueueReadBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueWriteBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                         MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                         MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBufferRect(bufferForBlt0.get(), bufferForBlt1.get(), bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, 0, nullptr, nullptr);
    commandQueue->enqueueSVMMemcpy(CL_TRUE, bufferForBlt0.get(), bufferForBlt1.get(), 1, 0, nullptr, nullptr);

    EXPECT_EQ(7u, bcsCsr->blitBufferCalled);

    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    mockCmdQueue->bcsEngines[0] = bcsEngine;
    mockCmdQueue->clearBcsStates();
    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(8u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(9u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);
    EXPECT_EQ(10u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueReadBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    EXPECT_EQ(11u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueWriteBufferRect(bufferForBlt0.get(), CL_TRUE, bufferOrigin, hostOrigin, region,
                                         MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                         MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    EXPECT_EQ(12u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueCopyBufferRect(bufferForBlt0.get(), bufferForBlt1.get(), bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, 0, nullptr, nullptr);
    EXPECT_EQ(13u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueSVMMemcpy(CL_TRUE, bufferForBlt0.get(), bufferForBlt1.get(), 1, 0, nullptr, nullptr);
    EXPECT_EQ(14u, bcsCsr->blitBufferCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenDebugFlagSetWhenDispatchingBlitCommandsThenPrintDispatchDetails) {
    DebugManager.flags.PrintBlitDispatchDetails.set(true);

    uint32_t maxBlitWidth = static_cast<uint32_t>(BlitterConstants::maxBlitWidth);
    uint32_t copySize = maxBlitWidth + 5;

    auto myHostPtr = std::make_unique<uint8_t[]>(copySize);
    uint64_t hostPtrAddr = castToUint64(myHostPtr.get());

    auto bufferForBlt = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, copySize, nullptr, retVal));
    bufferForBlt->forceDisallowCPUCopy = true;

    uint64_t bufferGpuAddr = bufferForBlt->getGraphicsAllocation(0)->getGpuAddress();

    testing::internal::CaptureStdout();

    commandQueue->enqueueWriteBuffer(bufferForBlt.get(), CL_TRUE, 0, copySize, myHostPtr.get(), nullptr, 0, nullptr, nullptr);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(0u, output.size());

    char expectedStr[512] = {};
    snprintf(expectedStr, 512, "\nBlit dispatch with AuxTranslationDirection %u \
\nBlit command. width: %u, height: %u, srcAddr: %#" SCNx64 ", dstAddr: %#" SCNx64 " \
\nBlit command. width: %u, height: %u, srcAddr: %#" SCNx64 ", dstAddr: %#" SCNx64 " ",
             static_cast<uint32_t>(AuxTranslationDirection::None),
             maxBlitWidth, 1, hostPtrAddr, bufferGpuAddr,
             (copySize - maxBlitWidth), 1, ptrOffset(hostPtrAddr, maxBlitWidth), ptrOffset(bufferGpuAddr, maxBlitWidth));

    EXPECT_TRUE(hasSubstr(output, std::string(expectedStr)));
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBcsSupportedWhenQueueIsBlockedThenDispatchBlitWhenUnblocked) {
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);

    auto bufferForBlt0 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto bufferForBlt1 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    bufferForBlt0->forceDisallowCPUCopy = true;
    bufferForBlt1->forceDisallowCPUCopy = true;
    UserEvent userEvent(bcsMockContext.get());

    cl_event waitlist = &userEvent;
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {1, 2, 1};

    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 1, &waitlist, nullptr);
    commandQueue->enqueueReadBuffer(bufferForBlt1.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);
    commandQueue->enqueueReadBufferRect(bufferForBlt0.get(), CL_FALSE, bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueWriteBufferRect(bufferForBlt0.get(), CL_FALSE, bufferOrigin, hostOrigin, region,
                                         MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                         MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    commandQueue->enqueueCopyBufferRect(bufferForBlt0.get(), bufferForBlt1.get(), bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, 0, nullptr, nullptr);
    commandQueue->enqueueSVMMemcpy(CL_FALSE, bufferForBlt0.get(), bufferForBlt1.get(), 1, 0, nullptr, nullptr);

    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);

    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(7u, bcsCsr->blitBufferCalled);

    commandQueue->enqueueWriteBuffer(bufferForBlt0.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(8u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueReadBuffer(bufferForBlt0.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(9u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 1, 1, 0, nullptr, nullptr);
    EXPECT_EQ(10u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueReadBufferRect(bufferForBlt0.get(), CL_FALSE, bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    EXPECT_EQ(11u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueWriteBufferRect(bufferForBlt0.get(), CL_FALSE, bufferOrigin, hostOrigin, region,
                                         MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                         MemoryConstants::cacheLineSize, &hostPtr, 0, nullptr, nullptr);
    EXPECT_EQ(12u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueCopyBufferRect(bufferForBlt0.get(), bufferForBlt1.get(), bufferOrigin, hostOrigin, region,
                                        MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                        MemoryConstants::cacheLineSize, 0, nullptr, nullptr);
    EXPECT_EQ(13u, bcsCsr->blitBufferCalled);
    commandQueue->enqueueSVMMemcpy(CL_FALSE, bufferForBlt0.get(), bufferForBlt1.get(), 1, 0, nullptr, nullptr);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBuffersWhenCopyBufferCalledThenUseBcs) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto bufferForBlt0 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto bufferForBlt1 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    bufferForBlt0->forceDisallowCPUCopy = true;
    bufferForBlt1->forceDisallowCPUCopy = true;

    cmdQ->enqueueCopyBuffer(bufferForBlt0.get(), bufferForBlt1.get(), 0, 0, 1, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(bcsCsr->getCS(0));
    auto commandItor = find<XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), commandItor);
    auto copyBltCmd = genCmdCast<XY_COPY_BLT *>(*commandItor);

    EXPECT_EQ(bufferForBlt0->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress(), copyBltCmd->getSourceBaseAddress());
    EXPECT_EQ(bufferForBlt1->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress(), copyBltCmd->getDestinationBaseAddress());
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBuffersWhenCopyBufferRectCalledThenUseBcs) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto bufferForBlt0 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto bufferForBlt1 = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    bufferForBlt0->forceDisallowCPUCopy = true;
    bufferForBlt1->forceDisallowCPUCopy = true;

    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {1, 2, 1};

    cmdQ->enqueueCopyBufferRect(bufferForBlt0.get(), bufferForBlt1.get(), bufferOrigin, hostOrigin, region,
                                0, 0, 0, 0, 0, nullptr, nullptr);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(bcsCsr->getCS(0));
    auto commandItor = find<XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), commandItor);
    auto copyBltCmd = genCmdCast<XY_COPY_BLT *>(*commandItor);

    EXPECT_EQ(bufferForBlt0->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress(), copyBltCmd->getSourceBaseAddress());
    EXPECT_EQ(bufferForBlt1->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress(), copyBltCmd->getDestinationBaseAddress());
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenDstHostPtrWhenEnqueueSVMMemcpyThenEnqueuReadBufferIsCalledAndBcs) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto pDstSVM = std::make_unique<char[]>(1);
    auto pSrcSVM = bcsMockContext->getSVMAllocsManager()->createSVMAlloc(1, {}, bcsMockContext->getRootDeviceIndices(), bcsMockContext->getDeviceBitfields());

    cmdQ->enqueueSVMMemcpy(true, pDstSVM.get(), pSrcSVM, 1, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(bcsCsr->getCS(0));
    auto commandItor = find<XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), commandItor);
    auto copyBltCmd = genCmdCast<XY_COPY_BLT *>(*commandItor);

    EXPECT_EQ(pSrcSVM, reinterpret_cast<void *>(copyBltCmd->getSourceBaseAddress()));
    EXPECT_EQ(pDstSVM.get(), reinterpret_cast<void *>(copyBltCmd->getDestinationBaseAddress()));

    bcsMockContext->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenSrcHostPtrWhenEnqueueSVMMemcpyThenEnqueuWriteBufferIsCalledAndBcs) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto pSrcSVM = std::make_unique<char[]>(1);
    auto pDstSVM = bcsMockContext->getSVMAllocsManager()->createSVMAlloc(1, {}, bcsMockContext->getRootDeviceIndices(), bcsMockContext->getDeviceBitfields());

    cmdQ->enqueueSVMMemcpy(true, pDstSVM, pSrcSVM.get(), 1, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(bcsCsr->getCS(0));
    auto commandItor = find<XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), commandItor);
    auto copyBltCmd = genCmdCast<XY_COPY_BLT *>(*commandItor);

    EXPECT_EQ(pSrcSVM.get(), reinterpret_cast<void *>(copyBltCmd->getSourceBaseAddress()));
    EXPECT_EQ(pDstSVM, reinterpret_cast<void *>(copyBltCmd->getDestinationBaseAddress()));

    bcsMockContext->getSVMAllocsManager()->freeSVMAlloc(pDstSVM);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockedBlitEnqueueWhenUnblockingThenMakeResidentAllTimestampPackets) {
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);
    bcsCsr->storeMakeResidentAllocations = true;

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());

    auto bufferForBlt = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    bufferForBlt->forceDisallowCPUCopy = true;

    TimestampPacketContainer previousTimestampPackets;
    mockCmdQ->obtainNewTimestampPacketNodes(1, previousTimestampPackets, false, *bcsCsr);
    auto dependencyFromPreviousEnqueue = mockCmdQ->timestampPacketContainer->peekNodes()[0];

    auto event = makeReleaseable<Event>(mockCmdQ, CL_COMMAND_READ_BUFFER, 0, 0);
    MockTimestampPacketContainer eventDependencyContainer(*bcsCsr->getTimestampPacketAllocator(), 1);
    auto eventDependency = eventDependencyContainer.getNode(0);
    event->addTimestampPacketNodes(eventDependencyContainer);

    auto userEvent = makeReleaseable<UserEvent>(bcsMockContext.get());
    cl_event waitlist[] = {userEvent.get(), event.get()};

    commandQueue->enqueueReadBuffer(bufferForBlt.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 2, waitlist, nullptr);

    auto outputDependency = mockCmdQ->timestampPacketContainer->peekNodes()[0];
    EXPECT_NE(outputDependency, dependencyFromPreviousEnqueue);

    EXPECT_FALSE(bcsCsr->isMadeResident(dependencyFromPreviousEnqueue->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()));
    EXPECT_FALSE(bcsCsr->isMadeResident(outputDependency->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()));
    EXPECT_FALSE(bcsCsr->isMadeResident(eventDependency->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()));

    userEvent->setStatus(CL_COMPLETE);

    EXPECT_TRUE(bcsCsr->isMadeResident(dependencyFromPreviousEnqueue->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), bcsCsr->taskCount));
    EXPECT_TRUE(bcsCsr->isMadeResident(outputDependency->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), bcsCsr->taskCount));
    EXPECT_TRUE(bcsCsr->isMadeResident(eventDependency->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), bcsCsr->taskCount));
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenMapAllocationWhenEnqueueingReadOrWriteBufferThenStoreMapAllocationInDispatchParameters) {
    DebugManager.flags.DisableZeroCopyForBuffers.set(true);
    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    uint8_t hostPtr[64] = {};

    auto bufferForBlt = clUniquePtr(Buffer::create(bcsMockContext.get(), CL_MEM_USE_HOST_PTR, 1, hostPtr, retVal));
    bufferForBlt->forceDisallowCPUCopy = true;
    auto mapAllocation = bufferForBlt->getMapAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, mapAllocation);

    mockCmdQ->kernelParams.transferAllocation = nullptr;
    auto mapPtr = clEnqueueMapBuffer(mockCmdQ, bufferForBlt.get(), true, 0, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mapAllocation, mockCmdQ->kernelParams.transferAllocation);

    mockCmdQ->kernelParams.transferAllocation = nullptr;
    retVal = clEnqueueUnmapMemObject(mockCmdQ, bufferForBlt.get(), mapPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(mapAllocation, mockCmdQ->kernelParams.transferAllocation);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenWriteBufferEnqueueWithGpgpuSubmissionWhenProgrammingCommandStreamThenDoNotAddSemaphoreWaitOnGpgpu) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto queueCsr = &cmdQ->getGpgpuCommandStreamReceiver();
    auto initialTaskCount = queueCsr->peekTaskCount();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ->peekCommandStream());

    uint32_t semaphoresCount = 0;
    for (auto &cmd : hwParser.cmdList) {
        if (auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(cmd)) {
            if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*semaphoreCmd)) {
                continue;
            }
            semaphoresCount++;
        }
    }
    EXPECT_EQ(0u, semaphoresCount);
    EXPECT_EQ(initialTaskCount + 1, queueCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenReadBufferEnqueueWithGpgpuSubmissionWhenProgrammingCommandStreamThenDoNotAddSemaphoreWaitOnGpgpu) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto queueCsr = &cmdQ->getGpgpuCommandStreamReceiver();
    auto initialTaskCount = queueCsr->peekTaskCount();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ->peekCommandStream());

    uint32_t semaphoresCount = 0;
    for (auto &cmd : hwParser.cmdList) {
        if (auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(cmd)) {
            if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*semaphoreCmd)) {
                continue;
            }
            semaphoresCount++;
        }
    }
    EXPECT_EQ(0u, semaphoresCount);
    EXPECT_EQ(initialTaskCount + 1, queueCsr->peekTaskCount());
}

template <typename FamilyType>
void BcsBufferTests::waitForCacheFlushFromBcsTest(MockCommandQueueHw<FamilyType> &commandQueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    bool isCacheFlushForBcsRequired = commandQueue.isCacheFlushForBcsRequired();

    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    commandQueue.enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParserGpGpu;
    HardwareParse hwParserBcs;
    hwParserGpGpu.parseCommands<FamilyType>(*commandQueue.peekCommandStream());
    hwParserBcs.parseCommands<FamilyType>(bcsCsr->commandStream);

    auto gpgpuPipeControls = findAll<PIPE_CONTROL *>(hwParserGpGpu.cmdList.begin(), hwParserGpGpu.cmdList.end());
    uint64_t cacheFlushWriteAddress = 0;

    for (auto &pipeControl : gpgpuPipeControls) {
        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControl);
        cacheFlushWriteAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
        if (cacheFlushWriteAddress != 0) {
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControlCmd->getDcFlushEnable());
            EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
            EXPECT_EQ(isCacheFlushForBcsRequired, 0u == pipeControlCmd->getImmediateData());
            break;
        }
    }

    auto bcsSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserBcs.cmdList.begin(), hwParserBcs.cmdList.end());
    size_t additionalSemaphores = UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo()) ? 2 : 0;

    if (isCacheFlushForBcsRequired) {
        EXPECT_NE(0u, cacheFlushWriteAddress);
        EXPECT_EQ(1u + additionalSemaphores, bcsSemaphores.size());

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*bcsSemaphores[0]);
        EXPECT_EQ(cacheFlushWriteAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    } else {
        EXPECT_EQ(additionalSemaphores, bcsSemaphores.size());
    }
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenCommandQueueWithCacheFlushRequirementWhenProgrammingCmdBufferThenWaitForCacheFlushFromBcs) {
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    cmdQ->overrideIsCacheFlushForBcsRequired.returnValue = true;
    waitForCacheFlushFromBcsTest<FamilyType>(*cmdQ);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenCommandQueueWithoutCacheFlushRequirementWhenProgrammingCmdBufferThenWaitForCacheFlushFromBcs) {
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    cmdQ->overrideIsCacheFlushForBcsRequired.returnValue = false;
    waitForCacheFlushFromBcsTest<FamilyType>(*cmdQ);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenPipeControlRequestWhenDispatchingBlitEnqueueThenWaitPipeControlOnBcsEngine) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);

    auto queueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&cmdQ->getGpgpuCommandStreamReceiver());
    queueCsr->stallingCommandsOnNextFlushRequired = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(queueCsr->commandStream);

    uint64_t pipeControlWriteAddress = 0;
    for (auto &cmd : hwParser.cmdList) {
        if (auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(cmd)) {
            if (pipeControlCmd->getPostSyncOperation() != PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                continue;
            }

            EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
            pipeControlWriteAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
            break;
        }
    }

    EXPECT_NE(0u, pipeControlWriteAddress);

    HardwareParse bcsHwParser;
    bcsHwParser.parseCommands<FamilyType>(bcsCsr->commandStream);
    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(bcsHwParser.cmdList.begin(), bcsHwParser.cmdList.end());

    if (cmdQ->isCacheFlushForBcsRequired()) {
        EXPECT_EQ(UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo()) ? 4u : 2u, semaphores.size());
        EXPECT_EQ(pipeControlWriteAddress, genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]))->getSemaphoreGraphicsAddress());
    } else {
        EXPECT_EQ(UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo()) ? 3u : 1u, semaphores.size());
        EXPECT_EQ(pipeControlWriteAddress, genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]))->getSemaphoreGraphicsAddress());
    }
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBarrierWhenReleasingMultipleBlockedEnqueuesThenProgramBarrierOnce) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    UserEvent userEvent0, userEvent1;
    cl_event waitlist0[] = {&userEvent0};
    cl_event waitlist1[] = {&userEvent1};

    cmdQ->enqueueBarrierWithWaitList(0, nullptr, nullptr);
    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 1, waitlist0, nullptr);
    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 1, waitlist1, nullptr);

    auto pipeControlLookup = [](LinearStream &stream, size_t offset) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(stream, offset);

        bool stallingPipeControlFound = false;
        for (auto &cmd : hwParser.cmdList) {
            if (auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(cmd)) {
                if (pipeControlCmd->getPostSyncOperation() != PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                    continue;
                }

                stallingPipeControlFound = true;
                EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
                EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControlCmd->getDcFlushEnable());
                break;
            }
        }

        return stallingPipeControlFound;
    };

    auto &csrStream = cmdQ->getGpgpuCommandStreamReceiver().getCS(0);
    EXPECT_TRUE(cmdQ->getGpgpuCommandStreamReceiver().isStallingCommandsOnNextFlushRequired());
    userEvent0.setStatus(CL_COMPLETE);
    EXPECT_FALSE(cmdQ->getGpgpuCommandStreamReceiver().isStallingCommandsOnNextFlushRequired());
    EXPECT_TRUE(pipeControlLookup(csrStream, 0));

    auto csrOffset = csrStream.getUsed();
    userEvent1.setStatus(CL_COMPLETE);
    EXPECT_FALSE(pipeControlLookup(csrStream, csrOffset));
    cmdQ->isQueueBlocked();
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenPipeControlRequestWhenDispatchingBlockedBlitEnqueueThenWaitPipeControlOnBcsEngine) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);

    auto queueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&cmdQ->getGpgpuCommandStreamReceiver());
    queueCsr->stallingCommandsOnNextFlushRequired = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    UserEvent userEvent;
    cl_event waitlist = &userEvent;
    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 1, &waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    HardwareParse bcsHwParser;
    bcsHwParser.parseCommands<FamilyType>(bcsCsr->commandStream);

    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(bcsHwParser.cmdList.begin(), bcsHwParser.cmdList.end());

    if (cmdQ->isCacheFlushForBcsRequired()) {
        EXPECT_EQ(UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo()) ? 4u : 2u, semaphores.size());
    } else {
        EXPECT_EQ(UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo()) ? 3u : 1u, semaphores.size());
    }

    cmdQ->isQueueBlocked();
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBufferOperationWithoutKernelWhenEstimatingCommandsSizeThenReturnCorrectValue) {
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    CsrDependencies csrDependencies;
    MultiDispatchInfo multiDispatchInfo;

    auto &hwInfo = cmdQ->getDevice().getHardwareInfo();

    auto readBufferCmdsSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_READ_BUFFER, csrDependencies, false, false,
                                                                                   true, *cmdQ, multiDispatchInfo, false, false);
    auto writeBufferCmdsSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_WRITE_BUFFER, csrDependencies, false, false,
                                                                                    true, *cmdQ, multiDispatchInfo, false, false);
    auto copyBufferCmdsSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_COPY_BUFFER, csrDependencies, false, false,
                                                                                   true, *cmdQ, multiDispatchInfo, false, false);
    auto expectedSize = TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<FamilyType>();

    if (cmdQ->isCacheFlushForBcsRequired()) {
        expectedSize += MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(hwInfo);
    }

    EXPECT_EQ(expectedSize, readBufferCmdsSize);
    EXPECT_EQ(expectedSize, writeBufferCmdsSize);
    EXPECT_EQ(expectedSize, copyBufferCmdsSize);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenOutputTimestampPacketWhenBlitCalledThenProgramMiFlushDwWithDataWrite) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cl_int retVal = CL_SUCCESS;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    auto outputTimestampPacket = cmdQ->timestampPacketContainer->peekNodes()[0];
    auto timestampPacketGpuWriteAddress = TimestampPacketHelper::getContextEndGpuAddress(*outputTimestampPacket);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    uint32_t miFlushDwCmdsWithOutputCount = 0;
    bool blitCmdFound = false;
    for (auto &cmd : hwParser.cmdList) {
        if (auto miFlushDwCmd = genCmdCast<MI_FLUSH_DW *>(cmd)) {
            if (miFlushDwCmd->getDestinationAddress() == 0) {
                continue;
            }

            EXPECT_EQ(miFlushDwCmdsWithOutputCount == 0,
                      timestampPacketGpuWriteAddress == miFlushDwCmd->getDestinationAddress());
            EXPECT_EQ(miFlushDwCmdsWithOutputCount == 0,
                      0u == miFlushDwCmd->getImmediateData());

            miFlushDwCmdsWithOutputCount++;
        } else if (genCmdCast<typename FamilyType::XY_COPY_BLT *>(cmd)) {
            blitCmdFound = true;
            EXPECT_EQ(0u, miFlushDwCmdsWithOutputCount);
        }
    }

    EXPECT_EQ(2u, miFlushDwCmdsWithOutputCount); // TimestampPacket + taskCount

    EXPECT_TRUE(blitCmdFound);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenInputAndOutputTimestampPacketWhenBlitCalledThenMakeThemResident) {
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cl_int retVal = CL_SUCCESS;

    auto memoryManager = bcsCsr->getMemoryManager();
    bcsCsr->timestampPacketAllocator = std::make_unique<MockTagAllocator<TimestampPackets<uint32_t>>>(device->getRootDeviceIndex(), memoryManager, 1,
                                                                                                      MemoryConstants::cacheLineSize,
                                                                                                      sizeof(TimestampPackets<uint32_t>),
                                                                                                      false, device->getDeviceBitfield());

    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    // first enqueue to create IOQ dependency
    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    auto inputTimestampPacketAllocation = cmdQ->timestampPacketContainer->peekNodes().at(0)->getBaseGraphicsAllocation();

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    auto outputTimestampPacketAllocation = cmdQ->timestampPacketContainer->peekNodes().at(0)->getBaseGraphicsAllocation();

    EXPECT_NE(outputTimestampPacketAllocation, inputTimestampPacketAllocation);

    EXPECT_EQ(cmdQ->taskCount, inputTimestampPacketAllocation->getDefaultGraphicsAllocation()->getTaskCount(bcsCsr->getOsContext().getContextId()));
    EXPECT_EQ(cmdQ->taskCount, outputTimestampPacketAllocation->getDefaultGraphicsAllocation()->getTaskCount(bcsCsr->getOsContext().getContextId()));
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockingWriteBufferWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->clearBcsEngines();
    cmdQ->bcsEngines[0] = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    bool tempAllocationFound = false;
    auto tempAllocation = myMockCsr->getTemporaryAllocations().peekHead();
    while (tempAllocation) {
        if (tempAllocation->getUnderlyingBuffer() == hostPtr) {
            tempAllocationFound = true;
            break;
        }
        tempAllocation = tempAllocation->next;
    }
    EXPECT_TRUE(tempAllocationFound);

    cmdQ->enqueueWriteBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockingReadBufferRectWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->clearBcsEngines();
    cmdQ->bcsEngines[0] = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {1, 2, 1};

    cmdQ->enqueueReadBufferRect(buffer.get(), false, bufferOrigin, hostOrigin, region,
                                MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                MemoryConstants::cacheLineSize, hostPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    bool tempAllocationFound = false;
    auto tempAllocation = myMockCsr->getTemporaryAllocations().peekHead();
    while (tempAllocation) {
        if (tempAllocation->getUnderlyingBuffer() == hostPtr) {
            tempAllocationFound = true;
            break;
        }
        tempAllocation = tempAllocation->next;
    }
    EXPECT_TRUE(tempAllocationFound);

    cmdQ->enqueueReadBufferRect(buffer.get(), true, bufferOrigin, hostOrigin, region,
                                MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                MemoryConstants::cacheLineSize, hostPtr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockingWriteBufferRectWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->clearBcsEngines();
    cmdQ->bcsEngines[0] = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {1, 2, 1};

    cmdQ->enqueueWriteBufferRect(buffer.get(), false, bufferOrigin, hostOrigin, region,
                                 MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                 MemoryConstants::cacheLineSize, hostPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    bool tempAllocationFound = false;
    auto tempAllocation = myMockCsr->getTemporaryAllocations().peekHead();
    while (tempAllocation) {
        if (tempAllocation->getUnderlyingBuffer() == hostPtr) {
            tempAllocationFound = true;
            break;
        }
        tempAllocation = tempAllocation->next;
    }
    EXPECT_TRUE(tempAllocationFound);

    cmdQ->enqueueWriteBufferRect(buffer.get(), true, bufferOrigin, hostOrigin, region,
                                 MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize,
                                 MemoryConstants::cacheLineSize, hostPtr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockingReadBufferWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->clearBcsEngines();
    cmdQ->bcsEngines[0] = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    cmdQ->enqueueReadBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    bool tempAllocationFound = false;
    auto tempAllocation = myMockCsr->getTemporaryAllocations().peekHead();
    while (tempAllocation) {
        if (tempAllocation->getUnderlyingBuffer() == hostPtr) {
            tempAllocationFound = true;
            break;
        }
        tempAllocation = tempAllocation->next;
    }
    EXPECT_TRUE(tempAllocationFound);

    cmdQ->enqueueReadBuffer(buffer.get(), true, 0, 1, hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockingSVMMemcpyAndEnqueuReadBufferIsCalledWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->clearBcsEngines();
    cmdQ->bcsEngines[0] = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    auto pDstSVM = std::make_unique<char[]>(256);
    auto pSrcSVM = bcsMockContext->getSVMAllocsManager()->createSVMAlloc(256, {}, bcsMockContext->getRootDeviceIndices(), bcsMockContext->getDeviceBitfields());

    cmdQ->enqueueSVMMemcpy(false, pDstSVM.get(), pSrcSVM, 256, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    auto tempAlloc = myMockCsr->getTemporaryAllocations().peekHead();

    EXPECT_EQ(0u, tempAlloc->countSuccessors());
    EXPECT_EQ(pDstSVM.get(), reinterpret_cast<void *>(tempAlloc->getGpuAddress()));

    cmdQ->enqueueSVMMemcpy(true, pDstSVM.get(), pSrcSVM, 256, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);

    bcsMockContext->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenSrcHostPtrBlockingEnqueueSVMMemcpyAndEnqueuWriteBufferIsCalledWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->clearBcsEngines();
    cmdQ->bcsEngines[0] = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    auto pSrcSVM = std::make_unique<char[]>(256);
    auto pDstSVM = bcsMockContext->getSVMAllocsManager()->createSVMAlloc(256, {}, bcsMockContext->getRootDeviceIndices(), bcsMockContext->getDeviceBitfields());

    cmdQ->enqueueSVMMemcpy(false, pDstSVM, pSrcSVM.get(), 256, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    auto tempAlloc = myMockCsr->getTemporaryAllocations().peekHead();

    EXPECT_EQ(0u, tempAlloc->countSuccessors());
    EXPECT_EQ(pSrcSVM.get(), reinterpret_cast<void *>(tempAlloc->getGpuAddress()));

    cmdQ->enqueueSVMMemcpy(true, pDstSVM, pSrcSVM.get(), 256, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);

    bcsMockContext->getSVMAllocsManager()->freeSVMAlloc(pDstSVM);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenDstHostPtrAndSrcHostPtrBlockingEnqueueSVMMemcpyAndEnqueuWriteBufferIsCalledWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->clearBcsEngines();
    cmdQ->bcsEngines[0] = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    auto pSrcSVM = std::make_unique<char[]>(256);
    auto pDstSVM = std::make_unique<char[]>(256);

    cmdQ->enqueueSVMMemcpy(false, pDstSVM.get(), pSrcSVM.get(), 256, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    auto tempAlloc = myMockCsr->getTemporaryAllocations().peekHead();

    EXPECT_EQ(1u, tempAlloc->countSuccessors());
    EXPECT_EQ(pSrcSVM.get(), reinterpret_cast<void *>(tempAlloc->getGpuAddress()));
    EXPECT_EQ(pDstSVM.get(), reinterpret_cast<void *>(tempAlloc->next->getGpuAddress()));

    cmdQ->enqueueSVMMemcpy(true, pDstSVM.get(), pSrcSVM.get(), 256, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenSvmToSvmCopyWhenEnqueueSVMMemcpyThenSvmMemcpyCommandIsCalledAndBcs) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));

    auto pDstSVM = bcsMockContext->getSVMAllocsManager()->createSVMAlloc(256, {}, bcsMockContext->getRootDeviceIndices(), bcsMockContext->getDeviceBitfields());
    auto pSrcSVM = bcsMockContext->getSVMAllocsManager()->createSVMAlloc(256, {}, bcsMockContext->getRootDeviceIndices(), bcsMockContext->getDeviceBitfields());

    cmdQ->enqueueSVMMemcpy(false, pDstSVM, pSrcSVM, 256, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(bcsCsr->getCS(0));
    auto commandItor = find<XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), commandItor);
    auto copyBltCmd = genCmdCast<XY_COPY_BLT *>(*commandItor);

    EXPECT_EQ(pSrcSVM, reinterpret_cast<void *>(copyBltCmd->getSourceBaseAddress()));
    EXPECT_EQ(pDstSVM, reinterpret_cast<void *>(copyBltCmd->getDestinationBaseAddress()));

    bcsMockContext->getSVMAllocsManager()->freeSVMAlloc(pDstSVM);
    bcsMockContext->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenSvmToSvmCopyTypeWhenEnqueueNonBlockingSVMMemcpyThenSvmMemcpyCommandIsEnqueuedWhenUsingBcsThenCallWait) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->clearBcsEngines();
    cmdQ->bcsEngines[0] = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    auto pDstSVM = bcsMockContext->getSVMAllocsManager()->createSVMAlloc(256, {}, bcsMockContext->getRootDeviceIndices(), bcsMockContext->getDeviceBitfields());
    auto pSrcSVM = bcsMockContext->getSVMAllocsManager()->createSVMAlloc(256, {}, bcsMockContext->getRootDeviceIndices(), bcsMockContext->getDeviceBitfields());

    cmdQ->enqueueSVMMemcpy(false, pDstSVM, pSrcSVM, 256, 0, nullptr, nullptr);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
    EXPECT_TRUE(gpgpuCsr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(myMockCsr->getTemporaryAllocations().peekIsEmpty());

    cmdQ->enqueueSVMMemcpy(true, pDstSVM, pSrcSVM, 256, 0, nullptr, nullptr);
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);

    bcsMockContext->getSVMAllocsManager()->freeSVMAlloc(pDstSVM);
    bcsMockContext->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

struct BcsSvmTests : public BcsBufferTests {

    template <typename FamilyType>
    void setUpT() {
        if (is32bit) {
            GTEST_SKIP();
        }
        REQUIRE_SVM_OR_SKIP(defaultHwInfo);
        BcsBufferTests::setUpT<FamilyType>();
        if (IsSkipped()) {
            GTEST_SKIP();
        }

        deviceMemAlloc = clDeviceMemAllocINTEL(bcsMockContext.get(), device.get(), nullptr, allocSize, 0u, &retVal);
        ASSERT_NE(nullptr, deviceMemAlloc);
        ASSERT_EQ(CL_SUCCESS, retVal);
        allocation.push_back(deviceMemAlloc);

        retVal = CL_SUCCESS;
        hostMemAlloc = clHostMemAllocINTEL(bcsMockContext.get(), nullptr, allocSize, 0u, &retVal);
        ASSERT_NE(nullptr, hostMemAlloc);
        ASSERT_EQ(CL_SUCCESS, retVal);
        allocation.push_back(hostMemAlloc);

        sharedMemAlloc = clSharedMemAllocINTEL(bcsMockContext.get(), device.get(), nullptr, allocSize, 0u, &retVal);
        ASSERT_NE(nullptr, sharedMemAlloc);
        ASSERT_EQ(CL_SUCCESS, retVal);
        allocation.push_back(sharedMemAlloc);
    }

    template <typename FamilyType>
    void tearDownT() {
        if (IsSkipped()) {
            return;
        }
        BcsBufferTests::tearDownT<FamilyType>();

        clMemFreeINTEL(bcsMockContext.get(), sharedMemAlloc);
        clMemFreeINTEL(bcsMockContext.get(), hostMemAlloc);
        clMemFreeINTEL(bcsMockContext.get(), deviceMemAlloc);
    }

    size_t allocSize = 4096u;
    std::vector<size_t> offset{0, 1, 2, 4, 8, 16, 32};
    std::vector<void *> allocation;

    void *hostMemAlloc = nullptr;
    void *deviceMemAlloc = nullptr;
    void *sharedMemAlloc = nullptr;

    cl_int retVal = CL_SUCCESS;
};

HWTEST_TEMPLATED_F(BcsSvmTests, givenSVMMAllocationWithOffsetWhenUsingBcsThenProperValuesAreSet) {
    DebugManager.flags.EnableBlitterOperationsSupport.set(1);

    for (auto srcPtr : allocation) {
        for (auto dstPtr : allocation) {
            for (auto srcOff : offset) {
                for (auto dstOff : offset) {
                    auto pSrcPtr = srcPtr;
                    auto pDstPtr = dstPtr;
                    auto srcOffset = srcOff;
                    auto dstOffset = dstOff;

                    pSrcPtr = ptrOffset(pSrcPtr, srcOffset);
                    pDstPtr = ptrOffset(pDstPtr, dstOffset);

                    auto dstSvmData = bcsMockContext->getSVMAllocsManager()->getSVMAlloc(pDstPtr);
                    auto srcSvmData = bcsMockContext->getSVMAllocsManager()->getSVMAlloc(pSrcPtr);

                    auto srcGpuAllocation = srcSvmData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
                    auto dstGpuAllocation = dstSvmData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

                    BuiltinOpParams builtinOpParams = {};
                    builtinOpParams.size = {allocSize, 0, 0};
                    builtinOpParams.srcPtr = const_cast<void *>(alignDown(pSrcPtr, 4));
                    builtinOpParams.srcSvmAlloc = srcGpuAllocation;
                    builtinOpParams.srcOffset = {ptrDiff(pSrcPtr, builtinOpParams.srcPtr), 0, 0};
                    builtinOpParams.dstPtr = alignDown(pDstPtr, 4);
                    builtinOpParams.dstSvmAlloc = dstGpuAllocation;
                    builtinOpParams.dstOffset = {ptrDiff(pDstPtr, builtinOpParams.dstPtr), 0, 0};

                    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);

                    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::BufferToBuffer,
                                                                                *bcsCsr, builtinOpParams);

                    EXPECT_EQ(srcOffset, blitProperties.srcOffset.x);
                    EXPECT_EQ(dstOffset, blitProperties.dstOffset.x);
                    EXPECT_EQ(dstGpuAllocation, blitProperties.dstAllocation);
                    EXPECT_EQ(srcGpuAllocation, blitProperties.srcAllocation);
                    EXPECT_EQ(dstGpuAllocation->getGpuAddress(), blitProperties.dstGpuAddress);
                    EXPECT_EQ(srcGpuAllocation->getGpuAddress(), blitProperties.srcGpuAddress);
                    EXPECT_EQ(pDstPtr, reinterpret_cast<void *>(blitProperties.dstGpuAddress + blitProperties.dstOffset.x));
                    EXPECT_EQ(pSrcPtr, reinterpret_cast<void *>(blitProperties.srcGpuAddress + blitProperties.srcOffset.x));
                }
            }
        }
    }
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBlockedEnqueueWhenUsingBcsThenWaitForValidTaskCountOnBlockingCall) {
    auto myMockCsr = new MyMockCsr<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    myMockCsr->taskCount = 1234;
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(*bcsMockContext->bcsOsContext);
    bcsMockContext->bcsCsr.reset(myMockCsr);

    EngineControl bcsEngineControl = {myMockCsr, bcsMockContext->bcsOsContext.get()};

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr));
    cmdQ->clearBcsEngines();
    cmdQ->bcsEngines[0] = &bcsEngineControl;
    auto &gpgpuCsr = cmdQ->getGpgpuCommandStreamReceiver();
    myMockCsr->gpgpuCsr = &gpgpuCsr;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    UserEvent userEvent;
    cl_event waitlist = &userEvent;

    cmdQ->enqueueWriteBuffer(buffer.get(), false, 0, 1, hostPtr, nullptr, 1, &waitlist, nullptr);

    userEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(0u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);

    cmdQ->finish();
    EXPECT_EQ(1u, myMockCsr->waitForTaskCountAndCleanAllocationListCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenDebugFlagSetToOneWhenEnqueueingCopyLocalBufferToLocalBufferThenUseBlitter) {
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    const bool preferBlitterHw = ClHwHelper::get(::defaultHwInfo->platform.eRenderCoreFamily).preferBlitterForLocalToLocalTransfers();
    uint32_t expectedBlitBufferCalled = 0;

    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_EQ(expectedBlitBufferCalled, bcsCsr->blitBufferCalled);
    commandQueue->enqueueCopyBuffer(&srcMemObj, &dstMemObj, 0, 1, 1, 0, nullptr, nullptr);
    if (preferBlitterHw) {
        expectedBlitBufferCalled++;
    }
    EXPECT_EQ(expectedBlitBufferCalled, bcsCsr->blitBufferCalled);

    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_EQ(expectedBlitBufferCalled, bcsCsr->blitBufferCalled);
    commandQueue->enqueueCopyBuffer(&srcMemObj, &dstMemObj, 0, 1, 1, 0, nullptr, nullptr);
    EXPECT_EQ(expectedBlitBufferCalled, bcsCsr->blitBufferCalled);

    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_EQ(expectedBlitBufferCalled, bcsCsr->blitBufferCalled);
    commandQueue->enqueueCopyBuffer(&srcMemObj, &dstMemObj, 0, 1, 1, 0, nullptr, nullptr);
    expectedBlitBufferCalled++;
    EXPECT_EQ(expectedBlitBufferCalled, bcsCsr->blitBufferCalled);
}

HWTEST_TEMPLATED_F(BcsBufferTests, givenBcsQueueWhenEnqueueingCopyBufferToBufferThenUseBlitterRegardlessOfPreference) {
    REQUIRE_BLITTER_OR_SKIP(&device->getDevice().getHardwareInfo());

    cl_command_queue_properties properties[] = {
        CL_QUEUE_FAMILY_INTEL,
        device->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::Copy),
        CL_QUEUE_INDEX_INTEL,
        0,
        0,
    };
    MockCommandQueueHw<FamilyType> queue(bcsMockContext.get(), device.get(), properties);
    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(this->bcsCsr);
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;

    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);
    queue.enqueueCopyBuffer(&srcMemObj, &dstMemObj, 0, 1, 1, 0, nullptr, nullptr);
    EXPECT_EQ(1u, bcsCsr->blitBufferCalled);

    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_EQ(1u, bcsCsr->blitBufferCalled);
    queue.enqueueCopyBuffer(&srcMemObj, &dstMemObj, 0, 1, 1, 0, nullptr, nullptr);
    EXPECT_EQ(2u, bcsCsr->blitBufferCalled);

    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_EQ(2u, bcsCsr->blitBufferCalled);
    queue.enqueueCopyBuffer(&srcMemObj, &dstMemObj, 0, 1, 1, 0, nullptr, nullptr);
    EXPECT_EQ(3u, bcsCsr->blitBufferCalled);
}
