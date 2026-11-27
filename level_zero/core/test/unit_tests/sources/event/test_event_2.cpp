/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_allocations_list.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/mock_timestamp_packet.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/event/event_host_synchronize_wait.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture_extended.h"
#include "level_zero/core/test/unit_tests/fixtures/event_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"
#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
extern std::atomic<uint32_t> yieldCounter;
extern volatile TagAddressType *pauseAddress;
extern TaskCountType pauseValue;
extern uint32_t pauseOffset;
extern std::function<void()> setupPauseAddress;
extern std::atomic_uint32_t tpauseCounter;
extern std::function<void()> controlTpause;
} // namespace CpuIntrinsicsTests

namespace L0 {
namespace ult {

using EventSynchronizeTest = Test<EventFixture<1, 0>>;
using EventTests = Test<EventFixture<1, 0>>;

struct EventSizeFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        hDevice = device->toHandle();
    }

    void tearDown() {
        eventObj0.reset(nullptr);
        eventObj1.reset(nullptr);
        eventPool.reset(nullptr);
        DeviceFixture::tearDown();
    }

    void createEvents() {
        ze_event_handle_t hEvent0 = 0;
        ze_event_handle_t hEvent1 = 0;
        ze_event_desc_t eventDesc0 = {};
        ze_event_desc_t eventDesc1 = {};

        eventDesc0.index = 0;
        eventDesc1.index = 1;

        auto result = eventPool->createEvent(&eventDesc0, &hEvent0);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        result = eventPool->createEvent(&eventDesc1, &hEvent1);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        eventObj0.reset(L0::Event::fromHandle(hEvent0));
        eventObj1.reset(L0::Event::fromHandle(hEvent1));
    }

    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        4};

    DebugManagerStateRestore restore;
    ze_device_handle_t hDevice = 0;
    std::unique_ptr<L0::EventPool> eventPool;
    std::unique_ptr<L0::Event> eventObj0;
    std::unique_ptr<L0::Event> eventObj1;
};

using EventSizeTests = Test<EventSizeFixture>;

HWTEST_F(EventSizeTests, whenCreatingEventPoolThenUseCorrectSizeAndAlignment) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool.reset(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &gfxCoreHelper = device->getGfxCoreHelper();

    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    uint32_t packetCount = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());

    auto expectedAlignment = static_cast<uint32_t>(gfxCoreHelper.getTimestampPacketAllocatorAlignment());
    auto singlePacketSize = TimestampPackets<typename FamilyType::TimestampPacketType, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();
    auto expectedSize = static_cast<uint32_t>(alignUp(packetCount * singlePacketSize, expectedAlignment));

    EXPECT_EQ(expectedSize, eventPool->getEventSize());

    createEvents();

    constexpr size_t timestampPacketTypeSize = sizeof(typename FamilyType::TimestampPacketType);

    EXPECT_EQ(timestampPacketTypeSize / 4, eventObj0->getTimestampSizeInDw());
    EXPECT_EQ(timestampPacketTypeSize / 4, eventObj1->getTimestampSizeInDw());

    EXPECT_EQ(0u, eventObj0->getContextStartOffset());
    EXPECT_EQ(timestampPacketTypeSize, eventObj0->getGlobalStartOffset());
    EXPECT_EQ(timestampPacketTypeSize * 2, eventObj0->getContextEndOffset());
    EXPECT_EQ(timestampPacketTypeSize * 3, eventObj0->getGlobalEndOffset());

    EXPECT_EQ(l0GfxCoreHelper.getImmediateWritePostSyncOffset(), eventObj0->getSinglePacketSize());

    auto hostPtrDiff = ptrDiff(eventObj1->getHostAddress(), eventObj0->getHostAddress());
    EXPECT_EQ(expectedSize, hostPtrDiff);
}

HWTEST_F(EventSizeTests, givenDebugFlagwhenCreatingEventPoolThenUseCorrectSizeAndAlignment) {
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto expectedAlignment = static_cast<uint32_t>(gfxCoreHelper.getTimestampPacketAllocatorAlignment());

    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    uint32_t packetCount = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());

    {
        debugManager.flags.OverrideTimestampPacketSize.set(4);

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool.reset(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto singlePacketSize = TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();

        auto expectedSize = static_cast<uint32_t>(alignUp(packetCount * singlePacketSize, expectedAlignment));

        EXPECT_EQ(expectedSize, eventPool->getEventSize());

        createEvents();

        EXPECT_EQ(1u, eventObj0->getTimestampSizeInDw());
        EXPECT_EQ(1u, eventObj1->getTimestampSizeInDw());

        auto hostPtrDiff = ptrDiff(eventObj1->getHostAddress(), eventObj0->getHostAddress());
        EXPECT_EQ(expectedSize, hostPtrDiff);
    }

    {
        debugManager.flags.OverrideTimestampPacketSize.set(8);

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool.reset(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto singlePacketSize = TimestampPackets<uint64_t, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();

        auto expectedSize = static_cast<uint32_t>(alignUp(packetCount * singlePacketSize, expectedAlignment));

        EXPECT_EQ(expectedSize, eventPool->getEventSize());

        createEvents();

        EXPECT_EQ(2u, eventObj0->getTimestampSizeInDw());
        EXPECT_EQ(2u, eventObj1->getTimestampSizeInDw());

        auto hostPtrDiff = ptrDiff(eventObj1->getHostAddress(), eventObj0->getHostAddress());
        EXPECT_EQ(expectedSize, hostPtrDiff);
    }

    {
        debugManager.flags.OverrideTimestampPacketSize.set(12);
        ze_result_t result = ZE_RESULT_SUCCESS;
        EXPECT_ANY_THROW(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result));
        EXPECT_ANY_THROW(createEvents());
    }
}

HWTEST_F(EventTests, whenCreatingNonTimestampEventsThenPacketsSizeIsQword) {
    DebugManagerStateRestore restore;

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    const ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};

    std::unique_ptr<L0::EventPool> timestampPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    eventPoolDesc.flags = 0;
    std::unique_ptr<L0::EventPool> regularPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    ze_event_handle_t timestampEventHandle = nullptr;
    ze_event_handle_t regularEventHandle = nullptr;

    timestampPool->createEvent(&eventDesc, &timestampEventHandle);
    regularPool->createEvent(&eventDesc, &regularEventHandle);

    auto timestampEvent = Event::fromHandle(timestampEventHandle);
    auto regularEvent = Event::fromHandle(regularEventHandle);

    auto timestampSinglePacketSize = NEO::TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>::getSinglePacketSize();
    EXPECT_EQ(timestampSinglePacketSize, timestampEvent->getSinglePacketSize());
    EXPECT_EQ(device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset(), regularEvent->getSinglePacketSize());

    timestampEvent->destroy();
    regularEvent->destroy();
}

HWTEST_F(EventTests, GivenEventWhenHostSynchronizeCalledThenExpectDownloadEventAllocationOnlyWhenEventWasUsedOnGpu) {
    std::map<GraphicsAllocation *, uint32_t> downloadAllocationTrack;

    constexpr uint32_t iterations = 2;

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_CLEARED);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    ze_result_t result;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));

    ASSERT_NE(event, nullptr);
    ASSERT_NE(nullptr, event->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, event->csrs[0]);

    size_t eventCompletionOffset = event->getContextStartOffset();
    if (event->isEventTimestampFlagSet()) {
        eventCompletionOffset = event->getContextEndOffset();
    }
    TagAddressType *eventAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    *eventAddress = Event::STATE_INITIAL;

    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = eventAddress;

    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        if (CpuIntrinsicsTests::pauseCounter >= iterations) {
            volatile TagAddressType *packet = CpuIntrinsicsTests::pauseAddress;
            *packet = Event::STATE_SIGNALED;
        }
    };

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0]);
    ultCsr->initializeResources(device->getDevicePreemptionMode());
    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation, uint64_t, size_t)>> backupCsrDownloadImpl(&ultCsr->downloadAllocationImpl);
    ultCsr->downloadAllocationImpl = [&downloadAllocationTrack](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        downloadAllocationTrack[&gfxAllocation]++;
    };

    auto eventAllocation = event->getAllocation(device);
    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t downloadedAllocations = downloadAllocationTrack[eventAllocation];
    EXPECT_EQ(0u, downloadedAllocations);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);

    event->reset();
    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = eventAddress;
    *eventAddress = Event::STATE_INITIAL;
    ultCsr->downloadAllocationsCalledCount = 0;

    eventAllocation->updateTaskCount(0u, ultCsr->getOsContext().getContextId());

    result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    downloadedAllocations = downloadAllocationTrack[eventAllocation];
    EXPECT_EQ(iterations + 1u, downloadedAllocations);
    EXPECT_EQ(2u, ultCsr->downloadAllocationsCalledCount);

    event->destroy();
}

struct EventContextGroupFixture : public EventFixture<1, 0> {
    using BaseClass = EventFixture<1, 0>;
    void setUp() {
        HardwareInfo hwInfo = *defaultHwInfo;
        if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
            skipped = true;
            GTEST_SKIP();
        }

        debugManager.flags.ContextGroupSize.set(2);
        BaseClass::setUp();
    }

    void tearDown() {
        if (!skipped) {
            BaseClass::tearDown();
        }
    }

    DebugManagerStateRestore restore;
    bool skipped = false;
};

using EventContextGroupTests = Test<EventContextGroupFixture>;

HWTEST_F(EventContextGroupTests, givenSecondaryCsrWhenDownloadingAllocationThenUseCorrectCsr) {
    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));

    size_t eventCompletionOffset = event->getContextStartOffset();
    if (event->isEventTimestampFlagSet()) {
        eventCompletionOffset = event->getContextEndOffset();
    }
    TagAddressType *eventAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    *eventAddress = Event::STATE_INITIAL;

    auto ultCsr = new UltCommandStreamReceiver<FamilyType>(*neoDevice->getExecutionEnvironment(), 0, 1);
    neoDevice->secondaryCsrs.clear();
    neoDevice->secondaryCsrs.push_back(std::unique_ptr<UltCommandStreamReceiver<FamilyType>>(ultCsr));

    OsContext osContext(0, static_cast<uint32_t>(neoDevice->getAllEngines().size()), EngineDescriptorHelper::getDefaultDescriptor());

    ultCsr->setupContext(osContext);
    ultCsr->initializeResources(device->getDevicePreemptionMode());

    uint32_t downloadCounter = 0;
    ultCsr->downloadAllocationImpl = [&downloadCounter](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        downloadCounter++;
    };

    auto eventAllocation = event->getAllocation(device);
    ultCsr->makeResident(*eventAllocation);

    event->hostSynchronize(1);

    EXPECT_EQ(1u, downloadCounter);

    event->destroy();
}

template <typename GfxFamily>
struct CacheFlushTrackingCsr : public UltCommandStreamReceiver<GfxFamily> {
    using BaseClass = UltCommandStreamReceiver<GfxFamily>;

    CacheFlushTrackingCsr(NEO::ExecutionEnvironment &executionEnvironment,
                          uint32_t rootDeviceIndex,
                          const NEO::DeviceBitfield deviceBitfield)
        : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield) {}

    NEO::SubmissionStatus flushTagUpdate() override {
        this->flushTagUpdateCalled = true;
        this->latestFlushedTaskCount = this->taskCount.load();
        return NEO::SubmissionStatus::success;
    }
};

template <typename GfxFamily>
struct KmdWaitTrackingCsr : public CacheFlushTrackingCsr<GfxFamily> {
    using BaseClass = CacheFlushTrackingCsr<GfxFamily>;

    KmdWaitTrackingCsr(NEO::ExecutionEnvironment &executionEnvironment,
                       uint32_t rootDeviceIndex,
                       const NEO::DeviceBitfield deviceBitfield)
        : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield) {}

    NEO::WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, NEO::QueueThrottle throttle) override {
        this->waitForTaskCountWithKmdNotifyInputParams.push_back({taskCountToWait, flushStampToWait, useQuickKmdSleep, throttle});
        if (onKmdWait) {
            onKmdWait();
        }
        if (this->waitForTaskCountWithKmdNotifyFallbackReturnValue.has_value()) {
            return *this->waitForTaskCountWithKmdNotifyFallbackReturnValue;
        }
        return NEO::WaitStatus::ready;
    }

    NEO::WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, NEO::QueueThrottle throttle, uint64_t timeoutNanoseconds) override {
        this->waitForTaskCountWithKmdNotifyInputParams.push_back({taskCountToWait, flushStampToWait, useQuickKmdSleep, throttle});
        boundedKmdWaitTimeouts.push_back(timeoutNanoseconds);
        if (onKmdWait) {
            onKmdWait();
        }
        if (this->waitForTaskCountWithKmdNotifyFallbackReturnValue.has_value()) {
            return *this->waitForTaskCountWithKmdNotifyFallbackReturnValue;
        }
        return NEO::WaitStatus::ready;
    }

    std::function<void()> onKmdWait;
    std::vector<uint64_t> boundedKmdWaitTimeouts;
};

HWTEST_F(EventContextGroupTests, givenPendingSelectedSecondaryCsrTaskWhenHostSynchronizeRequiresCacheFlushThenFlushAndWaitAreCalledOnSelectedCsrOnly) {
    if (!device->getGfxCoreHelper().areSecondaryContextsSupported()) {
        GTEST_SKIP();
    }

    if (!event->isDcFlushAllowed) {
        GTEST_SKIP();
    }

    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();

    auto defaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    ASSERT_NE(nullptr, defaultCsr);

    defaultCsr->taskCount = 0;
    defaultCsr->latestFlushedTaskCount = 0;
    defaultCsr->flushTagUpdateCalled = false;
    defaultCsr->waitForTaskCountCalled = false;
    defaultCsr->waitForTaskCountReturnValue = NEO::WaitStatus::ready;

    auto secondaryCsr = std::make_unique<CacheFlushTrackingCsr<FamilyType>>(*neoDevice->getExecutionEnvironment(), 0, 1);

    OsContext osContext(0, static_cast<uint32_t>(neoDevice->getAllEngines().size()), EngineDescriptorHelper::getDefaultDescriptor());
    secondaryCsr->setupContext(osContext);
    secondaryCsr->initializeResources(device->getDevicePreemptionMode());
    secondaryCsr->setPrimaryCsr(defaultCsr);

    secondaryCsr->taskCount = 1;
    secondaryCsr->latestFlushedTaskCount = 0;
    secondaryCsr->flushTagUpdateCalled = false;
    secondaryCsr->waitForTaskCountCalled = false;
    secondaryCsr->waitForTaskCountReturnValue = NEO::WaitStatus::ready;

    auto secondaryCsrPtr = secondaryCsr.get();

    event->setCsrForCacheFlush(secondaryCsrPtr);
    event->setDualCopyOffload(true);
    *static_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;

    auto result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(defaultCsr->flushTagUpdateCalled);
    EXPECT_FALSE(defaultCsr->waitForTaskCountCalled);

    EXPECT_TRUE(secondaryCsrPtr->flushTagUpdateCalled);
    EXPECT_TRUE(secondaryCsrPtr->waitForTaskCountCalled);
    EXPECT_EQ(1u, secondaryCsrPtr->peekLatestFlushedTaskCount());
}

HWTEST_F(EventContextGroupTests, givenKmdWaitStrategyAndInfiniteTimeoutWhenHostSynchronizeRequiresCacheFlushThenKmdWaitUsesSelectedCsrAndEventIsQueriedAgain) {
    if (!device->getGfxCoreHelper().areSecondaryContextsSupported()) {
        GTEST_SKIP();
    }

    if (!event->isDcFlushAllowed) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    auto defaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    ASSERT_NE(nullptr, defaultCsr);

    defaultCsr->taskCount = 0;
    defaultCsr->latestFlushedTaskCount = 0;
    defaultCsr->flushTagUpdateCalled = false;
    defaultCsr->waitForTaskCountCalled = false;
    defaultCsr->captureWaitForTaskCountWithKmdNotifyInputParams = true;
    defaultCsr->waitForTaskCountWithKmdNotifyInputParams.clear();

    auto secondaryCsr = std::make_unique<KmdWaitTrackingCsr<FamilyType>>(*neoDevice->getExecutionEnvironment(), 0, 1);

    OsContext osContext(0, static_cast<uint32_t>(neoDevice->getAllEngines().size()), EngineDescriptorHelper::getDefaultDescriptor());
    secondaryCsr->setupContext(osContext);
    secondaryCsr->initializeResources(device->getDevicePreemptionMode());
    secondaryCsr->setPrimaryCsr(defaultCsr);

    secondaryCsr->taskCount = 1;
    secondaryCsr->latestFlushedTaskCount = 0;
    secondaryCsr->flushTagUpdateCalled = false;
    secondaryCsr->waitForTaskCountCalled = false;
    secondaryCsr->captureWaitForTaskCountWithKmdNotifyInputParams = true;
    secondaryCsr->waitForTaskCountWithKmdNotifyFallbackReturnValue = NEO::WaitStatus::ready;
    secondaryCsr->flushStamp->setStamp(0);

    auto secondaryCsrPtr = secondaryCsr.get();

    event->setCsrForCacheFlush(secondaryCsrPtr);
    event->setDualCopyOffload(true);
    auto eventAddress = static_cast<uint32_t *>(event->getHostAddress());
    *eventAddress = Event::STATE_INITIAL;
    uint32_t kmdWaitCallbacks = 0;
    secondaryCsr->onKmdWait = [&]() {
        kmdWaitCallbacks++;
        *eventAddress = Event::STATE_SIGNALED;
    };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, kmdWaitCallbacks);

    EXPECT_FALSE(defaultCsr->flushTagUpdateCalled);
    EXPECT_FALSE(defaultCsr->waitForTaskCountCalled);
    EXPECT_TRUE(defaultCsr->waitForTaskCountWithKmdNotifyInputParams.empty());

    EXPECT_TRUE(secondaryCsrPtr->flushTagUpdateCalled);
    EXPECT_FALSE(secondaryCsrPtr->waitForTaskCountCalled);
    ASSERT_EQ(1u, secondaryCsrPtr->waitForTaskCountWithKmdNotifyInputParams.size());
    EXPECT_EQ(1u, secondaryCsrPtr->waitForTaskCountWithKmdNotifyInputParams[0].taskCountToWait);
    EXPECT_EQ(1u, secondaryCsrPtr->waitForTaskCountWithKmdNotifyInputParams[0].flushStampToWait);
    EXPECT_FALSE(secondaryCsrPtr->waitForTaskCountWithKmdNotifyInputParams[0].useQuickKmdSleep);
    EXPECT_EQ(NEO::QueueThrottle::LOW, secondaryCsrPtr->waitForTaskCountWithKmdNotifyInputParams[0].throttle);
}

HWTEST_F(EventContextGroupTests, givenKmdWaitStrategyAndShortFiniteTimeoutWhenHostSynchronizeRequiresCacheFlushThenKmdWaitIsNotUsed) {
    if (!device->getGfxCoreHelper().areSecondaryContextsSupported()) {
        GTEST_SKIP();
    }

    if (!event->isDcFlushAllowed) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    auto secondaryCsr = std::make_unique<KmdWaitTrackingCsr<FamilyType>>(*neoDevice->getExecutionEnvironment(), 0, 1);

    OsContext osContext(0, static_cast<uint32_t>(neoDevice->getAllEngines().size()), EngineDescriptorHelper::getDefaultDescriptor());
    secondaryCsr->setupContext(osContext);
    secondaryCsr->initializeResources(device->getDevicePreemptionMode());
    secondaryCsr->taskCount = 1;
    secondaryCsr->latestFlushedTaskCount = 0;
    secondaryCsr->flushTagUpdateCalled = false;
    secondaryCsr->captureWaitForTaskCountWithKmdNotifyInputParams = true;

    auto secondaryCsrPtr = secondaryCsr.get();

    event->setCsrForCacheFlush(secondaryCsrPtr);
    event->setDualCopyOffload(true);
    *static_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_INITIAL;

    auto result = event->hostSynchronize(1);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    EXPECT_FALSE(secondaryCsrPtr->flushTagUpdateCalled);
    EXPECT_TRUE(secondaryCsrPtr->waitForTaskCountWithKmdNotifyInputParams.empty());
}

HWTEST_F(EventContextGroupTests, givenKmdWaitStrategyAndLongFiniteTimeoutWhenHostSynchronizeRequiresCacheFlushThenBoundedKmdWaitIsUsed) {
    if (!device->getGfxCoreHelper().areSecondaryContextsSupported()) {
        GTEST_SKIP();
    }

    if (!event->isDcFlushAllowed) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategyMinTimeoutMicroseconds.set(20000);

    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    auto secondaryCsr = std::make_unique<KmdWaitTrackingCsr<FamilyType>>(*neoDevice->getExecutionEnvironment(), 0, 1);
    OsContext osContext(0, static_cast<uint32_t>(neoDevice->getAllEngines().size()), EngineDescriptorHelper::getDefaultDescriptor());
    secondaryCsr->setupContext(osContext);
    secondaryCsr->initializeResources(device->getDevicePreemptionMode());
    secondaryCsr->taskCount = 1;
    secondaryCsr->latestFlushedTaskCount = 0;
    secondaryCsr->flushStamp->setStamp(0);

    auto eventAddress = static_cast<uint32_t *>(event->getHostAddress());
    *eventAddress = Event::STATE_INITIAL;
    secondaryCsr->onKmdWait = [&]() {
        *eventAddress = Event::STATE_SIGNALED;
    };
    event->setCsrForCacheFlush(secondaryCsr.get());
    event->setDualCopyOffload(true);

    constexpr uint64_t timeoutNanoseconds = 1000000000u;
    auto result = event->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, secondaryCsr->boundedKmdWaitTimeouts.size());
    EXPECT_GT(secondaryCsr->boundedKmdWaitTimeouts[0], 0u);
    EXPECT_LE(secondaryCsr->boundedKmdWaitTimeouts[0], timeoutNanoseconds - 20000000u);
    EXPECT_EQ(1u, secondaryCsr->waitForTaskCountWithKmdNotifyInputParams.size());
}

HWTEST_F(EventContextGroupTests, givenKmdWaitStrategyAndCounterBasedEventWithoutCacheFlushWhenHostSynchronizeThenKmdWaitIsNotUsed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.AllowDcFlush.set(0);
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    event.reset();
    ze_result_t createResult = ZE_RESULT_SUCCESS;
    event = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device, createResult)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, createResult);
    ASSERT_NE(nullptr, event);
    ASSERT_FALSE(event->isDcFlushAllowed);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    ASSERT_TRUE(event->isCounterBased());

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    ASSERT_NE(nullptr, csr);
    csr->taskCount = 1;
    csr->latestFlushedTaskCount = 0;
    csr->captureWaitForTaskCountWithKmdNotifyInputParams = true;
    csr->waitForTaskCountWithKmdNotifyInputParams.clear();

    uint64_t counterStorage = Event::STATE_INITIAL;
    NEO::MockGraphicsAllocation counterAllocation(&counterStorage, sizeof(counterStorage));
    counterAllocation.updateTaskCount(1u, csr->getOsContext().getContextId());
    event->getInOrderExecEventHelper().assignData(1u, 0u, 1u, 1u, &counterAllocation, &counterAllocation, counterAllocation.getGpuAddress(), counterAllocation.getGpuAddress(), &counterStorage, 0u, 0u, false, false);

    auto counterAddress = static_cast<uint64_t *>(event->getInOrderExecEventHelper().getBaseHostCpuAddress());
    *counterAddress = Event::STATE_INITIAL;

    uint32_t pauseCounter = 0;
    VariableBackup<std::function<void()>> setupPauseAddressBackup(&CpuIntrinsicsTests::setupPauseAddress);
    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        if (++pauseCounter == 2) {
            *counterAddress = event->getInOrderExecBaseSignalValue();
        }
    };

    auto result = event->hostSynchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(csr->waitForTaskCountWithKmdNotifyInputParams.empty());
}

HWTEST_F(EventContextGroupTests, givenNoPendingTaskOnSelectedSecondaryCsrWhenHostSynchronizeRequiresCacheFlushThenFlushAndWaitAreNotCalled) {
    if (!device->getGfxCoreHelper().areSecondaryContextsSupported()) {
        GTEST_SKIP();
    }

    if (!event->isDcFlushAllowed) {
        GTEST_SKIP();
    }

    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();

    auto defaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    ASSERT_NE(nullptr, defaultCsr);

    defaultCsr->taskCount = 0;
    defaultCsr->latestFlushedTaskCount = 0;
    defaultCsr->flushTagUpdateCalled = false;
    defaultCsr->waitForTaskCountCalled = false;
    defaultCsr->waitForTaskCountReturnValue = NEO::WaitStatus::ready;

    auto secondaryCsr = std::make_unique<CacheFlushTrackingCsr<FamilyType>>(*neoDevice->getExecutionEnvironment(), 0, 1);

    OsContext osContext(0, static_cast<uint32_t>(neoDevice->getAllEngines().size()), EngineDescriptorHelper::getDefaultDescriptor());
    secondaryCsr->setupContext(osContext);
    secondaryCsr->initializeResources(device->getDevicePreemptionMode());
    secondaryCsr->setPrimaryCsr(defaultCsr);

    secondaryCsr->taskCount = 0;
    secondaryCsr->latestFlushedTaskCount = 0;
    secondaryCsr->flushTagUpdateCalled = false;
    secondaryCsr->waitForTaskCountCalled = false;
    secondaryCsr->waitForTaskCountReturnValue = NEO::WaitStatus::ready;

    auto secondaryCsrPtr = secondaryCsr.get();

    event->setCsrForCacheFlush(secondaryCsrPtr);
    event->setDualCopyOffload(true);
    *static_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;

    auto result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(defaultCsr->flushTagUpdateCalled);
    EXPECT_FALSE(defaultCsr->waitForTaskCountCalled);

    EXPECT_FALSE(secondaryCsrPtr->flushTagUpdateCalled);
    EXPECT_FALSE(secondaryCsrPtr->waitForTaskCountCalled);
}

HWTEST_F(EventContextGroupTests, givenNullCsrForCacheFlushWhenHostSynchronizeRequiresCacheFlushThenDefaultCsrIsUsed) {
    if (!event->isDcFlushAllowed) {
        GTEST_SKIP();
    }

    auto defaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    ASSERT_NE(nullptr, defaultCsr);

    defaultCsr->taskCount = 1;
    defaultCsr->latestFlushedTaskCount = 0;
    defaultCsr->flushTagUpdateCalled = false;
    defaultCsr->waitForTaskCountCalled = false;
    defaultCsr->waitForTaskCountReturnValue = NEO::WaitStatus::ready;
    defaultCsr->callFlushTagUpdate = false;

    event->setCsrForCacheFlush(nullptr);
    event->setDualCopyOffload(true);
    *static_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;

    auto result = event->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(defaultCsr->flushTagUpdateCalled);
    EXPECT_FALSE(defaultCsr->waitForTaskCountCalled);
}

HWTEST_F(EventTests, givenRegularEventWhenCallingResetAdditionalTimestampNodeMultipleTimesWithTagAllocatorThenTagAddedToAdditionalTimestampNodeVectorOnce) {

    MockTagAllocator<DeviceAllocNodeType<true>> eventTagAllocator(0, neoDevice->getMemoryManager());
    ze_result_t result;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_NE(event, nullptr);

    event->resetAdditionalTimestampNode(eventTagAllocator.getTag(), 1, false);

    event->resetAdditionalTimestampNode(eventTagAllocator.getTag(), 1, false);
    EXPECT_EQ(1u, event->inOrderExecHelper.getAdditionalTimestampNodesCount());
}

HWTEST_F(EventTests, givenRegularEventWhenCallingResetAdditionalTimestampNodeWithNullptrThenVectorCleared) {

    MockTagAllocator<DeviceAllocNodeType<true>> eventTagAllocator(0, neoDevice->getMemoryManager());
    ze_result_t result;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_NE(event, nullptr);

    event->resetAdditionalTimestampNode(eventTagAllocator.getTag(), 1, false);
    EXPECT_EQ(1u, event->inOrderExecHelper.getAdditionalTimestampNodesCount());

    event->resetAdditionalTimestampNode(nullptr, 0, false);
    EXPECT_EQ(0u, event->inOrderExecHelper.getAdditionalTimestampNodesCount());
}

HWTEST_F(EventTests, givenInOrderEventWhenHostSynchronizeIsCalledThenAllocationIsDonwloadedOnlyAfterEventWasUsedOnGpu) {
    std::map<GraphicsAllocation *, uint32_t> downloadAllocationTrack;

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, neoDevice->getMemoryManager());
    ze_result_t result;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));

    ASSERT_NE(event, nullptr);

    TagAddressType *eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0]);
    ultCsr->initializeResources(device->getDevicePreemptionMode());

    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation, uint64_t, size_t)>> backupCsrDownloadImpl(&ultCsr->downloadAllocationImpl);
    ultCsr->downloadAllocationImpl = [&downloadAllocationTrack](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        downloadAllocationTrack[&gfxAllocation]++;
    };

    auto mockNode = tagAllocator.getTag();
    auto syncAllocation = mockNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(mockNode, nullptr, *neoDevice, 1, false);
    *inOrderExecInfo->getBaseHostAddress(0) = 1;

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, downloadAllocationTrack[syncAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);

    auto event2 = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));

    event2->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event2->getInOrderExecEventHelper().initializeLocalTempStorage();
    event2->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);
    syncAllocation->updateTaskCount(0u, ultCsr->getOsContext().getContextId());
    ultCsr->downloadAllocationsCalledCount = 0;
    eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    result = event2->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(0u, downloadAllocationTrack[syncAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);
}

struct EventSignalerPreviousContextsTests : public EventTests {
    template <typename FamilyType>
    NEO::UltCommandStreamReceiver<FamilyType> *findPreviousContextCsr() {
        auto defaultCsr = this->neoDevice->getDefaultEngine().commandStreamReceiver;
        for (auto &engine : this->neoDevice->getAllEngines()) {
            if (engine.commandStreamReceiver != defaultCsr) {
                return static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
            }
        }
        return nullptr;
    }

    static constexpr TaskCountType previousContextTaskCount = 7u;
};

HWTEST_F(EventSignalerPreviousContextsTests, givenRegularEventWithSignalerPreviousContextsWhenHostSynchronizeSucceedsThenAllocationsOfPreviousContextsAreDownloadedOnlyInTbxMode) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    auto previousContextCsr = findPreviousContextCsr<FamilyType>();
    ASSERT_NE(nullptr, previousContextCsr);

    for (const auto csrType : {CommandStreamReceiverType::hardware, CommandStreamReceiverType::tbx}) {
        neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = csrType;
        ze_result_t result;
        auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
        ASSERT_NE(nullptr, event);

        size_t completionOffset = event->isEventTimestampFlagSet() ? event->getContextEndOffset() : event->getContextStartOffset();
        *static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), completionOffset)) = Event::STATE_SIGNALED;

        event->setSignalerPreviousContexts({{previousContextCsr, previousContextTaskCount}});
        previousContextCsr->downloadAllocationsCalledCount = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));

        if (csrType == CommandStreamReceiverType::tbx) {
            EXPECT_EQ(1u, previousContextCsr->downloadAllocationsCalledCount);
            EXPECT_TRUE(previousContextCsr->latestDownloadAllocationsBlocking);
            EXPECT_EQ(previousContextTaskCount, previousContextCsr->latestDownloadAllocationsTaskCount);
        } else {
            EXPECT_EQ(0u, previousContextCsr->downloadAllocationsCalledCount);
        }
    }
}

HWTEST_F(EventSignalerPreviousContextsTests, givenCounterBasedEventWithSignalerPreviousContextsWhenHostSynchronizeSucceedsThenAllocationsOfPreviousContextsAreDownloadedOnlyInTbxMode) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, neoDevice->getMemoryManager());
    auto previousContextCsr = findPreviousContextCsr<FamilyType>();
    ASSERT_NE(nullptr, previousContextCsr);

    for (const auto csrType : {CommandStreamReceiverType::hardware, CommandStreamReceiverType::tbx}) {
        neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = csrType;
        ze_result_t result;
        auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
        ASSERT_NE(nullptr, event);

        auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(tagAllocator.getTag(), nullptr, *neoDevice, 1, false);
        *inOrderExecInfo->getBaseHostAddress(0) = 1;
        event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
        event->getInOrderExecEventHelper().initializeLocalTempStorage();
        event->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);

        event->setSignalerPreviousContexts({{previousContextCsr, previousContextTaskCount}});
        previousContextCsr->downloadAllocationsCalledCount = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));

        if (csrType == CommandStreamReceiverType::tbx) {
            EXPECT_EQ(1u, previousContextCsr->downloadAllocationsCalledCount);
            EXPECT_TRUE(previousContextCsr->latestDownloadAllocationsBlocking);
            EXPECT_EQ(previousContextTaskCount, previousContextCsr->latestDownloadAllocationsTaskCount);
        } else {
            EXPECT_EQ(0u, previousContextCsr->downloadAllocationsCalledCount);
        }
    }
}

HWTEST_F(EventSignalerPreviousContextsTests, givenEventWithSignalerPreviousContextsWhenEventIsResetThenPreviousContextsAreCleared) {
    auto previousContextCsr = findPreviousContextCsr<FamilyType>();
    ASSERT_NE(nullptr, previousContextCsr);
    ze_result_t result;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_NE(nullptr, event);

    event->setSignalerPreviousContexts({{previousContextCsr, previousContextTaskCount}});
    ASSERT_EQ(1u, event->signalerPreviousContexts.size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->reset());
    EXPECT_TRUE(event->signalerPreviousContexts.empty());
}

HWTEST_F(EventSignalerPreviousContextsTests, givenNotSignaledRegularEventWhenHostSynchronizeChecksForGpuHangThenDeviceLostIsReturnedOnlyWhenSignalerPreviousContextIsHung) {
    auto previousContextCsr = findPreviousContextCsr<FamilyType>();
    ASSERT_NE(nullptr, previousContextCsr);
    neoDevice->getUltCommandStreamReceiver<FamilyType>().isGpuHangDetectedReturnValue = false;

    for (const bool previousContextAttached : {false, true}) {
        for (const bool previousContextHung : {false, true}) {
            ze_result_t result;
            auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
            ASSERT_NE(nullptr, event);
            if (previousContextAttached) {
                event->setSignalerPreviousContexts({{previousContextCsr, previousContextTaskCount}});
            }
            previousContextCsr->isGpuHangDetectedReturnValue = previousContextHung;

            const auto expectedResult = (previousContextAttached && previousContextHung) ? ZE_RESULT_ERROR_DEVICE_LOST : ZE_RESULT_NOT_READY;
            EXPECT_EQ(expectedResult, event->hostSynchronize(0));
        }
    }
}

HWTEST_F(EventSignalerPreviousContextsTests, givenNotCompletedCounterBasedEventWhenHostSynchronizeChecksForGpuHangThenDeviceLostIsReturnedOnlyWhenSignalerPreviousContextIsHung) {
    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, neoDevice->getMemoryManager());
    auto previousContextCsr = findPreviousContextCsr<FamilyType>();
    ASSERT_NE(nullptr, previousContextCsr);
    neoDevice->getUltCommandStreamReceiver<FamilyType>().isGpuHangDetectedReturnValue = false;

    for (const bool previousContextAttached : {false, true}) {
        for (const bool previousContextHung : {false, true}) {
            ze_result_t result;
            auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
            ASSERT_NE(nullptr, event);

            auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(tagAllocator.getTag(), nullptr, *neoDevice, 1, false);
            *inOrderExecInfo->getBaseHostAddress(0) = 0;
            event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
            event->getInOrderExecEventHelper().initializeLocalTempStorage();
            event->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);

            if (previousContextAttached) {
                event->setSignalerPreviousContexts({{previousContextCsr, previousContextTaskCount}});
            }
            previousContextCsr->isGpuHangDetectedReturnValue = previousContextHung;

            const auto expectedResult = (previousContextAttached && previousContextHung) ? ZE_RESULT_ERROR_DEVICE_LOST : ZE_RESULT_NOT_READY;
            EXPECT_EQ(expectedResult, event->hostSynchronize(0));
        }
    }
}

HWTEST_F(EventSignalerPreviousContextsTests, givenSignaledEventAndForcedGpuStatusCheckWhenHostSynchronizingThenDeviceLostIsReturnedOnlyWhenSignalerPreviousContextIsHung) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForceGpuStatusCheckOnSuccessfulEventHostSynchronize.set(1);
    auto previousContextCsr = findPreviousContextCsr<FamilyType>();
    ASSERT_NE(nullptr, previousContextCsr);
    neoDevice->getUltCommandStreamReceiver<FamilyType>().isGpuHangDetectedReturnValue = false;

    for (const bool previousContextHung : {false, true}) {
        ze_result_t result;
        auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
        ASSERT_NE(nullptr, event);
        size_t completionOffset = event->isEventTimestampFlagSet() ? event->getContextEndOffset() : event->getContextStartOffset();
        *static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), completionOffset)) = Event::STATE_SIGNALED;

        event->setSignalerPreviousContexts({{previousContextCsr, previousContextTaskCount}});
        previousContextCsr->isGpuHangDetectedReturnValue = previousContextHung;

        const auto expectedResult = previousContextHung ? ZE_RESULT_ERROR_DEVICE_LOST : ZE_RESULT_SUCCESS;
        EXPECT_EQ(expectedResult, event->hostSynchronize(0));
    }
}

HWTEST_F(EventTests, givenExternalCbEventWhenHostSynchronizeIsCalledThenPreambleAllocationIsDonwloadedAndCounterIsSignaled) {
    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_SIGNALED);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    VariableBackup<std::function<void()>> backupControlTpause(&CpuIntrinsicsTests::controlTpause);

    std::map<GraphicsAllocation *, uint32_t> downloadAllocationTrack;

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, neoDevice->getMemoryManager());
    ze_result_t result;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));

    ASSERT_NE(event, nullptr);

    TagAddressType *eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0]);
    ultCsr->initializeResources(device->getDevicePreemptionMode());

    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation, uint64_t, size_t)>> backupCsrDownloadImpl(&ultCsr->downloadAllocationImpl);
    ultCsr->downloadAllocationImpl = [&downloadAllocationTrack](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        downloadAllocationTrack[&gfxAllocation]++;
    };

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isGpuHangDetectedReturnValue = false;

    auto mockNode = tagAllocator.getTag();
    auto syncAllocation = mockNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    uint64_t counterValue = 1;
    uint64_t patchPreambleData = 0;
    uint64_t *patchPreambleHostAddress = &patchPreambleData;
    MockGraphicsAllocation preambleAllocation(patchPreambleHostAddress, reinterpret_cast<uint64_t>(patchPreambleHostAddress), sizeof(patchPreambleData));
    uint64_t preambleDevAddress = 0x1000;
    MockGraphicsAllocation preambleDevAllocation(nullptr, preambleDevAddress, sizeof(patchPreambleData));

    CpuIntrinsicsTests::tpauseCounter = 0u;

    CpuIntrinsicsTests::controlTpause = [patchPreambleHostAddress, counterValue]() {
        if (CpuIntrinsicsTests::tpauseCounter > 1) {
            *patchPreambleHostAddress = counterValue;
        }
    };

    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = eventAddress;
    CpuIntrinsicsTests::setupPauseAddress = [patchPreambleHostAddress, counterValue]() {
        if (CpuIntrinsicsTests::pauseCounter > 1) {
            *patchPreambleHostAddress = counterValue;
        }
    };

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(mockNode, nullptr, *neoDevice, 1, false);
    *inOrderExecInfo->getBaseHostAddress(0) = 1;

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);
    event->externalEvent = true;

    event->getInOrderExecEventHelper().assignPatchPreambleData(counterValue, patchPreambleHostAddress, preambleAllocation.getGpuAddress(), &preambleAllocation, preambleDevAddress, &preambleDevAllocation);

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, downloadAllocationTrack[syncAllocation]);
    EXPECT_EQ(0u, downloadAllocationTrack[&preambleAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);

    auto event2 = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));

    event2->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event2->getInOrderExecEventHelper().initializeLocalTempStorage();
    event2->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);
    event2->externalEvent = true;
    event2->getInOrderExecEventHelper().assignPatchPreambleData(counterValue, patchPreambleHostAddress, preambleAllocation.getGpuAddress(), &preambleAllocation, preambleDevAddress, &preambleDevAllocation);

    syncAllocation->updateTaskCount(0u, ultCsr->getOsContext().getContextId());
    preambleAllocation.updateTaskCount(0u, ultCsr->getOsContext().getContextId());
    ultCsr->downloadAllocationsCalledCount = 0;
    eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    result = event2->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(0u, downloadAllocationTrack[syncAllocation]);
    EXPECT_NE(0u, downloadAllocationTrack[&preambleAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);
}

HWTEST_F(EventTests, givenNotExternalCbEventWithPatchPreambleAssignedWhenHostSynchronizeIsCalledThenPreambleAllocationIsDonwloadedAndCounterIsSignaled) {
    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_SIGNALED);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    VariableBackup<std::function<void()>> backupControlTpause(&CpuIntrinsicsTests::controlTpause);

    std::map<GraphicsAllocation *, uint32_t> downloadAllocationTrack;

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, neoDevice->getMemoryManager());
    ze_result_t result;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_NE(event, nullptr);

    TagAddressType *eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0]);
    ultCsr->initializeResources(device->getDevicePreemptionMode());

    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation, uint64_t, size_t)>> backupCsrDownloadImpl(&ultCsr->downloadAllocationImpl);
    ultCsr->downloadAllocationImpl = [&downloadAllocationTrack](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        downloadAllocationTrack[&gfxAllocation]++;
    };

    auto &csr = this->neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isGpuHangDetectedReturnValue = false;

    auto mockNode = tagAllocator.getTag();
    auto syncAllocation = mockNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    uint64_t counterValue = 1;
    uint64_t patchPreambleData = 0;
    uint64_t *patchPreambleHostAddress = &patchPreambleData;
    MockGraphicsAllocation preambleAllocation(patchPreambleHostAddress, reinterpret_cast<uint64_t>(patchPreambleHostAddress), sizeof(patchPreambleData));
    uint64_t preambleDevAddress = 0x1000;
    MockGraphicsAllocation preambleDevAllocation(nullptr, preambleDevAddress, sizeof(patchPreambleData));

    CpuIntrinsicsTests::tpauseCounter = 0u;

    CpuIntrinsicsTests::controlTpause = [patchPreambleHostAddress, counterValue]() {
        if (CpuIntrinsicsTests::tpauseCounter > 1) {
            *patchPreambleHostAddress = counterValue;
        }
    };

    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = eventAddress;
    CpuIntrinsicsTests::setupPauseAddress = [patchPreambleHostAddress, counterValue]() {
        if (CpuIntrinsicsTests::pauseCounter > 1) {
            *patchPreambleHostAddress = counterValue;
        }
    };

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(mockNode, nullptr, *neoDevice, 1, false);
    *inOrderExecInfo->getBaseHostAddress(0) = 1;

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);
    event->externalEvent = false;

    event->getInOrderExecEventHelper().assignPatchPreambleData(counterValue, patchPreambleHostAddress, preambleAllocation.getGpuAddress(), &preambleAllocation, preambleDevAddress, &preambleDevAllocation);

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, downloadAllocationTrack[syncAllocation]);
    EXPECT_EQ(0u, downloadAllocationTrack[&preambleAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);
}

HWTEST_F(EventTests, GivenLinuxUserFenceKmdWaitFlagWhenCreatingHostVisibleEventThenKmdWaitIsEnabledWithoutUserInterrupt) {
    DebugManagerStateRestore restore;
    ze_result_t result = ZE_RESULT_SUCCESS;

    neoDevice->getUltCommandStreamReceiver<FamilyType>().isWaitUserFenceNotEqualSupportedValue = true;

    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(false);
    auto eventWithoutFlag = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(eventWithoutFlag->isLinuxUserFenceKmdWaitEnabled());
    EXPECT_FALSE(eventWithoutFlag->isSignalWithUserInterrupt());
    // The dedicated flag should not enable the generic interrupt mode (post-sync interrupt bit) behavior.
    EXPECT_FALSE(eventWithoutFlag->isInterruptModeEnabled());

    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    auto eventWithFlag = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(eventWithFlag->isLinuxUserFenceKmdWaitEnabled());
    EXPECT_FALSE(eventWithFlag->isSignalWithUserInterrupt());
    EXPECT_FALSE(eventWithFlag->isInterruptModeEnabled());
}

HWTEST_F(EventTests, GivenLinuxUserFenceKmdWaitFlagWhenNotEqualUserFenceUnsupportedThenKmdWaitIsNotEnabled) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);

    neoDevice->getUltCommandStreamReceiver<FamilyType>().isWaitUserFenceNotEqualSupportedValue = false;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(event->isLinuxUserFenceKmdWaitEnabled());
    EXPECT_FALSE(event->isSignalWithUserInterrupt());
}

HWTEST_F(EventTests, GivenLinuxUserFenceKmdWaitFlagWhenCreatingDeviceOnlyEventThenKmdWaitIsNotEnabled) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);

    neoDevice->getUltCommandStreamReceiver<FamilyType>().isWaitUserFenceNotEqualSupportedValue = true;

    ze_event_pool_desc_t deviceOnlyPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    deviceOnlyPoolDesc.count = 1;
    deviceOnlyPoolDesc.flags = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto deviceOnlyPool = std::unique_ptr<L0::EventPool>(whiteboxCast(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &deviceOnlyPoolDesc, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, deviceOnlyPool);

    auto deviceOnlyEvent = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(deviceOnlyPool.get(), &eventDesc, device, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(deviceOnlyEvent->isLinuxUserFenceKmdWaitEnabled());
    EXPECT_FALSE(deviceOnlyEvent->isSignalWithUserInterrupt());
}

HWTEST_F(EventTests, GivenLinuxUserFenceKmdWaitFlagWhenCreatingHostVisibleStandaloneCounterBasedEventThenKmdWaitIsEnabledWithoutUserInterrupt) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);

    neoDevice->getUltCommandStreamReceiver<FamilyType>().isWaitUserFenceNotEqualSupportedValue = true;

    ze_event_handle_t handle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &defaultIntelCounterBasedEventDesc, &handle));

    auto event = zeUniquePtr(whiteboxCast(Event::fromHandle(handle)));
    ASSERT_NE(nullptr, event);
    EXPECT_TRUE(event->isCounterBasedExplicitlyEnabled());
    EXPECT_TRUE(event->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));
    EXPECT_TRUE(event->isLinuxUserFenceKmdWaitEnabled());
    EXPECT_FALSE(event->isSignalWithUserInterrupt());
    EXPECT_FALSE(event->isInterruptModeEnabled());
}

HWTEST_F(EventTests, GivenLinuxUserFenceKmdWaitFlagWhenCreatingDeviceScopeStandaloneCounterBasedEventThenKmdWaitIsNotEnabled) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);

    neoDevice->getUltCommandStreamReceiver<FamilyType>().isWaitUserFenceNotEqualSupportedValue = true;

    ze_event_counter_based_desc_t deviceScopeDesc = defaultIntelCounterBasedEventDesc;
    deviceScopeDesc.signal = 0;

    ze_event_handle_t handle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &deviceScopeDesc, &handle));

    auto event = zeUniquePtr(whiteboxCast(Event::fromHandle(handle)));
    ASSERT_NE(nullptr, event);
    EXPECT_TRUE(event->isCounterBasedExplicitlyEnabled());
    EXPECT_FALSE(event->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));
    EXPECT_FALSE(event->isLinuxUserFenceKmdWaitEnabled());
    EXPECT_FALSE(event->isSignalWithUserInterrupt());
}

HWTEST_F(EventTests, GivenWindowsDiscreteKmdWaitFlagWhenCreatingEventThenKmdWaitIsEnabledOnlyForHostVisibleEventOnDiscreteWddmDevice) {
    DebugManagerStateRestore restore;
    auto rootDeviceEnvironment = neoDevice->executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    rootDeviceEnvironment->osInterface = std::make_unique<NEO::OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    ze_result_t result = ZE_RESULT_SUCCESS;

    NEO::debugManager.flags.EventHostSynchronizeWindowsDiscreteKmdWait.set(false);
    auto eventWithoutFlag = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(eventWithoutFlag->isWindowsDiscreteKmdWaitEnabled());

    NEO::debugManager.flags.EventHostSynchronizeWindowsDiscreteKmdWait.set(true);
    auto eventWithFlag = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(eventWithFlag->isWindowsDiscreteKmdWaitEnabled());

    ze_event_pool_desc_t deviceOnlyPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    deviceOnlyPoolDesc.count = 1;
    auto deviceOnlyPool = std::unique_ptr<L0::EventPool>(whiteboxCast(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &deviceOnlyPoolDesc, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto deviceOnlyEvent = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(deviceOnlyPool.get(), &eventDesc, device, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(deviceOnlyEvent->isWindowsDiscreteKmdWaitEnabled());

    rootDeviceEnvironment->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    auto drmEvent = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(drmEvent->isWindowsDiscreteKmdWaitEnabled());

    rootDeviceEnvironment->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    rootDeviceEnvironment->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    auto integratedEvent = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(integratedEvent->isWindowsDiscreteKmdWaitEnabled());
}

HWTEST_F(EventTests, givenExternalCbEventWhenHostSynchronizeUsesWaitFenceThenPreambleCounterIsSynchronized) {
    NEO::debugManager.flags.WaitForUserFenceOnEventHostSynchronize.set(1);

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue, Event::STATE_SIGNALED);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    VariableBackup<std::function<void()>> backupControlTpause(&CpuIntrinsicsTests::controlTpause);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, neoDevice->getMemoryManager());
    ze_result_t result;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));

    TagAddressType *eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    ASSERT_NE(event, nullptr);
    EXPECT_TRUE(event->isKmdWaitModeEnabled());
    EXPECT_TRUE(event->isInterruptModeEnabled());

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    event->csrs[0] = ultCsr;
    ultCsr->initializeResources(device->getDevicePreemptionMode());
    ultCsr->isGpuHangDetectedReturnValue = false;

    ultCsr->isUserFenceWaitSupported = true;
    ultCsr->waitUserFenceParams.forceRetStatusEnabled = true;
    ultCsr->waitUserFenceParams.forceRetStatusValue = true;

    auto mockNode = tagAllocator.getTag();

    uint64_t counterValue = 1;
    uint64_t patchPreambleData = 0;
    uint64_t *patchPreambleHostAddress = &patchPreambleData;
    MockGraphicsAllocation preambleAllocation(patchPreambleHostAddress, reinterpret_cast<uint64_t>(patchPreambleHostAddress), sizeof(patchPreambleData));
    uint64_t preambleDevAddress = 0x1000;
    MockGraphicsAllocation preambleDevAllocation(nullptr, preambleDevAddress, sizeof(patchPreambleData));

    CpuIntrinsicsTests::tpauseCounter = 0u;

    CpuIntrinsicsTests::controlTpause = [patchPreambleHostAddress, counterValue]() {
        if (CpuIntrinsicsTests::tpauseCounter > 1) {
            *patchPreambleHostAddress = counterValue;
        }
    };

    CpuIntrinsicsTests::pauseCounter = 0u;
    CpuIntrinsicsTests::pauseAddress = eventAddress;
    CpuIntrinsicsTests::setupPauseAddress = [patchPreambleHostAddress, counterValue]() {
        if (CpuIntrinsicsTests::pauseCounter > 1) {
            *patchPreambleHostAddress = counterValue;
        }
    };

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(mockNode, nullptr, *neoDevice, 1, false);
    *inOrderExecInfo->getBaseHostAddress(0) = 1;

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);
    event->externalEvent = true;

    event->getInOrderExecEventHelper().assignPatchPreambleData(counterValue, patchPreambleHostAddress, preambleAllocation.getGpuAddress(), &preambleAllocation, preambleDevAddress, &preambleDevAllocation);

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(EventTests, givenExternalCbEventWithIncompletePreambleWhenHostSynchronizeUsesWaitFenceThenCoreIsYieldedOnlyWhenWaiting) {
    NEO::debugManager.flags.WaitForUserFenceOnEventHostSynchronize.set(1);

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress, nullptr);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);
    VariableBackup<std::function<void()>> backupControlTpause(&CpuIntrinsicsTests::controlTpause);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();
    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, neoDevice->getMemoryManager());
    ze_result_t result;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));
    ASSERT_NE(event, nullptr);

    TagAddressType *eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    EXPECT_TRUE(event->isKmdWaitModeEnabled());

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    event->csrs[0] = ultCsr;
    ultCsr->initializeResources(device->getDevicePreemptionMode());
    ultCsr->isGpuHangDetectedReturnValue = false;
    ultCsr->isUserFenceWaitSupported = true;
    ultCsr->waitUserFenceParams.forceRetStatusEnabled = true;
    ultCsr->waitUserFenceParams.forceRetStatusValue = true;

    auto mockNode = tagAllocator.getTag();

    uint64_t counterValue = 1;
    uint64_t patchPreambleData = 0;
    uint64_t *patchPreambleHostAddress = &patchPreambleData;
    MockGraphicsAllocation preambleAllocation(patchPreambleHostAddress, reinterpret_cast<uint64_t>(patchPreambleHostAddress), sizeof(patchPreambleData));
    uint64_t preambleDevAddress = 0x1000;
    MockGraphicsAllocation preambleDevAllocation(nullptr, preambleDevAddress, sizeof(patchPreambleData));

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(mockNode, nullptr, *neoDevice, 1, false);
    *inOrderExecInfo->getBaseHostAddress(0) = 1;

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);
    event->externalEvent = true;

    event->getInOrderExecEventHelper().assignPatchPreambleData(counterValue, patchPreambleHostAddress, preambleAllocation.getGpuAddress(), &preambleAllocation, preambleDevAddress, &preambleDevAllocation);

    auto yieldCountBefore = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->hostSynchronize(0));
    EXPECT_EQ(yieldCountBefore, CpuIntrinsicsTests::yieldCounter);

    yieldCountBefore = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->hostSynchronize(1));
    EXPECT_GT(CpuIntrinsicsTests::yieldCounter.load(), yieldCountBefore);
}

HWTEST_F(EventTests, givenStandaloneCbEventAndTbxModeWhenSynchronizingThenHandleCorrectly) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    uint64_t counterValue = 2;

    ze_host_mem_alloc_desc_t desc = {};
    void *ptr = nullptr;
    context->allocHostMem(&desc, sizeof(uint64_t), 1, &ptr);

    uint64_t *hostAddress = static_cast<uint64_t *>(ptr);

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_handle_t handle = nullptr;

    ze_event_counter_based_external_sync_allocation_desc_t externalSync = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
    externalSync.deviceAddress = gpuAddress;
    externalSync.hostAddress = hostAddress;
    externalSync.completionValue = counterValue;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalSync;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));

    auto eventObj = Event::fromHandle(handle);

    ASSERT_NE(event, nullptr);
    auto result = eventObj->hostSynchronize(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    zeEventDestroy(handle);
    context->freeMem(hostAddress);
}

HWTEST_F(EventTests, givenInOrderEventWithHostAllocWhenHostSynchronizeIsCalledThenAllocationIsDonwloadedOnlyAfterEventWasUsedOnGpu) {
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    std::map<GraphicsAllocation *, uint32_t> downloadAllocationTrack;

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, neoDevice->getMemoryManager());
    MockTagAllocator<DeviceAllocNodeType<true>> hostTagAllocator(0, neoDevice->getMemoryManager());
    ze_result_t result;
    auto event = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));

    ASSERT_NE(event, nullptr);

    TagAddressType *eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0]);
    ultCsr->initializeResources(device->getDevicePreemptionMode());
    VariableBackup<std::function<void(GraphicsAllocation & gfxAllocation, uint64_t, size_t)>> backupCsrDownloadImpl(&ultCsr->downloadAllocationImpl);
    ultCsr->downloadAllocationImpl = [&downloadAllocationTrack](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        downloadAllocationTrack[&gfxAllocation]++;
    };

    auto deviceMockNode = deviceTagAllocator.getTag();
    auto hostMockNode = hostTagAllocator.getTag();
    auto deviceSyncAllocation = deviceMockNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    auto hostSyncAllocation = hostMockNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(deviceMockNode, hostMockNode, *neoDevice, 1, false);
    *inOrderExecInfo->getBaseHostAddress(0) = 1;

    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event->getInOrderExecEventHelper().initializeLocalTempStorage();
    event->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    result = event->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, downloadAllocationTrack[deviceSyncAllocation]);
    EXPECT_EQ(0u, downloadAllocationTrack[hostSyncAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);

    auto event2 = zeUniquePtr(whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result)));

    event2->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    event2->getInOrderExecEventHelper().initializeLocalTempStorage();
    event2->updateInOrderExecState(inOrderExecInfo, 1, 0, 0);
    hostSyncAllocation->updateTaskCount(0u, ultCsr->getOsContext().getContextId());
    ultCsr->downloadAllocationsCalledCount = 0;
    eventAddress = static_cast<TagAddressType *>(event->getHostAddress());
    *eventAddress = Event::STATE_SIGNALED;

    result = event2->hostSynchronize(timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, downloadAllocationTrack[deviceSyncAllocation]);
    EXPECT_NE(0u, downloadAllocationTrack[hostSyncAllocation]);
    EXPECT_NE(0u, downloadAllocationTrack[hostSyncAllocation]);
    EXPECT_EQ(1u, ultCsr->downloadAllocationsCalledCount);
}

HWTEST_F(EventTests, GivenEventIsReadyToDownloadAllAllocationsWhenDownloadAllocationNotRequiredThenDontDownloadAllocations) {
    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::hardware;
    ze_result_t result;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));

    size_t offset = event->getCompletionFieldOffset();
    void *completionAddress = ptrOffset(event->hostAddressFromPool, offset);
    size_t packets = event->getPacketsInUse();
    uint64_t signaledValue = Event::STATE_SIGNALED;
    for (size_t i = 0; i < packets; i++) {
        memcpy(completionAddress, &signaledValue, sizeof(uint64_t));
        completionAddress = ptrOffset(completionAddress, event->getSinglePacketSize());
    }

    auto status = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
    EXPECT_EQ(0u, static_cast<UltCommandStreamReceiver<FamilyType> *>(event->csrs[0])->downloadAllocationsCalledCount);
    event->destroy();
}

HWTEST_F(EventTests, GivenNotReadyEventBecomesReadyWhenDownloadAllocationRequiredThenDownloadAllocationsOnce) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    CommandStreamReceiverType tbxCsrTypes[] = {CommandStreamReceiverType::tbx, CommandStreamReceiverType::tbxWithAub};
    ze_result_t result;
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(neoDevice->getUltCommandStreamReceiver<FamilyType>());
    for (auto csrType : tbxCsrTypes) {
        ultCsr.commandStreamReceiverType = csrType;
        auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));

        auto status = event->queryStatus(0);
        EXPECT_EQ(ZE_RESULT_NOT_READY, status);
        EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);

        size_t offset = event->getCompletionFieldOffset();
        void *completionAddress = ptrOffset(event->hostAddressFromPool, offset);
        size_t packets = event->getPacketsInUse();
        uint64_t signaledValue = Event::STATE_SIGNALED;
        for (size_t i = 0; i < packets; i++) {
            memcpy(completionAddress, &signaledValue, sizeof(uint64_t));
            completionAddress = ptrOffset(completionAddress, event->getSinglePacketSize());
        }

        status = event->queryStatus(0);
        EXPECT_EQ(ZE_RESULT_SUCCESS, status);
        EXPECT_EQ(1u, ultCsr.downloadAllocationsCalledCount);

        status = event->queryStatus(0);
        EXPECT_EQ(ZE_RESULT_SUCCESS, status);
        EXPECT_EQ(1u, ultCsr.downloadAllocationsCalledCount);

        event->destroy();
        ultCsr.downloadAllocationsCalledCount = 0;
    }
}
HWTEST_F(EventTests, GivenCsrTbxModeWhenEventCreatedAndSignaledThenEventAllocationIsWritten) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();
    auto mockMemIface = static_cast<NEO::MockMemoryOperations *>(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.get());

    constexpr uint32_t expectedBanks = std::numeric_limits<uint32_t>::max();

    mockMemIface->captureGfxAllocationsForMakeResident = true;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;

    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    EXPECT_EQ(0u, ultCsr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(0u, ultCsr.writeMemoryParams.chunkWriteCallCount);

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));
    auto eventAllocation = event->getAllocation(device);
    auto offsetInSharedAlloc = event->getOffsetInSharedAlloc();

    EXPECT_TRUE(eventAllocation->getAubInfo().writeMemoryOnly);

    auto eventAllocItor = std::find(mockMemIface->gfxAllocationsForMakeResident.begin(),
                                    mockMemIface->gfxAllocationsForMakeResident.end(),
                                    eventAllocation);
    EXPECT_EQ(mockMemIface->gfxAllocationsForMakeResident.end(), eventAllocItor);

    uint32_t expectedCallCount = std::min(static_cast<uint32_t>(eventPool->getEventSize() / sizeof(uint64_t)), uint32_t(16));

    EXPECT_EQ(2u, ultCsr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(1u, ultCsr.writeMemoryParams.chunkWriteCallCount);
    EXPECT_EQ(eventAllocation, ultCsr.writeMemoryParams.latestGfxAllocation);
    EXPECT_TRUE(ultCsr.writeMemoryParams.latestChunkedMode);
    EXPECT_EQ(sizeof(uint64_t) * expectedCallCount, ultCsr.writeMemoryParams.latestChunkSize);
    EXPECT_EQ(0u + offsetInSharedAlloc, ultCsr.writeMemoryParams.latestGpuVaChunkOffset);
    EXPECT_FALSE(eventAllocation->isTbxWritable(expectedBanks));

    auto status = event->hostSignal(false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);

    EXPECT_EQ(3u, ultCsr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(2u, ultCsr.writeMemoryParams.chunkWriteCallCount);
    EXPECT_EQ(eventAllocation, ultCsr.writeMemoryParams.latestGfxAllocation);
    EXPECT_TRUE(ultCsr.writeMemoryParams.latestChunkedMode);
    EXPECT_EQ(event->getSinglePacketSize(), ultCsr.writeMemoryParams.latestChunkSize);
    EXPECT_EQ(0u + offsetInSharedAlloc, ultCsr.writeMemoryParams.latestGpuVaChunkOffset);

    EXPECT_FALSE(eventAllocation->isTbxWritable(expectedBanks));

    std::bitset<32> singleBitMask;
    for (uint32_t i = 0; i < 32; i++) {
        singleBitMask.reset();
        singleBitMask.set(i, true);
        uint32_t bit = static_cast<uint32_t>(singleBitMask.to_ulong());
        EXPECT_FALSE(eventAllocation->isTbxWritable(bit));
    }

    event->reset();
    EXPECT_EQ(0, mockMemIface->makeResidentCalledCount);

    EXPECT_EQ(4u, ultCsr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(3u, ultCsr.writeMemoryParams.chunkWriteCallCount);
    EXPECT_EQ(eventAllocation, ultCsr.writeMemoryParams.latestGfxAllocation);
    EXPECT_TRUE(ultCsr.writeMemoryParams.latestChunkedMode);
    EXPECT_EQ(event->getSinglePacketSize(), ultCsr.writeMemoryParams.latestChunkSize);
    EXPECT_EQ(0u + offsetInSharedAlloc, ultCsr.writeMemoryParams.latestGpuVaChunkOffset);

    EXPECT_FALSE(eventAllocation->isTbxWritable(expectedBanks));

    size_t offset = event->getCompletionFieldOffset();
    void *completionAddress = ptrOffset(event->hostAddressFromPool, offset);
    size_t packets = event->getPacketsInUse();
    uint64_t signaledValue = Event::STATE_SIGNALED;
    for (size_t i = 0; i < packets; i++) {
        memcpy(completionAddress, &signaledValue, sizeof(uint64_t));
        completionAddress = ptrOffset(completionAddress, event->getSinglePacketSize());
    }

    status = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
    EXPECT_EQ(1u, ultCsr.downloadAllocationsCalledCount);

    event->destroy();
}

template <typename TagSizeT>
struct MockEventCompletion : public L0::EventImp<TagSizeT> {
    using BaseClass = L0::EventImp<TagSizeT>;
    using BaseClass::clearTimestampTagData;
    using BaseClass::csrs;
    using BaseClass::gpuEndTimestamp;
    using BaseClass::gpuStartTimestamp;
    using BaseClass::hostAddressFromPool;
    using BaseClass::maxPacketCount;
    using BaseClass::statusQueryAssignedCompletionData;

    MockEventCompletion(MultiGraphicsAllocation *alloc, uint32_t eventSize, uint32_t maxPacketsCount, int index, L0::Device *device) : BaseClass::EventImp(index, device, false) {
        auto neoDevice = device->getNEODevice();

        this->eventPoolAllocation = alloc;

        uint64_t baseHostAddr = reinterpret_cast<uint64_t>(alloc->getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex())->getUnderlyingBuffer());
        this->totalEventSize = eventSize;
        this->eventPoolOffset = index * this->totalEventSize;
        hostAddressFromPool = reinterpret_cast<void *>(baseHostAddr + this->eventPoolOffset);
        if (this->csrs.empty()) {
            this->csrs.push_back(neoDevice->getDefaultEngine().commandStreamReceiver);
        } else {
            this->csrs[0] = neoDevice->getDefaultEngine().commandStreamReceiver;
        }

        this->maxPacketCount = maxPacketsCount;
    }

    void assignKernelEventCompletionData(void *address) override {
        assignKernelEventCompletionDataCounter++;
    }

    ze_result_t hostEventSetValue(Event::State eventState) override {
        if (shouldHostEventSetValueFail) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return BaseClass::hostEventSetValue(eventState);
    }

    ze_result_t hostSynchronize(uint64_t timeout) override {
        hostSynchronizeCalled++;
        return BaseClass::hostSynchronize(timeout);
    }

    using BaseClass::queryStatus;
    ze_result_t queryStatus(int64_t timeSinceWait, bool blockOnMiss) override {
        if (failOnNextQueryStatus) {
            failOnNextQueryStatus = false;
            return ZE_RESULT_NOT_READY;
        }

        return BaseClass::queryStatus(timeSinceWait, blockOnMiss);
    }

    bool shouldHostEventSetValueFail = false;
    bool failOnNextQueryStatus = false;
    uint32_t assignKernelEventCompletionDataCounter = 0u;
    uint32_t hostSynchronizeCalled = 0;
};

using SimulationUploadTagType = NEO::TimestampPackets<uint32_t, NEO::TimestampPacketConstants::preferredPacketCount>;

struct MockTagNodeForSimulationUpload : public NEO::TagNode<SimulationUploadTagType> {
    MockTagNodeForSimulationUpload(NEO::MultiGraphicsAllocation *allocation, uint64_t gpuAddress, void *cpuBase) {
        this->gfxAllocation = allocation;
        this->gpuAddress = gpuAddress;
        this->tagForCpuAccess = reinterpret_cast<SimulationUploadTagType *>(cpuBase);
    }
};

TEST_F(EventTests, WhenQueryingStatusAfterHostSignalThenDontAccessMemoryAndReturnSuccess) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    auto result = event->hostSignal(false);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 0u);
}

TEST_F(EventTests, givenDebugFlagSetWhenCallingResetThenSynchronizeBeforeReset) {
    debugManager.flags.SynchronizeEventBeforeReset.set(1);

    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    event->failOnNextQueryStatus = true;

    *reinterpret_cast<uint32_t *>(event->hostAddressFromPool) = Event::STATE_SIGNALED;

    StreamCapture capture;
    capture.captureStdout();

    EXPECT_EQ(0u, event->hostSynchronizeCalled);

    event->reset();

    EXPECT_EQ(1u, event->hostSynchronizeCalled);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("");
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(EventTests, givenDebugFlagSetWhenCallingResetThenPrintLogAndSynchronizeBeforeReset) {
    debugManager.flags.SynchronizeEventBeforeReset.set(2);

    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    *reinterpret_cast<uint32_t *>(event->hostAddressFromPool) = Event::STATE_SIGNALED;

    {
        event->failOnNextQueryStatus = false;

        StreamCapture capture;
        capture.captureStdout();

        EXPECT_EQ(0u, event->hostSynchronizeCalled);

        event->reset();

        EXPECT_EQ(1u, event->hostSynchronizeCalled);

        std::string output = capture.getCapturedStdout();
        std::string expectedOutput("");
        EXPECT_EQ(expectedOutput, output);
    }

    {
        event->failOnNextQueryStatus = true;

        StreamCapture capture;
        capture.captureStdout();

        EXPECT_EQ(1u, event->hostSynchronizeCalled);

        event->reset();

        EXPECT_EQ(2u, event->hostSynchronizeCalled);

        std::string output = capture.getCapturedStdout();
        char expectedStr[128] = {};
        snprintf(expectedStr, 128, "\nzeEventHostReset: Event %p not ready. Calling zeEventHostSynchronize.", event.get());

        EXPECT_EQ(std::string(expectedStr), output);
    }
}

TEST_F(EventTests, whenAppendAdditionalCsrThenStoreUniqueCsr) {
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    auto csr2 = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    ze_result_t result;
    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));
    EXPECT_EQ(event->csrs.size(), 1u);

    event->appendAdditionalCsr(csr1.get());
    EXPECT_EQ(event->csrs.size(), 2u);

    event->appendAdditionalCsr(csr2.get());
    EXPECT_EQ(event->csrs.size(), 3u);

    event->appendAdditionalCsr(csr1.get());
    EXPECT_EQ(event->csrs.size(), 3u);

    event->destroy();
}

TEST_F(EventTests, WhenQueryingStatusAfterHostSignalThatFailedThenAccessMemoryAndReturnSuccess) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    event->shouldHostEventSetValueFail = true;
    event->hostSignal(false);
    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 1u);
}

HWTEST_F(EventTests, givenQwordPacketSizeWhenSignalingThenCopyQword) {
    using TimestampPacketType = typename FamilyType::TimestampPacketType;

    auto event = std::make_unique<MockEventCompletion<TimestampPacketType>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);

    auto completionAddress = static_cast<uint64_t *>(event->getCompletionFieldHostAddress());

    {
        event->setSinglePacketSize(sizeof(uint64_t));
        *completionAddress = std::numeric_limits<uint64_t>::max();

        event->hostSignal(false);

        EXPECT_EQ(static_cast<uint64_t>(Event::STATE_SIGNALED), *completionAddress);

        EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);
    }

    {
        event->setSinglePacketSize(NEO::TimestampPackets<TimestampPacketType, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());

        *completionAddress = std::numeric_limits<uint64_t>::max();

        event->hostSignal(false);

        if (sizeof(TimestampPacketType) == sizeof(uint32_t)) {
            uint64_t expectedValue = static_cast<uint64_t>(Event::STATE_SIGNALED);

            EXPECT_EQ(expectedValue, *completionAddress);
            EXPECT_EQ(static_cast<uint32_t>(Event::STATE_SIGNALED), *static_cast<uint32_t *>(event->getCompletionFieldHostAddress()));
        } else if (sizeof(TimestampPacketType) == sizeof(uint64_t)) {
            EXPECT_EQ(static_cast<uint64_t>(Event::STATE_SIGNALED), *completionAddress);
        } else {
            ASSERT_TRUE(false);
        }

        EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);
    }
}

TEST_F(EventTests, WhenQueryingStatusThenAccessMemoryOnce) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 1u);
}

TEST_F(EventTests, WhenQueryingStatusThenStatusQueryRecordsCompletionDataAssigned) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    EXPECT_FALSE(event->statusQueryAssignedCompletionData);
    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);
    EXPECT_TRUE(event->statusQueryAssignedCompletionData);
}

TEST_F(EventTests, GivenStatusQueryAssignedCompletionDataWhenQueryingKernelTimestampThenCompletionDataNotAssignedAgain) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    ze_kernel_timestamp_result_t result = {};
    EXPECT_EQ(event->queryKernelTimestamp(&result), ZE_RESULT_SUCCESS);
    EXPECT_TRUE(event->statusQueryAssignedCompletionData);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 1u);
}

TEST_F(EventTests, GivenHostSignaledWhenQueryingKernelTimestampThenStatusQueryRecordsCompletionDataNotAssigned) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    EXPECT_EQ(event->hostSignal(false), ZE_RESULT_SUCCESS);
    ze_kernel_timestamp_result_t result = {};
    EXPECT_EQ(event->queryKernelTimestamp(&result), ZE_RESULT_SUCCESS);
    EXPECT_FALSE(event->statusQueryAssignedCompletionData);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 1u);
}

TEST_F(EventTests, GivenNotReadyPacketsWhenQueryingStatusThenNotReadyReturnedAndCompletionDataAssignmentNotRecorded) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    const uint32_t clearedPacket[4] = {static_cast<uint32_t>(Event::STATE_CLEARED), static_cast<uint32_t>(Event::STATE_CLEARED),
                                       static_cast<uint32_t>(Event::STATE_CLEARED), static_cast<uint32_t>(Event::STATE_CLEARED)};
    event->kernelEventCompletionData.assignDataToAllTimestamps(0u, clearedPacket);

    EXPECT_FALSE(event->statusQueryAssignedCompletionData);
    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_NOT_READY);
    EXPECT_FALSE(event->statusQueryAssignedCompletionData);
}

TEST_F(EventTests, GivenNotReadyPacketsWhenQueryingStatusThenCoreIsNotYielded) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    const uint32_t clearedPacket[4] = {static_cast<uint32_t>(Event::STATE_CLEARED), static_cast<uint32_t>(Event::STATE_CLEARED),
                                       static_cast<uint32_t>(Event::STATE_CLEARED), static_cast<uint32_t>(Event::STATE_CLEARED)};
    event->kernelEventCompletionData.assignDataToAllTimestamps(0u, clearedPacket);

    const auto yieldCountBefore = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_NOT_READY);
    EXPECT_EQ(yieldCountBefore, CpuIntrinsicsTests::yieldCounter);
}

TEST_F(EventTests, GivenNotReadyPacketsWhenQueryingKernelTimestampThenNotReadyReturnedAndCompletionDataAssignmentNotRecorded) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    const uint32_t clearedPacket[4] = {static_cast<uint32_t>(Event::STATE_CLEARED), static_cast<uint32_t>(Event::STATE_CLEARED),
                                       static_cast<uint32_t>(Event::STATE_CLEARED), static_cast<uint32_t>(Event::STATE_CLEARED)};
    event->kernelEventCompletionData.assignDataToAllTimestamps(0u, clearedPacket);

    ze_kernel_timestamp_result_t result = {};
    EXPECT_EQ(event->queryKernelTimestamp(&result), ZE_RESULT_NOT_READY);
    EXPECT_FALSE(event->statusQueryAssignedCompletionData);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 1u);
}

TEST_F(EventTests, WhenQueryingStatusAfterResetThenAccessMemory) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->reset(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->queryStatus(0), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->assignKernelEventCompletionDataCounter, 2u);
}

TEST_F(EventTests, WhenResetEventThenZeroCpuTimestamps) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    event->gpuStartTimestamp = 10u;
    event->gpuEndTimestamp = 20u;
    EXPECT_EQ(event->reset(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(event->gpuStartTimestamp, 0u);
    EXPECT_EQ(event->gpuEndTimestamp, 0u);
}

HWTEST_F(EventTests, givenZeroPartitionCountWhenClearingTimestampTagDataThenSkipSimulationUpload) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    event->csrs.clear();
    event->csrs.push_back(&ultCsr);
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr.writeMemoryParams = {};

    uint8_t storage[4096] = {};
    NEO::MockGraphicsAllocation tagAllocation(storage, 0x1234000, sizeof(storage));
    NEO::MultiGraphicsAllocation multiAllocation(1);
    multiAllocation.addAllocation(&tagAllocation);
    MockTagNodeForSimulationUpload timestampNode(&multiAllocation, tagAllocation.getGpuAddress(), storage);

    event->clearTimestampTagData(0u, &timestampNode);

    EXPECT_EQ(0u, ultCsr.writeMemoryParams.totalCallCount);
}

HWTEST_F(EventTests, givenEmptyCsrListWhenClearingTimestampTagDataThenSkipSimulationUpload) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    event->csrs.clear();
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr.writeMemoryParams = {};

    uint8_t storage[4096] = {};
    NEO::MockGraphicsAllocation tagAllocation(storage, 0x1834000, sizeof(storage));
    NEO::MultiGraphicsAllocation multiAllocation(1);
    multiAllocation.addAllocation(&tagAllocation);
    MockTagNodeForSimulationUpload timestampNode(&multiAllocation, tagAllocation.getGpuAddress(), storage);

    event->clearTimestampTagData(2u, &timestampNode);

    EXPECT_EQ(0u, ultCsr.writeMemoryParams.totalCallCount);
}

HWTEST_F(EventTests, givenNonSimulationCsrWhenClearingTimestampTagDataThenSkipSimulationUpload) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    event->csrs.clear();
    event->csrs.push_back(&ultCsr);
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::hardware;
    ultCsr.writeMemoryParams = {};

    uint8_t storage[4096] = {};
    NEO::MockGraphicsAllocation tagAllocation(storage, 0x2234000, sizeof(storage));
    NEO::MultiGraphicsAllocation multiAllocation(1);
    multiAllocation.addAllocation(&tagAllocation);
    MockTagNodeForSimulationUpload timestampNode(&multiAllocation, tagAllocation.getGpuAddress(), storage);

    event->clearTimestampTagData(2u, &timestampNode);

    EXPECT_EQ(0u, ultCsr.writeMemoryParams.totalCallCount);
}

HWTEST_F(EventTests, givenAubCsrWhenClearingTimestampTagDataThenUploadEachPartitionChunk) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    event->csrs.clear();
    event->csrs.push_back(&ultCsr);
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    ultCsr.writeMemoryParams = {};

    uint8_t storage[4096] = {};
    NEO::MockGraphicsAllocation tagAllocation(storage, 0x2834000, sizeof(storage));
    NEO::MultiGraphicsAllocation multiAllocation(1);
    multiAllocation.addAllocation(&tagAllocation);
    MockTagNodeForSimulationUpload timestampNode(&multiAllocation, tagAllocation.getGpuAddress(), storage);

    constexpr uint32_t partitionCount = 2u;
    event->clearTimestampTagData(partitionCount, &timestampNode);

    EXPECT_EQ(partitionCount, ultCsr.writeMemoryParams.totalCallCount);
}

HWTEST_F(EventTests, givenAubCsrAnd64BitTagWhenClearingTimestampTagDataThenUploadEachPartitionChunk) {
    auto event = std::make_unique<MockEventCompletion<uint64_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    event->csrs.clear();
    event->csrs.push_back(&ultCsr);
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    ultCsr.writeMemoryParams = {};

    uint8_t storage[4096] = {};
    NEO::MockGraphicsAllocation tagAllocation(storage, 0x2c34000, sizeof(storage));
    NEO::MultiGraphicsAllocation multiAllocation(1);
    multiAllocation.addAllocation(&tagAllocation);
    MockTagNodeForSimulationUpload timestampNode(&multiAllocation, tagAllocation.getGpuAddress(), storage);

    constexpr uint32_t partitionCount = 2u;
    event->clearTimestampTagData(partitionCount, &timestampNode);

    EXPECT_EQ(partitionCount, ultCsr.writeMemoryParams.totalCallCount);
}

HWTEST_F(EventTests, Given64BitEventAndDrmKmdWaitStrategyWhenSignalAllPacketsUsesRemainingPacketThenHostSynchronizeFallsBackToPolling) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    csr.isWaitUserFenceNotEqualSupportedValue = true;
    csr.waitUserFenceParams.forceRetStatusEnabled = true;

    auto prepareEvent = [&]() {
        auto testedEvent = std::make_unique<MockEventCompletion<uint64_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
        testedEvent->maxPacketCount = 2u;
        testedEvent->setLinuxUserFenceKmdWaitEnabled(true);
        return testedEvent;
    };

    auto eventWithFirstPacketPending = prepareEvent();
    std::vector<uint64_t> firstPacketPendingStorage(alignUp(eventWithFirstPacketPending->getMaxPacketsCount() * eventWithFirstPacketPending->getSinglePacketSize() + sizeof(NEO::TimeStampData), sizeof(uint64_t)) / sizeof(uint64_t));
    eventWithFirstPacketPending->hostAddressFromPool = firstPacketPendingStorage.data();
    auto pendingFirstPacketAddress = static_cast<uint64_t *>(eventWithFirstPacketPending->getCompletionFieldHostAddress());
    *pendingFirstPacketAddress = Event::STATE_CLEARED;
    csr.waitUserFenceParams.forceRetStatusValue = true;
    csr.waitUserFenceParams.onWait = [&]() { *pendingFirstPacketAddress = Event::STATE_SIGNALED; };

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventWithFirstPacketPending->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(NEO::UserFenceValueWidth::u64, csr.waitUserFenceParams.latestWaitValueWidth);

    auto eventWithRemainingPacketPending = prepareEvent();
    std::vector<uint64_t> remainingPacketPendingStorage(alignUp(eventWithRemainingPacketPending->getMaxPacketsCount() * eventWithRemainingPacketPending->getSinglePacketSize() + sizeof(NEO::TimeStampData), sizeof(uint64_t)) / sizeof(uint64_t));
    eventWithRemainingPacketPending->hostAddressFromPool = remainingPacketPendingStorage.data();
    auto firstPacketAddress = static_cast<uint64_t *>(eventWithRemainingPacketPending->getCompletionFieldHostAddress());
    auto remainingPacketAddress = static_cast<uint64_t *>(ptrOffset(eventWithRemainingPacketPending->getHostAddress(), eventWithRemainingPacketPending->getSinglePacketSize() + eventWithRemainingPacketPending->getCompletionFieldOffset()));
    *firstPacketAddress = Event::STATE_SIGNALED;
    *remainingPacketAddress = Event::STATE_CLEARED;
    csr.waitUserFenceParams.callCountWithOperation = 0;
    csr.waitUserFenceParams.forceRetStatusValue = false;
    csr.waitUserFenceParams.onWait = [&]() { *remainingPacketAddress = Event::STATE_SIGNALED; };

    EXPECT_EQ(ZE_RESULT_SUCCESS, eventWithRemainingPacketPending->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, csr.waitUserFenceParams.callCountWithOperation);
    EXPECT_EQ(NEO::UserFenceValueWidth::u64, csr.waitUserFenceParams.latestWaitValueWidth);
    EXPECT_EQ(castToUint64(remainingPacketAddress), csr.waitUserFenceParams.latestWaitedAddress);
}

HWTEST_F(EventTests, GivenDrmAndKmdWaitStrategyWhenLinuxUserFenceKmdWaitIsDisabledThenUserFenceWaitIsNotUsed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;

    auto testedEvent = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    std::vector<uint64_t> eventStorage(alignUp(testedEvent->getMaxPacketsCount() * testedEvent->getSinglePacketSize() + sizeof(NEO::TimeStampData), sizeof(uint64_t)) / sizeof(uint64_t));
    testedEvent->hostAddressFromPool = eventStorage.data();
    testedEvent->setLinuxUserFenceKmdWaitEnabled(false);
    testedEvent->failOnNextQueryStatus = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, testedEvent->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCount);
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCountWithOperation);
}

HWTEST_F(EventTests, GivenDrmAndKmdWaitStrategyWhenEventUsesBcsThenUserFenceWaitIsNotUsed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    auto &osContext = reinterpret_cast<NEO::MockOsContext &>(csr.getOsContext());
    osContext.engineType = aub_stream::ENGINE_BCS;

    auto testedEvent = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    std::vector<uint64_t> eventStorage(alignUp(testedEvent->getMaxPacketsCount() * testedEvent->getSinglePacketSize() + sizeof(NEO::TimeStampData), sizeof(uint64_t)) / sizeof(uint64_t));
    testedEvent->hostAddressFromPool = eventStorage.data();
    testedEvent->setLinuxUserFenceKmdWaitEnabled(true);
    testedEvent->failOnNextQueryStatus = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, testedEvent->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCount);
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCountWithOperation);
}

HWTEST_F(EventTests, GivenDrmAndKmdWaitStrategyWhenLinuxUserFenceKmdWaitFlagIsDisabledThenUserFenceWaitIsNotUsed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(false);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;

    auto testedEvent = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    std::vector<uint64_t> eventStorage(alignUp(testedEvent->getMaxPacketsCount() * testedEvent->getSinglePacketSize() + sizeof(NEO::TimeStampData), sizeof(uint64_t)) / sizeof(uint64_t));
    testedEvent->hostAddressFromPool = eventStorage.data();
    testedEvent->setLinuxUserFenceKmdWaitEnabled(true);
    testedEvent->failOnNextQueryStatus = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, testedEvent->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCount);
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCountWithOperation);
}

HWTEST_F(EventTests, GivenDrmAndKmdWaitStrategyWhenPacketEventUsesMultiplePartitionsThenUserFenceWaitIsNotUsed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.set(3);
    NEO::debugManager.flags.EventHostSynchronizeLinuxUserFenceKmdWait.set(true);
    NEO::debugManager.flags.EventHostSynchronizeKmdWaitInitialPollMicroseconds.set(0);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.isUserFenceWaitSupported = true;
    csr.setActivePartitions(2);

    auto testedEvent = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    std::vector<uint64_t> eventStorage(alignUp(testedEvent->getMaxPacketsCount() * testedEvent->getSinglePacketSize() + sizeof(NEO::TimeStampData), sizeof(uint64_t)) / sizeof(uint64_t));
    testedEvent->hostAddressFromPool = eventStorage.data();
    testedEvent->setLinuxUserFenceKmdWaitEnabled(true);
    testedEvent->failOnNextQueryStatus = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, testedEvent->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCount);
    EXPECT_EQ(0u, csr.waitUserFenceParams.callCountWithOperation);
}

HWTEST_F(EventTests, givenSimulationCsrWhenClearingTimestampTagDataThenUploadEachPartitionChunk) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    event->csrs.clear();
    event->csrs.push_back(&ultCsr);
    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr.writeMemoryParams = {};

    uint8_t storage[4096] = {};
    NEO::MockGraphicsAllocation tagAllocation(storage, 0x3234000, sizeof(storage));
    NEO::MultiGraphicsAllocation multiAllocation(1);
    multiAllocation.addAllocation(&tagAllocation);
    MockTagNodeForSimulationUpload timestampNode(&multiAllocation, tagAllocation.getGpuAddress(), storage);

    constexpr uint32_t partitionCount = 2u;
    event->clearTimestampTagData(partitionCount, &timestampNode);

    EXPECT_EQ(partitionCount, ultCsr.writeMemoryParams.totalCallCount);
}

TEST_F(EventTests, WhenEventResetIsCalledThenPacketsUsedHaveNotBeenReset) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    event->gpuStartTimestamp = 10u;
    event->gpuEndTimestamp = 20u;
    event->setPacketsInUse(3u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->reset());

    EXPECT_EQ(3u, event->getPacketsInUse());
    EXPECT_EQ(event->gpuStartTimestamp, 0u);
    EXPECT_EQ(event->gpuEndTimestamp, 0u);
}

TEST_F(EventTests, GivenResetAllPacketsWhenResetPacketsThenOnePacketUsed) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    event->gpuStartTimestamp = 10u;
    event->gpuEndTimestamp = 20u;
    event->setPacketsInUse(3u);
    auto resetAllPackets = true;
    event->resetPackets(resetAllPackets);

    EXPECT_EQ(1u, event->getPacketsInUse());
    EXPECT_EQ(event->gpuStartTimestamp, 0u);
    EXPECT_EQ(event->gpuEndTimestamp, 0u);
}

TEST_F(EventTests, GivenResetAllPacketsFalseWhenResetPacketsThenPacketsUsedHaveNotBeenReset) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    event->gpuStartTimestamp = 10u;
    event->gpuEndTimestamp = 20u;
    event->setPacketsInUse(3u);
    auto resetAllPackets = false;
    event->resetPackets(resetAllPackets);

    EXPECT_EQ(3u, event->getPacketsInUse());
    EXPECT_EQ(event->gpuStartTimestamp, 0u);
    EXPECT_EQ(event->gpuEndTimestamp, 0u);
}

TEST_F(EventTests, givenCallToEventQueryStatusWithKernelPointerReturnsCounter) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    Mock<Module> mockModule(this->device, nullptr);
    std::shared_ptr<Mock<KernelImp>> mockKernel{new Mock<KernelImp>{}};
    mockKernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel->module = &mockModule;

    event->setKernelForPrintf(std::weak_ptr<Kernel>{mockKernel});
    event->setKernelWithPrintfDeviceMutex(&static_cast<Device *>(this->device)->printfKernelMutex);
    EXPECT_FALSE(event->getKernelForPrintf().expired());
    EXPECT_NE(nullptr, event->getKernelWithPrintfDeviceMutex());

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    event->hostSynchronize(timeout);
    EXPECT_EQ(1u, mockKernel->printPrintfOutputCalledTimes);
}

TEST_F(EventTests, givenCallToEventQueryStatusWithNullKernelPointerReturnsCounter) {
    auto event = std::make_unique<MockEventCompletion<uint32_t>>(&eventPool->getAllocation(), eventPool->getEventSize(), eventPool->getEventMaxPackets(), 1u, device);
    Mock<Module> mockModule(this->device, nullptr);
    std::shared_ptr<Mock<KernelImp>> mockKernel{new Mock<KernelImp>{}};
    mockKernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel->module = &mockModule;

    event->resetKernelForPrintf();
    EXPECT_TRUE(event->getKernelForPrintf().expired());

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    event->hostSynchronize(timeout);
    EXPECT_EQ(0u, mockKernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(nullptr, event->getKernelWithPrintfDeviceMutex());
}

TEST_F(EventSynchronizeTest, whenEventSetCsrThenCorrectCsrSet) {
    auto defaultCsr = neoDevice->getDefaultEngine().commandStreamReceiver;
    const auto mockCsr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());

    EXPECT_EQ(event->csrs[0], defaultCsr);
    event->setCsr(mockCsr.get(), false);
    EXPECT_EQ(event->csrs[0], mockCsr.get());

    event->reset();
    EXPECT_EQ(event->csrs[0], defaultCsr);
}

template <int32_t multiTile>
struct EventDynamicPacketUseFixture : public DeviceFixture {
    void setUp() {
        if constexpr (multiTile == 1) {
            debugManager.flags.CreateMultipleSubDevices.set(2);
            debugManager.flags.EnableImplicitScaling.set(1);
        }
        DeviceFixture::setUp();
    }

    void testAllDevices() {
        auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        ze_event_pool_desc_t eventPoolDesc = {
            ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
            nullptr,
            0,
            1};

        ze_result_t result = ZE_RESULT_SUCCESS;
        std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);

        auto eventPoolMaxPackets = eventPool->getEventMaxPackets();
        auto expectedPoolMaxPackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());
        if constexpr (multiTile == 1) {
            expectedPoolMaxPackets *= 2;
        }
        EXPECT_EQ(expectedPoolMaxPackets, eventPoolMaxPackets);

        auto eventSize = eventPool->getEventSize();
        auto expectedEventSize = static_cast<uint32_t>(alignUp(expectedPoolMaxPackets * gfxCoreHelper.getSingleTimestampPacketSize(), gfxCoreHelper.getTimestampPacketAllocatorAlignment()));
        EXPECT_EQ(expectedEventSize, eventSize);

        ze_event_desc_t eventDesc = {
            ZE_STRUCTURE_TYPE_EVENT_DESC,
            nullptr,
            0,
            ZE_EVENT_SCOPE_FLAG_DEVICE,
            ZE_EVENT_SCOPE_FLAG_DEVICE};

        std::unique_ptr<L0::Event> event(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));

        EXPECT_EQ(expectedPoolMaxPackets, event->getMaxPacketsCount());
    }

    void testSingleDevice() {
        ze_result_t result = ZE_RESULT_SUCCESS;

        device->getNEODevice()->getExecutionEnvironment()->setDeviceHierarchyMode(DeviceHierarchyMode::composite);

        auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        ze_event_pool_desc_t eventPoolDesc = {
            ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
            nullptr,
            0,
            1};

        std::vector<ze_device_handle_t> deviceHandles;

        L0::Device *eventDevice = device;
        if constexpr (multiTile == 1) {
            uint32_t count = 2;
            ze_device_handle_t subDevices[2];
            result = device->getSubDevices(&count, subDevices);
            ASSERT_EQ(ZE_RESULT_SUCCESS, result);
            deviceHandles.push_back(subDevices[0]);
            eventDevice = Device::fromHandle(subDevices[0]);
        } else {
            deviceHandles.push_back(device->toHandle());
        }

        std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 1, deviceHandles.data(), &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);

        auto eventPoolMaxPackets = eventPool->getEventMaxPackets();
        auto expectedPoolMaxPackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());

        EXPECT_EQ(expectedPoolMaxPackets, eventPoolMaxPackets);

        auto eventSize = eventPool->getEventSize();
        auto expectedEventSize = static_cast<uint32_t>(alignUp(expectedPoolMaxPackets * gfxCoreHelper.getSingleTimestampPacketSize(), gfxCoreHelper.getTimestampPacketAllocatorAlignment()));
        EXPECT_EQ(expectedEventSize, eventSize);

        ze_event_desc_t eventDesc = {
            ZE_STRUCTURE_TYPE_EVENT_DESC,
            nullptr,
            0,
            ZE_EVENT_SCOPE_FLAG_DEVICE,
            ZE_EVENT_SCOPE_FLAG_DEVICE};

        std::unique_ptr<L0::Event> event(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, eventDevice, result));

        EXPECT_EQ(expectedPoolMaxPackets, event->getMaxPacketsCount());
    }

    void testSignalAllPackets(uint32_t eventValueAfterSignal, uint32_t queryRetAfterPartialReset, ze_event_pool_flags_t flags) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

        ze_event_pool_desc_t eventPoolDesc = {
            ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
            nullptr,
            flags,
            1};

        std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);

        auto eventPoolMaxPackets = eventPool->getEventMaxPackets();
        auto expectedPoolMaxPackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());

        EXPECT_EQ(expectedPoolMaxPackets, eventPoolMaxPackets);

        ze_event_desc_t eventDesc = {
            ZE_STRUCTURE_TYPE_EVENT_DESC,
            nullptr,
            0,
            ZE_EVENT_SCOPE_FLAG_DEVICE,
            ZE_EVENT_SCOPE_FLAG_DEVICE};

        std::unique_ptr<L0::Event> event(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, result));
        EXPECT_EQ(expectedPoolMaxPackets, event->getMaxPacketsCount());

        // natively one packet is in use
        uint32_t usedPackets = event->getPacketsInUse();

        uint32_t maxPackets = event->getMaxPacketsCount();
        size_t packetSize = event->getSinglePacketSize();
        void *eventHostAddress = event->getHostAddress();
        uint32_t remainingPackets = maxPackets - usedPackets;
        void *remainingPacketsAddress = ptrOffset(eventHostAddress, (usedPackets * packetSize));
        if (event->isEventTimestampFlagSet()) {
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, event->getContextEndOffset());
        }

        for (uint32_t i = 0; i < remainingPackets; i++) {
            uint32_t *completionField = reinterpret_cast<uint32_t *>(remainingPacketsAddress);
            EXPECT_EQ(Event::STATE_INITIAL, *completionField);
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, packetSize);
        }

        result = event->queryStatus(0);
        EXPECT_EQ(ZE_RESULT_NOT_READY, result);
        event->resetCompletionStatus();

        event->hostSignal(false);

        remainingPacketsAddress = ptrOffset(eventHostAddress, (usedPackets * packetSize));
        if (event->isEventTimestampFlagSet()) {
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, event->getContextEndOffset());
        }

        for (uint32_t i = 0; i < remainingPackets; i++) {
            uint32_t *completionField = reinterpret_cast<uint32_t *>(remainingPacketsAddress);
            EXPECT_EQ(eventValueAfterSignal, *completionField);
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, packetSize);
        }

        result = event->queryStatus(0);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        event->resetCompletionStatus();

        remainingPacketsAddress = ptrOffset(eventHostAddress, (usedPackets * packetSize));
        if (event->isEventTimestampFlagSet()) {
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, event->getContextEndOffset());
        }

        {
            uint32_t *completionField = reinterpret_cast<uint32_t *>(remainingPacketsAddress);
            *completionField = Event::STATE_CLEARED;

            result = event->queryStatus(0);
            EXPECT_EQ(queryRetAfterPartialReset, result);
            event->resetCompletionStatus();

            *completionField = Event::STATE_SIGNALED;

            result = event->queryStatus(0);
            EXPECT_EQ(ZE_RESULT_SUCCESS, result);
            event->resetCompletionStatus();
        }

        event->reset();

        uint32_t eventValueAfterReset = Event::STATE_CLEARED;
        for (uint32_t i = 0; i < remainingPackets; i++) {
            uint32_t *completionField = reinterpret_cast<uint32_t *>(remainingPacketsAddress);

            // manually signaled free packet will not be cleared when signal all is not active
            eventValueAfterReset = (i == 0) ? Event::STATE_SIGNALED : Event::STATE_CLEARED;
            EXPECT_EQ(eventValueAfterReset, *completionField);
            remainingPacketsAddress = ptrOffset(remainingPacketsAddress, packetSize);
        }

        result = event->queryStatus(0);
        EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    }

    DebugManagerStateRestore restorer;
};

using EventDynamicPacketUseTest = Test<EventDynamicPacketUseFixture<0>>;
HWTEST_F(EventDynamicPacketUseTest, givenDynamicPacketEstimationWhenGettingMaxPacketFromAllDevicesThenMaxPossibleSelected) {
    testAllDevices();
}

HWTEST_F(EventDynamicPacketUseTest, givenDynamicPacketEstimationWhenGettingMaxPacketFromSingleDeviceThenMaxFromThisDeviceSelected) {
    testSingleDevice();
}

using EventMultiTileDynamicPacketUseTest = Test<EventDynamicPacketUseFixture<1>>;
HWTEST2_F(EventMultiTileDynamicPacketUseTest, givenDynamicPacketEstimationWhenGettingMaxPacketFromAllDevicesThenMaxPossibleSelected, IsAtLeastXeCore) {
    testAllDevices();
}

HWTEST2_F(EventMultiTileDynamicPacketUseTest, givenEventUsedCreatedOnSubDeviceButUsedOnDifferentSubdeviceWhenQueryingThenDownload, IsAtLeastXeCore) {
    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    auto rootDevice = static_cast<MockDeviceImp *>(device);

    ASSERT_TRUE(rootDevice->subDevices.size() > 1);

    auto subDevice0 = rootDevice->subDevices[0];
    auto subDevice1 = rootDevice->subDevices[1];

    auto rootCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(rootDevice->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto ultCsr0 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice0->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto ultCsr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice1->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto ultCsr2 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice1->getNEODevice()->getInternalEngine().commandStreamReceiver);

    rootCsr->initializeResources(device->getDevicePreemptionMode());
    ultCsr0->initializeResources(device->getDevicePreemptionMode());
    ultCsr1->initializeResources(device->getDevicePreemptionMode());

    rootCsr->commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr0->commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr1->commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr2->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    ultCsr2->resourcesInitialized = false;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto event = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, subDevice1, result));

    size_t eventCompletionOffset = event->getContextStartOffset();
    if (event->isEventTimestampFlagSet()) {
        eventCompletionOffset = event->getContextEndOffset();
    }
    TagAddressType *eventAddress = static_cast<TagAddressType *>(ptrOffset(event->getHostAddress(), eventCompletionOffset));
    *eventAddress = Event::STATE_INITIAL;

    uint32_t rootDownloadCounter = 0;
    uint32_t downloadCounter0 = 0;
    uint32_t downloadCounter1 = 0;
    uint32_t downloadCounter2 = 0;

    rootCsr->downloadAllocationImpl = [&rootDownloadCounter](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        rootDownloadCounter++;
    };
    ultCsr0->downloadAllocationImpl = [&downloadCounter0](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        downloadCounter0++;
    };
    ultCsr1->downloadAllocationImpl = [&downloadCounter1](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        downloadCounter1++;
    };
    ultCsr2->downloadAllocationImpl = [&downloadCounter2](GraphicsAllocation &gfxAllocation, uint64_t, size_t) {
        downloadCounter2++;
    };

    auto eventAllocation = event->getAllocation(device);
    ultCsr0->makeResident(*eventAllocation);
    ultCsr2->makeResident(*eventAllocation);
    rootCsr->makeResident(*eventAllocation);

    // all packets are signaled and waited on, so the whole event must be completed for
    // hostSynchronize to succeed and reach the download path under test
    for (uint32_t packetId = 0; packetId < event->getMaxPacketsCount(); packetId++) {
        auto packetAddress = static_cast<uint64_t *>(ptrOffset(event->getHostAddress(), packetId * event->getSinglePacketSize() + event->getCompletionFieldOffset()));
        *packetAddress = Event::STATE_SIGNALED;
    }

    event->hostSynchronize(1);

    EXPECT_EQ(1u, rootCsr->downloadAllocationsCalledCount);
    EXPECT_FALSE(rootCsr->latestDownloadAllocationsBlocking);
    EXPECT_EQ(0u, rootDownloadCounter);

    EXPECT_EQ(1u, ultCsr0->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr0->latestDownloadAllocationsBlocking);
    EXPECT_EQ(1u, downloadCounter0);

    EXPECT_EQ(1u, ultCsr1->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr1->latestDownloadAllocationsBlocking);
    EXPECT_EQ(0u, downloadCounter1);

    EXPECT_EQ(0u, ultCsr2->downloadAllocationsCalledCount);
    EXPECT_EQ(0u, downloadCounter2);

    event->destroy();
}

HWTEST2_F(EventMultiTileDynamicPacketUseTest, givenEventCounterBasedUsedCreatedOnSubDeviceButUsedOnDifferentSubdeviceWhenQueryingThenDownload, IsAtLeastXeCore) {
    neoDevice->getExecutionEnvironment()->calculateMaxOsContextCount();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();

    auto rootDevice = static_cast<MockDeviceImp *>(device);

    ASSERT_TRUE(rootDevice->subDevices.size() > 1);

    auto subDevice0 = rootDevice->subDevices[0];
    auto subDevice1 = rootDevice->subDevices[1];

    auto ultCsr0 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice0->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    auto ultCsr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(subDevice1->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ultCsr0->initializeResources(device->getDevicePreemptionMode());
    ultCsr1->initializeResources(device->getDevicePreemptionMode());

    ultCsr0->commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ultCsr1->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 3;
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto event0 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, subDevice1, result));
    auto event1 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, subDevice1, result));
    auto event2 = whiteboxCast(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, subDevice1, result));
    event0->eventPoolAllocation = nullptr;
    event1->eventPoolAllocation = nullptr;
    event2->eventPoolAllocation = nullptr;

    auto inOrderExecInfo0 = NEO::InOrderExecInfo::create(device->getDeviceInOrderCounterAllocator()->getTag(), nullptr, *device->getNEODevice(), 1);
    inOrderExecInfo0->setLastWaitedCounterValue(1, 0);
    event0->getInOrderExecEventHelper().initializeLocalTempStorage();
    event0->updateInOrderExecState(inOrderExecInfo0, 1, 0, 0);

    uint64_t counter = 2;

    event1->getInOrderExecEventHelper().initializeLocalTempStorage();
    event1->getInOrderExecEventHelper().assignData(1, 0, 1, 1, nullptr, nullptr, 1, 0, &counter, 0, 0, false, true);

    MockGraphicsAllocation mockAlloc(rootDeviceIndex, nullptr, 1);

    event2->getInOrderExecEventHelper().initializeLocalTempStorage();
    event2->getInOrderExecEventHelper().assignData(1, 0, 1, 1, &mockAlloc, &mockAlloc, 1, 0, &counter, 0, 0, false, true);

    ultCsr0->makeResident(*inOrderExecInfo0->getDeviceCounterAllocation());

    event0->hostSynchronize(1);

    EXPECT_EQ(1u, ultCsr0->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr0->latestDownloadAllocationsBlocking);

    EXPECT_EQ(1u, ultCsr1->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr1->latestDownloadAllocationsBlocking);

    event1->hostSynchronize(1);

    EXPECT_EQ(2u, ultCsr0->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr0->latestDownloadAllocationsBlocking);

    EXPECT_EQ(3u, ultCsr1->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr1->latestDownloadAllocationsBlocking);

    event2->hostSynchronize(1);

    EXPECT_EQ(2u, ultCsr0->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr0->latestDownloadAllocationsBlocking);

    EXPECT_EQ(4u, ultCsr1->downloadAllocationsCalledCount);
    EXPECT_FALSE(ultCsr1->latestDownloadAllocationsBlocking);

    event0->destroy();
    event1->destroy();
    event2->destroy();
}

HWTEST2_F(EventMultiTileDynamicPacketUseTest, givenDynamicPacketEstimationWhenGettingMaxPacketFromSingleOneTileDeviceThenMaxFromThisDeviceSelected, IsAtLeastXeCore) {
    testSingleDevice();
}

using EventSignalUsedPacketsTest = Test<EventDynamicPacketUseFixture<0>>;
HWTEST2_F(EventSignalUsedPacketsTest, givenDynamicPacketEstimationWhenImmediateSignalUsedPacketThenUsedPacketCompletionSignaled, IsAtLeastXeCore) {
    testSignalAllPackets(Event::STATE_CLEARED, ZE_RESULT_SUCCESS, 0);
}

HWTEST2_F(EventSignalUsedPacketsTest, givenDynamicPacketEstimationWhenTimestampSignalUsedPacketThenUsedPacketCompletionSignaled, IsAtLeastXeCore) {
    testSignalAllPackets(Event::STATE_CLEARED, ZE_RESULT_SUCCESS, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

void testQueryAllPackets(L0::Event *event) {
    auto result = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    uint32_t usedPackets = event->getPacketsInUse();

    uint32_t maxPackets = event->getMaxPacketsCount();
    size_t packetSize = event->getSinglePacketSize();
    void *firstPacketAddress = event->getCompletionFieldHostAddress();
    void *eventHostAddress = firstPacketAddress;
    for (uint32_t i = 0; i < usedPackets; i++) {
        uint32_t *completionField = reinterpret_cast<uint32_t *>(eventHostAddress);
        EXPECT_EQ(Event::STATE_INITIAL, *completionField);
        *completionField = Event::STATE_SIGNALED;
        eventHostAddress = ptrOffset(eventHostAddress, packetSize);
    }

    EXPECT_EQ(maxPackets, usedPackets);

    result = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = event->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventHostAddress = firstPacketAddress;
    for (uint32_t i = 0; i < maxPackets; i++) {
        uint32_t *completionField = reinterpret_cast<uint32_t *>(eventHostAddress);
        EXPECT_EQ(Event::STATE_INITIAL, *completionField);
        eventHostAddress = ptrOffset(eventHostAddress, packetSize);
    }

    result = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_NOT_READY, result);

    result = event->hostSignal(false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventHostAddress = firstPacketAddress;
    for (uint32_t i = 0; i < maxPackets; i++) {
        uint32_t *completionField = reinterpret_cast<uint32_t *>(eventHostAddress);
        EXPECT_EQ(Event::STATE_SIGNALED, *completionField);
        eventHostAddress = ptrOffset(eventHostAddress, packetSize);
    }

    result = event->queryStatus(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using TimestampEventAllPacketSignalSinglePacketUseTest = Test<EventCompactL3FlushFixture<1, 1, 1>>;
TEST_F(TimestampEventAllPacketSignalSinglePacketUseTest, givenSignalAllEventPacketWhenQueryingAndSignalingTimestampEventThenUseEventMaxPackets) {
    testQueryAllPackets(event.get());
}

using ImmediateEventAllPacketSignalSinglePacketUseTest = Test<EventCompactL3FlushFixture<1, 0, 1>>;
TEST_F(ImmediateEventAllPacketSignalSinglePacketUseTest, givenSignalAllEventPacketWhenQueryingAndSignalingImmediateEventThenUseEventMaxPackets) {
    testQueryAllPackets(event.get());
}

struct LocalMemoryEnabledDeviceFixture : public DeviceFixture {
    void setUp() {
        debugManager.flags.EnableLocalMemory.set(1);
        DeviceFixture::setUp();
    }
    void tearDown() {
        DeviceFixture::tearDown();
    }
    DebugManagerStateRestore restore;
};

using EventTimestampTest = Test<LocalMemoryEnabledDeviceFixture>;
HWTEST2_F(EventTimestampTest, givenAppendMemoryCopyIsCalledWhenCpuCopyIsUsedAndCopyTimeIsLessThanDeviceTimestampResolutionThenReturnTimstampDifferenceAsOne, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExperimentalForceCopyThroughLock.set(1);
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    neoDevice->setOSTime(new NEO::MockOSTimeWithConstTimestamp());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<L0::Event>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device, returnValue));
    constexpr uint32_t copySize = 1024;
    auto hostPtr = new char[copySize];
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *devicePtr;
    context->allocDeviceMem(device->toHandle(), &deviceDesc, copySize, 1u, &devicePtr);
    CmdListMemoryCopyParams copyParams = {};
    cmdList.appendMemoryCopy(devicePtr, hostPtr, copySize, event->toHandle(), 0, nullptr, copyParams);

    ze_kernel_timestamp_result_t result = {};
    event->queryKernelTimestamp(&result);
    EXPECT_EQ(result.context.kernelEnd - result.context.kernelStart, 1u);

    delete[] hostPtr;
    context->freeMem(devicePtr);
}

TEST_F(EventTests, givenDefaultDescriptorWhenCreatingCounterBasedEventThenEventWithNoProfilingAndSignalScopeHostAndDeviceScopeWaitIsCreated) {
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &defaultIntelCounterBasedEventDesc, &handle));

    auto eventObj = Event::fromHandle(handle);
    EXPECT_TRUE(eventObj->isCounterBasedExplicitlyEnabled());
    EXPECT_FALSE(eventObj->isIpcImported());
    EXPECT_FALSE(eventObj->isEventTimestampFlagSet());
    EXPECT_TRUE(eventObj->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));
    EXPECT_TRUE(eventObj->isWaitScope(ZE_EVENT_SCOPE_FLAG_DEVICE));
    EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_HOST_VISIBLE), eventObj->getCounterBasedFlags());
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenNullDescriptorWhenCreatingDeprecatedCbEvent2ThenEventWithNoProfilingAndSignalScopeHostAndDeviceScopeWaitIsCreated) {
    ze_event_handle_t handle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCounterBasedEventCreate2(context, device, nullptr, &handle));

    auto eventObj = Event::fromHandle(handle);
    EXPECT_TRUE(eventObj->isCounterBasedExplicitlyEnabled());
    EXPECT_FALSE(eventObj->isIpcImported());
    EXPECT_FALSE(eventObj->isEventTimestampFlagSet());
    EXPECT_TRUE(eventObj->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));
    EXPECT_TRUE(eventObj->isWaitScope(ZE_EVENT_SCOPE_FLAG_DEVICE));
    EXPECT_EQ(static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_HOST_VISIBLE), eventObj->getCounterBasedFlags());
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenDescriptorWithExternalFlagWhenCreatingCounterBasedEventThenEventWithExternalFlagIsCreated) {
    ze_event_handle_t handle = nullptr;
    ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &desc, &handle));

    auto eventObj = Event::fromHandle(handle);
    EXPECT_TRUE(eventObj->isCounterBasedExplicitlyEnabled());
    EXPECT_TRUE(eventObj->isExternalEvent());
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenHostTimestampFlagWhenCreatingCounterBasedEventThenTimestampStorageIsAllocated) {
    ze_event_handle_t handle = nullptr;
    ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &desc, &handle));

    auto eventObj = Event::fromHandle(handle);
    EXPECT_TRUE(eventObj->isEventTimestampFlagSet());
    EXPECT_TRUE(eventObj->hasKernelMappedTsCapability);
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenDeviceTimestampFlagWhenCreatingCounterBasedEventThenTimestampStorageIsAllocatedWithoutMappedCapability) {
    ze_event_handle_t handle = nullptr;
    ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &desc, &handle));

    auto eventObj = Event::fromHandle(handle);
    EXPECT_TRUE(eventObj->isEventTimestampFlagSet());
    EXPECT_FALSE(eventObj->hasKernelMappedTsCapability);
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenNoTimestampFlagsWhenCreatingCounterBasedEventThenTimestampStorageIsNotAllocated) {
    ze_event_handle_t handle = nullptr;
    ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &desc, &handle));

    auto eventObj = Event::fromHandle(handle);
    EXPECT_FALSE(eventObj->isEventTimestampFlagSet());
    EXPECT_FALSE(eventObj->hasKernelMappedTsCapability);
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenCounterBasedEventCreatedWithAdditionalFlagsWhenQueryingCounterBasedFlagsThenAllCreationFlagsAreReturned) {
    ze_event_handle_t handle = nullptr;
    ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_HOST_VISIBLE | ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &desc, &handle));

    ze_event_counter_based_flags_t flags = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, Event::fromHandle(handle)->getCounterBasedFlags(&flags));
    EXPECT_EQ(static_cast<uint32_t>(desc.flags), flags);
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenCounterBasedEventCreatedWithTimestampAndExternalFlagsWhenQueryingCounterBasedFlagsThenAllCreationFlagsAreReturned) {
    ze_event_handle_t handle = nullptr;
    ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &desc, &handle));

    ze_event_counter_based_flags_t flags = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, Event::fromHandle(handle)->getCounterBasedFlags(&flags));
    EXPECT_EQ(static_cast<uint32_t>(desc.flags), flags);
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenNoImmediateFlagsWhenCreatingCounterBasedEventThenImmediateIsAddedToReportedFlags) {
    ze_event_handle_t handle = nullptr;
    ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_HOST_VISIBLE;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &desc, &handle));

    ze_event_counter_based_flags_t flags = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, Event::fromHandle(handle)->getCounterBasedFlags(&flags));
    EXPECT_EQ(static_cast<uint32_t>(ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_HOST_VISIBLE), flags);
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenUndefinedFlagWhenCreatingCounterBasedEventThenReturnInvalidEnumeration) {
    constexpr uint32_t firstUndefinedFlag = ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL << 1;

    ze_event_handle_t handle = nullptr;
    ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | firstUndefinedFlag;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, zeEventCounterBasedCreate(context, device, &desc, &handle));
    EXPECT_EQ(nullptr, handle);

    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &desc, &handle));
    zeEventDestroy(handle);
}

TEST_F(EventTests, givenBothTimestampFlagsWhenCreatingCounterBasedEventThenReturnError) {
    ze_event_handle_t handle = nullptr;
    ze_event_counter_based_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP | ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedCreate(context, device, &desc, &handle));
    EXPECT_EQ(nullptr, handle);
}

TEST_F(EventTests, givenInvalidExtensionArgumentWhenCreatingEventThenDoNotAbortAndReturnErrorCode) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;
    ze_result_t returnValue;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ze_event_counter_based_external_sync_allocation_desc_t externalSyncAllocProperties = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
    ze_event_desc_t eventDesc = {};
    eventDesc.pNext = &externalSyncAllocProperties;

    auto event0 = Event::create<uint32_t>(eventPool.get(), &eventDesc, device, returnValue);
    EXPECT_EQ(event0, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    ze_event_counter_based_external_aggregate_storage_desc_t externalStorageAllocProperties = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_AGGREGATE_STORAGE_DESC};
    eventDesc.pNext = &externalStorageAllocProperties;

    auto event1 = Event::create<uint32_t>(eventPool.get(), &eventDesc, device, returnValue);
    EXPECT_EQ(event1, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST_F(EventTests, givenCounterBasedEventMaxValueQueryWhenCallingApiThenReturnLimitMatchingInOrderCounterWidth) {
    uint64_t maxValue = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetCounterBasedEventMaxValue(device->toHandle(), &maxValue));

    auto &l0GfxCoreHelper = device->getL0GfxCoreHelper();

    if constexpr (FamilyType::isQwordInOrderCounter) {
        EXPECT_EQ(std::numeric_limits<uint64_t>::max(), maxValue);
    } else {
        EXPECT_EQ(static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()), maxValue);
    }

    EXPECT_EQ(l0GfxCoreHelper.getCounterBasedEventMaxValue(), maxValue);
    EXPECT_EQ(sizeof(uint64_t) == l0GfxCoreHelper.getCmdListWaitOnMemoryDataSize(), maxValue == std::numeric_limits<uint64_t>::max());
}

TEST_F(EventTests, givenNullArgumentsWhenQueryingCounterBasedEventMaxValueThenReturnError) {
    uint64_t maxValue = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, zeDeviceGetCounterBasedEventMaxValue(nullptr, &maxValue));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, zeDeviceGetCounterBasedEventMaxValue(device->toHandle(), nullptr));
}

TEST_F(EventTests, givenCompletionValueExceedingMaxValueWhenCreatingCbEventWithExternalSyncAllocThenReturnError) {
    const uint64_t maxValue = device->getL0GfxCoreHelper().getCounterBasedEventMaxValue();
    if (maxValue == std::numeric_limits<uint64_t>::max()) {
        GTEST_SKIP();
    }

    uint64_t counter = 0;

    ze_event_counter_based_external_sync_allocation_desc_t externalSyncAllocProperties = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC};
    externalSyncAllocProperties.deviceAddress = &counter;
    externalSyncAllocProperties.hostAddress = &counter;
    externalSyncAllocProperties.completionValue = maxValue + 1;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalSyncAllocProperties;

    ze_event_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));
    EXPECT_EQ(nullptr, handle);
}

TEST_F(EventTests, givenCompletionValueExceedingMaxValueWhenCreatingCbEventWithExternalStorageThenReturnErrorAndAcceptValueAtLimit) {
    const uint64_t maxValue = device->getL0GfxCoreHelper().getCounterBasedEventMaxValue();
    if (maxValue == std::numeric_limits<uint64_t>::max()) {
        GTEST_SKIP();
    }

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, sizeof(uint64_t), 4096u, &deviceAlloc));

    ze_event_counter_based_external_aggregate_storage_desc_t externalStorageAllocProperties = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_AGGREGATE_STORAGE_DESC};
    externalStorageAllocProperties.deviceAddress = reinterpret_cast<uint64_t *>(deviceAlloc);
    externalStorageAllocProperties.incrementValue = 1;
    externalStorageAllocProperties.completionValue = maxValue + 1;

    ze_event_counter_based_desc_t counterBasedDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    counterBasedDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
    counterBasedDesc.pNext = &externalStorageAllocProperties;

    ze_event_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));
    EXPECT_EQ(nullptr, handle);

    externalStorageAllocProperties.completionValue = maxValue;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context, device, &counterBasedDesc, &handle));
    ASSERT_NE(nullptr, handle);
    zeEventDestroy(handle);

    context->freeMem(deviceAlloc);
}

TEST_F(EventTests, givenNotCounterBasedEventWhenCallingGetCounterBasedFlagsThenReturnSuccessAndFlagsAreZero) {
    ze_event_counter_based_flags_t flags{};
    auto result = event->getCounterBasedFlags(&flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, flags);
    EXPECT_FALSE(event->isCounterBased());
}

TEST_F(EventTests, givenCounterBasedEventWhenCallingGetCounterBasedFlagsThenReturnSuccessAndCorrectFlags) {
    event->enableCounterBasedMode(true, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    ze_event_counter_based_flags_t flags{};
    auto result = event->getCounterBasedFlags(&flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE, flags);
    EXPECT_TRUE(event->isCounterBased());
}

TEST_F(EventTests, givenImplicitlyEnabledCounterBasedEventWhenResetThenModeReturnsToInitialAndCanBeEnabledAgain) {
    ze_event_counter_based_flags_t flags{};

    event->enableCounterBasedMode(false, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    EXPECT_TRUE(event->isCounterBased());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->getCounterBasedFlags(&flags));
    EXPECT_EQ(ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE, flags);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->reset());
    EXPECT_FALSE(event->isCounterBased());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->getCounterBasedFlags(&flags));
    EXPECT_EQ(0u, flags);

    event->enableCounterBasedMode(false, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
    EXPECT_TRUE(event->isCounterBased());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->getCounterBasedFlags(&flags));
    EXPECT_EQ(ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE, flags);
}

using CounterBasedEventTests = Test<CounterBasedEventFixture>;
TEST_F(CounterBasedEventTests, givenEventCreatedWithCounterBasedEventPoolAndCallingGetCounterBasedEventFlagsThenEventIsCounterBasedAndCorrectFlagsReturned) {
    EXPECT_TRUE(event->isCounterBased());
    ze_event_counter_based_flags_t counterBasedEventFlags{};
    auto result = event->getCounterBasedFlags(&counterBasedEventFlags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    constexpr uint32_t counterBasedEventPoolFlag = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE;
    EXPECT_EQ(counterBasedEventPoolFlag, counterBasedEventFlags);
}

TEST_F(CounterBasedEventTests, givenCbEventWithApiRequiredExternalFlagSetWhenSettingGraphInternalThenEventIsNotSetAsInternal) {
    EXPECT_TRUE(event->isCounterBased());
    event->setApiRequiredGraphExternalEvent(true);
    EXPECT_TRUE(event->getApiRequiredGraphExternalEvent());
    event->setIsSignalledAsGraphInternalEvent(true);
    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
}

using EventTemporaryAllocationCleanupTest = Test<DeviceFixture>;

HWTEST_F(EventTemporaryAllocationCleanupTest,
         givenRecordedCleanupTaskCountWhenHandleSuccessfulHostSynchronizationThenTemporaryAllocationsUpToEventCompletionAreFreedAndLaterOnesAreKept) {
    struct MockCleanupEvent : public L0::EventImp<uint32_t> {
        MockCleanupEvent(int index, L0::Device *device) : EventImp(index, device, false) {}
        using L0::EventImp<uint32_t>::handleSuccessfulHostSynchronization;
    };

    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    const auto contextId = csr->getOsContext().getContextId();
    auto memoryManager = neoDevice->getMemoryManager();

    constexpr TaskCountType eventTaskCount = 10u;

    auto mockEvent = std::make_unique<MockCleanupEvent>(0, device);
    mockEvent->setCsr(csr, true);
    mockEvent->setCleanupTaskCount(csr, eventTaskCount);

    // Simulate a CSR tag that lags the event's real completion. The fix must clean based on the
    // recorded event completion, not this stale live value, so a stale tag must not affect the result.
    *csr->getTagAddress() = eventTaskCount - 1u;

    auto &temporaryAllocations = MockAllocationsList::whiteboxCast(memoryManager->getTemporaryAllocationsList());

    // Pin whose work is covered by the synchronized event (task count == event's) -> must be freed.
    auto completedAllocation = memoryManager->allocateGraphicsMemoryWithProperties(
        NEO::MockAllocationProperties{neoDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    completedAllocation->setHostPtrTaskCountAssignment(0);
    completedAllocation->updateTaskCount(eventTaskCount, contextId);
    csr->getInternalAllocationStorage()->storeAllocationWithTaskCount(
        std::unique_ptr<NEO::GraphicsAllocation>(completedAllocation), NEO::AllocationUsage::TEMPORARY_ALLOCATION, eventTaskCount);

    // Later, still in-flight pin (task count beyond the event's completion) -> must be kept.
    auto laterAllocation = memoryManager->allocateGraphicsMemoryWithProperties(
        NEO::MockAllocationProperties{neoDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    laterAllocation->setHostPtrTaskCountAssignment(0);
    laterAllocation->updateTaskCount(eventTaskCount + 2u, contextId);
    csr->getInternalAllocationStorage()->storeAllocationWithTaskCount(
        std::unique_ptr<NEO::GraphicsAllocation>(laterAllocation), NEO::AllocationUsage::TEMPORARY_ALLOCATION, eventTaskCount + 2u);

    ASSERT_EQ(2u, temporaryAllocations.peekSize());

    mockEvent->handleSuccessfulHostSynchronization();

    ASSERT_EQ(1u, temporaryAllocations.peekSize());
    EXPECT_EQ(laterAllocation, temporaryAllocations.peekFront());

    csr->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(neoDevice);
}

HWTEST_F(EventTemporaryAllocationCleanupTest, givenNoTemporaryAllocationsWhenHandleSuccessfulHostSynchronizationThenTemporaryAllocationListIsNotCleaned) {
    struct MockCleanupEvent : public L0::EventImp<uint32_t> {
        MockCleanupEvent(int index, L0::Device *device) : EventImp(index, device, false) {}
        using L0::EventImp<uint32_t>::handleSuccessfulHostSynchronization;
    };

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto storage = new MockInternalAllocationStorage(csr);
    csr.internalAllocationStorage.reset(storage);
    auto memoryManager = neoDevice->getMemoryManager();

    auto mockEvent = std::make_unique<MockCleanupEvent>(0, device);
    mockEvent->setCsr(&csr, true);

    ASSERT_TRUE(storage->getTemporaryAllocations().peekIsEmpty());
    mockEvent->handleSuccessfulHostSynchronization();
    EXPECT_EQ(0u, storage->cleanAllocationsCalled);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(
        NEO::MockAllocationProperties{neoDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    storage->storeAllocationWithTaskCount(std::unique_ptr<NEO::GraphicsAllocation>(allocation), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1u);

    mockEvent->handleSuccessfulHostSynchronization();
    EXPECT_EQ(1u, storage->cleanAllocationsCalled);

    storage->getTemporaryAllocations().freeAllGraphicsAllocations(neoDevice);
}

} // namespace ult
} // namespace L0
