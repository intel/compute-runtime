/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <memory>

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
}

namespace NEO {
namespace SysCalls {
extern bool failMmap;
}
} // namespace NEO

struct DrmDirectSubmissionTest : public DrmMemoryManagerBasic {
    void SetUp() override {
        DrmMemoryManagerBasic::SetUp();
        executionEnvironment.incRefInternal();

        executionEnvironment.memoryManager = std::make_unique<DrmMemoryManager>(GemCloseWorkerMode::gemCloseWorkerInactive,
                                                                                debugManager.flags.EnableForcePin.get(),
                                                                                true,
                                                                                executionEnvironment);
        device.reset(MockDevice::create<MockDevice>(&executionEnvironment, 0u));
        executionEnvironment.rootDeviceEnvironments[0]->initReleaseHelper();
        osContext = std::make_unique<OsContextLinux>(*executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>(), device->getRootDeviceIndex(), 0u,
                                                     EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                  PreemptionMode::ThreadGroup, device->getDeviceBitfield()));
        osContext->ensureContextInitialized(false);
        device->getDefaultEngine().commandStreamReceiver->setupContext(*osContext);
    }

    void TearDown() override {
        DrmMemoryManagerBasic::TearDown();
    }

    std::unique_ptr<OsContextLinux> osContext;
    std::unique_ptr<MockDevice> device;
};

template <typename GfxFamily, typename Dispatcher>
struct MockDrmDirectSubmission : public DrmDirectSubmission<GfxFamily, Dispatcher> {
    using BaseClass = DrmDirectSubmission<GfxFamily, Dispatcher>;
    using BaseClass::activeTiles;
    using BaseClass::allocateResources;
    using BaseClass::completionFenceAllocation;
    using BaseClass::completionFenceValue;
    using BaseClass::currentRingBuffer;
    using BaseClass::currentTagData;
    using BaseClass::disableMonitorFence;
    using BaseClass::dispatchMonitorFenceRequired;
    using BaseClass::dispatchSwitchRingBufferSection;
    using BaseClass::DrmDirectSubmission;
    using BaseClass::getSizeDisablePrefetcher;
    using BaseClass::getSizeDispatchRelaxedOrderingQueueStall;
    using BaseClass::getSizeEnd;
    using BaseClass::getSizeNewResourceHandler;
    using BaseClass::getSizePrefetchMitigation;
    using BaseClass::getSizeSemaphoreSection;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::getTagAddressValue;
    using BaseClass::gpuHangCheckPeriod;
    using BaseClass::handleNewResourcesSubmission;
    using BaseClass::handleResidency;
    using BaseClass::handleSwitchRingBuffers;
    using BaseClass::immWritePostSyncOffset;
    using BaseClass::inputMonitorFenceDispatchRequirement;
    using BaseClass::isCompleted;
    using BaseClass::isDisablePrefetcherRequired;
    using BaseClass::isNewResourceHandleNeeded;
    using BaseClass::lastUllsLightExecTimestamp;
    using BaseClass::miMemFenceRequired;
    using BaseClass::osContext;
    using BaseClass::partitionConfigSet;
    using BaseClass::partitionedMode;
    using BaseClass::pciBarrierPtr;
    using BaseClass::relaxedOrderingEnabled;
    using BaseClass::ringBuffers;
    using BaseClass::ringStart;
    using BaseClass::rootDeviceEnvironment;
    using BaseClass::sfenceMode;
    using BaseClass::submit;
    using BaseClass::switchRingBuffers;
    using BaseClass::tagAddress;
    using BaseClass::updateTagValue;
    using BaseClass::wait;
    using BaseClass::workPartitionAllocation;

    MockDrmDirectSubmission(const DirectSubmissionInputParams &inputParams) : BaseClass(inputParams) {
        this->lastUllsLightExecTimestamp = std::chrono::time_point<std::chrono::steady_clock>::max();
    }

    std::chrono::steady_clock::time_point getCpuTimePoint() override {
        return this->callBaseGetCpuTimePoint ? BaseClass::getCpuTimePoint() : cpuTimePointReturnValue;
    }
    void getTagAddressValue(TagData &tagData) override {
        if (setTagAddressValue) {
            tagData.tagAddress = tagAddressSetValue;
            tagData.tagValue = tagValueSetValue;
        } else {
            BaseClass::getTagAddressValue(tagData);
        }
    }
    std::chrono::steady_clock::time_point cpuTimePointReturnValue{};
    bool callBaseGetCpuTimePoint = true;
    uint64_t tagAddressSetValue = MemoryConstants::pageSize;
    uint64_t tagValueSetValue = 1ull;
    bool setTagAddressValue = false;
};

using namespace NEO;

struct MockDrmMemoryManagerForcePin : public DrmMemoryManager {
    using DrmMemoryManager::forcePinEnabled;
};

HWTEST_F(DrmDirectSubmissionTest, whenCreateDrmDirectSubmissionLightThenDisableForcePin) {
    EXPECT_TRUE(static_cast<MockDrmMemoryManagerForcePin *>(executionEnvironment.memoryManager.get())->forcePinEnabled);
    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    drm->bindAvailable = false;
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(static_cast<MockDrmMemoryManagerForcePin *>(executionEnvironment.memoryManager.get())->forcePinEnabled);
}

HWTEST_F(DrmDirectSubmissionTest, whenCreateDrmDirectSubmissionThenEnableForcePin) {
    EXPECT_TRUE(static_cast<MockDrmMemoryManagerForcePin *>(executionEnvironment.memoryManager.get())->forcePinEnabled);
    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    drm->bindAvailable = true;
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(static_cast<MockDrmMemoryManagerForcePin *>(executionEnvironment.memoryManager.get())->forcePinEnabled);
}

HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenCallingLinuxImplementationThenExpectInitialImplementationValues) {
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    EXPECT_TRUE(drm->isDirectSubmissionActive());

    EXPECT_TRUE(drmDirectSubmission.allocateResources());

    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size, nullptr));

    EXPECT_TRUE(drmDirectSubmission.handleResidency());

    EXPECT_NE(0ull, drmDirectSubmission.switchRingBuffers(nullptr));

    EXPECT_EQ(0ull, drmDirectSubmission.updateTagValue(drmDirectSubmission.dispatchMonitorFenceRequired(false)));

    TagData tagData = {1ull, 1ull};
    drmDirectSubmission.getTagAddressValue(tagData);
    EXPECT_EQ(drmDirectSubmission.currentTagData.tagAddress, tagData.tagAddress);
    EXPECT_EQ(drmDirectSubmission.currentTagData.tagValue + 1, tagData.tagValue);

    *drmDirectSubmission.tagAddress = 1u;
}

HWTEST_F(DrmDirectSubmissionTest, givenUllsLightWhenSwitchRingBufferNeedsToAllocateNewRingBufferThenAddToResidencyVectorAndRingStop) {
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    drmDirectSubmission.osContext.setDirectSubmissionActive();
    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    EXPECT_TRUE(drm->isDirectSubmissionActive());
    EXPECT_TRUE(drmDirectSubmission.allocateResources());

    drmDirectSubmission.ringBuffers[1].completionFence = 1u;
    drmDirectSubmission.ringStart = true;
    ResidencyContainer residencyContainer{};
    static_cast<DrmMemoryOperationsHandler *>(executionEnvironment.rootDeviceEnvironments[device->getRootDeviceIndex()]->memoryOperationsInterface.get())->mergeWithResidencyContainer(&drmDirectSubmission.osContext, residencyContainer);
    const auto expectedSize = residencyContainer.size() + 1u;

    EXPECT_NE(0ull, drmDirectSubmission.switchRingBuffers(&residencyContainer));

    EXPECT_EQ(residencyContainer.size(), expectedSize);
    EXPECT_FALSE(drmDirectSubmission.ringStart);

    drmDirectSubmission.ringBuffers[1].completionFence = 0u;
}

HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenCallingIsCompletedThenProperValueReturned) {
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    EXPECT_TRUE(drm->isDirectSubmissionActive());
    EXPECT_TRUE(drmDirectSubmission.allocateResources());

    drmDirectSubmission.ringBuffers[0].completionFence = 1u;
    EXPECT_FALSE(drmDirectSubmission.isCompleted(0u));

    *drmDirectSubmission.tagAddress = 1u;
    EXPECT_TRUE(drmDirectSubmission.isCompleted(0u));

    drmDirectSubmission.ringBuffers[0].completionFence = 0u;
}

HWTEST_F(DrmDirectSubmissionTest, whenCreateDirectSubmissionThenValidObjectIsReturned) {
    auto directSubmission = DirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>::create(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_NE(directSubmission.get(), nullptr);

    bool ret = directSubmission->initialize(false);
    EXPECT_TRUE(ret);
}

HWTEST_F(DrmDirectSubmissionTest, givenCompletionFenceSupportWhenCreateDrmDirectSubmissionThenTagAllocationIsSetAsCompletionFenceAllocation) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();

    ASSERT_TRUE(drm->completionFenceSupport());

    auto expectedCompletionFenceAllocation = commandStreamReceiver.getTagAllocation();
    EXPECT_NE(nullptr, expectedCompletionFenceAllocation);
    {
        MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
        EXPECT_EQ(expectedCompletionFenceAllocation, directSubmission.completionFenceAllocation);
    }
    {
        MockDrmDirectSubmission<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
        EXPECT_EQ(expectedCompletionFenceAllocation, directSubmission.completionFenceAllocation);
    }
}

HWTEST_F(DrmDirectSubmissionTest, givenCompletionFenceSupportWhenGettingCompletionFencePointerThenCompletionFenceValueAddressIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();

    ASSERT_TRUE(drm->completionFenceSupport());

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
    EXPECT_EQ(&directSubmission.completionFenceValue, directSubmission.getCompletionValuePointer());
}

HWTEST_F(DrmDirectSubmissionTest, givenDebugFlagSetWhenInitializingThenOverrideFenceStartValue) {
    DebugManagerStateRestore restorer;

    TaskCountType fenceStartValue = 1234;

    debugManager.flags.EnableDrmCompletionFence.set(1);
    debugManager.flags.OverrideUserFenceStartValue.set(static_cast<int32_t>(fenceStartValue));
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();

    ASSERT_TRUE(drm->completionFenceSupport());

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
    EXPECT_EQ(fenceStartValue, directSubmission.completionFenceValue);
}

HWTEST_F(DrmDirectSubmissionTest, givenLatestSentTaskCountWhenInitializingThenSetStartValue) {
    DebugManagerStateRestore restorer;

    TaskCountType fenceStartValue = 1234;

    debugManager.flags.EnableDrmCompletionFence.set(1);
    auto &commandStreamReceiver = device->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.latestSentTaskCount = fenceStartValue;

    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();

    ASSERT_TRUE(drm->completionFenceSupport());

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
    EXPECT_EQ(fenceStartValue, directSubmission.completionFenceValue);
}

HWTEST_F(DrmDirectSubmissionTest, givenNoCompletionFenceSupportWhenGettingCompletionFencePointerThenNullptrIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(0);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();

    ASSERT_FALSE(drm->completionFenceSupport());

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
    EXPECT_EQ(nullptr, directSubmission.getCompletionValuePointer());
}

HWTEST_F(DrmDirectSubmissionTest, givenPciBarrierWhenCreateDirectSubmissionThenPtrMappedAndOtherSyncMethodsDisabled) {
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    auto ptr = drm->getIoctlHelper()->pciBarrierMmap();
    if (!ptr) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionPCIBarrier.set(1);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);

    EXPECT_NE(nullptr, directSubmission.pciBarrierPtr);
    EXPECT_NE(DirectSubmissionSfenceMode::disabled, directSubmission.sfenceMode);
    EXPECT_FALSE(directSubmission.miMemFenceRequired);

    SysCalls::munmap(ptr, MemoryConstants::pageSize);
}

HWTEST_F(DrmDirectSubmissionTest, givenPciBarrierWhenCreateDirectSubmissionAndMmapFailsThenPtrNotMappedAndOtherSyncMethodsRemain) {
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    auto ptr = drm->getIoctlHelper()->pciBarrierMmap();
    if (!ptr) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionPCIBarrier.set(1);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    VariableBackup<bool> backup(&SysCalls::failMmap, true);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);

    EXPECT_EQ(nullptr, directSubmission.pciBarrierPtr);
    EXPECT_NE(DirectSubmissionSfenceMode::disabled, directSubmission.sfenceMode);
    auto expectMiMemFence = device->getHardwareInfo().capabilityTable.isIntegratedDevice ? false : device->getRootDeviceEnvironment().getHelper<ProductHelper>().isAcquireGlobalFenceInDirectSubmissionRequired(device->getHardwareInfo());
    EXPECT_EQ(directSubmission.miMemFenceRequired, expectMiMemFence);

    SysCalls::munmap(ptr, MemoryConstants::pageSize);
}

HWTEST_F(DrmDirectSubmissionTest, givenPciBarrierDisabledWhenCreateDirectSubmissionThenPtrNotMappedAndOtherSyncMethodsRemain) {
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    auto ptr = drm->getIoctlHelper()->pciBarrierMmap();
    if (!ptr) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionPCIBarrier.set(0);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);

    EXPECT_EQ(nullptr, directSubmission.pciBarrierPtr);
    EXPECT_NE(DirectSubmissionSfenceMode::disabled, directSubmission.sfenceMode);
    auto expectMiMemFence = device->getHardwareInfo().capabilityTable.isIntegratedDevice ? false : device->getRootDeviceEnvironment().getHelper<ProductHelper>().isAcquireGlobalFenceInDirectSubmissionRequired(device->getHardwareInfo());
    EXPECT_EQ(directSubmission.miMemFenceRequired, expectMiMemFence);

    SysCalls::munmap(ptr, MemoryConstants::pageSize);
}

HWTEST_F(DrmDirectSubmissionTest, givenNoCompletionFenceSupportWhenCreateDrmDirectSubmissionThenCompletionFenceAllocationIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(0);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();

    ASSERT_FALSE(drm->completionFenceSupport());
    {
        MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
        EXPECT_EQ(directSubmission.miMemFenceRequired, directSubmission.completionFenceAllocation != nullptr);
    }
    {
        MockDrmDirectSubmission<FamilyType, BlitterDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
        EXPECT_EQ(directSubmission.miMemFenceRequired, directSubmission.completionFenceAllocation != nullptr);
    }
}

HWTEST_F(DrmDirectSubmissionTest, givenDirectSubmissionWithoutCompletionFenceAllocationWhenDestroyingThenNoWaitForUserFenceIsCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(0);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    ASSERT_FALSE(drm->completionFenceSupport());

    drm->waitUserFenceParams.clear();
    {
        MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
        directSubmission.completionFenceValue = 10;
    }

    EXPECT_EQ(0u, drm->waitUserFenceParams.size());
}

HWTEST_F(DrmDirectSubmissionTest, givenCompletionFenceSupportAndFenceIsNotCompletedWhenDestroyingThenWaitForUserFenceIsCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    ASSERT_TRUE(drm->completionFenceSupport());

    drm->waitUserFenceParams.clear();
    {
        MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
        directSubmission.completionFenceValue = 10;
    }

    EXPECT_EQ(osContext->getDrmContextIds().size(), drm->waitUserFenceParams.size());
}

HWTEST_F(DrmDirectSubmissionTest, givenCompletionFenceSupportAndHangingContextWhenDestroyingThenWaitForUserFenceIsCalledWithSmallTimeout) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    ASSERT_TRUE(drm->completionFenceSupport());

    drm->waitUserFenceParams.clear();

    osContext->setHangDetected();
    {
        MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
        directSubmission.completionFenceValue = 10;
    }

    EXPECT_EQ(osContext->getDrmContextIds().size(), drm->waitUserFenceParams.size());
    EXPECT_EQ(1, drm->waitUserFenceParams[0].timeout);
    EXPECT_EQ(10u, drm->waitUserFenceParams[0].value);
    EXPECT_EQ(Drm::ValueWidth::u64, drm->waitUserFenceParams[0].dataWidth);
}

HWTEST_F(DrmDirectSubmissionTest, givenCompletionFenceSupportAndFenceIsNotCompletedWhenWaitOnSpecificAddressesPerOsContext) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = device->getUltCommandStreamReceiver<FamilyType>();
    memset(commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(), 0, commandStreamReceiver.getTagAllocation()->getUnderlyingBufferSize());
    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    ASSERT_TRUE(drm->completionFenceSupport());
    auto completionFenceBaseCpuAddress = reinterpret_cast<uint64_t>(commandStreamReceiver.getTagAddress()) + TagAllocationLayout::completionFenceOffset;
    uint32_t expectedCompletionValueToWait = 10u;

    {
        DeviceBitfield firstTileBitfield{0b01};
        OsContextLinux osContext(*drm, 0, 0u,
                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                              PreemptionMode::ThreadGroup, firstTileBitfield));
        osContext.ensureContextInitialized(false);
        commandStreamReceiver.setupContext(osContext);
        drm->waitUserFenceParams.clear();
        {
            MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
            directSubmission.completionFenceValue = expectedCompletionValueToWait;
        }
        EXPECT_EQ(1u, drm->waitUserFenceParams.size());
        EXPECT_EQ(expectedCompletionValueToWait, drm->waitUserFenceParams[0].value);
        EXPECT_EQ(completionFenceBaseCpuAddress, drm->waitUserFenceParams[0].address);
    }
    {
        DeviceBitfield secondTileBitfield{0b10};
        OsContextLinux osContext(*drm, 0, 0u,
                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                              PreemptionMode::ThreadGroup, secondTileBitfield));
        osContext.ensureContextInitialized(false);
        commandStreamReceiver.setupContext(osContext);
        drm->waitUserFenceParams.clear();
        {
            MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
            directSubmission.completionFenceValue = expectedCompletionValueToWait;
        }
        EXPECT_EQ(1u, drm->waitUserFenceParams.size());
        EXPECT_EQ(expectedCompletionValueToWait, drm->waitUserFenceParams[0].value);
        EXPECT_EQ(completionFenceBaseCpuAddress, drm->waitUserFenceParams[0].address);
    }

    {
        DeviceBitfield twoTilesBitfield{0b11};
        OsContextLinux osContext(*drm, 0, 0u,
                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                              PreemptionMode::ThreadGroup, twoTilesBitfield));
        osContext.ensureContextInitialized(false);
        commandStreamReceiver.setupContext(osContext);
        drm->waitUserFenceParams.clear();
        MockGraphicsAllocation workPartitionAllocation{};
        commandStreamReceiver.workPartitionAllocation = &workPartitionAllocation;
        {
            debugManager.flags.EnableImplicitScaling.set(1);
            MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
            directSubmission.completionFenceValue = expectedCompletionValueToWait;
        }
        commandStreamReceiver.workPartitionAllocation = nullptr;

        EXPECT_EQ(2u, drm->waitUserFenceParams.size());
        EXPECT_EQ(expectedCompletionValueToWait, drm->waitUserFenceParams[0].value);
        EXPECT_EQ(completionFenceBaseCpuAddress, drm->waitUserFenceParams[0].address);

        EXPECT_EQ(expectedCompletionValueToWait, drm->waitUserFenceParams[1].value);
        EXPECT_EQ(completionFenceBaseCpuAddress + commandStreamReceiver.getImmWritePostSyncWriteOffset(), drm->waitUserFenceParams[1].address);
    }
    commandStreamReceiver.setupContext(*osContext);
}

HWTEST_F(DrmDirectSubmissionTest, givenNoCompletionFenceSupportWhenSubmittingThenNoCompletionAddressIsPassedToExec) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(0);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    drmDirectSubmission.completionFenceAllocation = nullptr;
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    MockBufferObject mockBO(0, drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    for (auto i = 0; i < 2; i++) {
        mockBO.passedExecParams.clear();
        EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size, nullptr));

        ASSERT_EQ(1u, mockBO.passedExecParams.size());
        EXPECT_EQ(0u, mockBO.passedExecParams[0].completionGpuAddress);
        EXPECT_EQ(0u, mockBO.passedExecParams[0].completionValue);
    }
    ringBuffer->getBufferObjectToModify(0) = initialBO;
}

HWTEST_F(DrmDirectSubmissionTest, givenNoCompletionFenceSupportAndExecFailureWhenSubmittingThenGetDispatchErrorCode) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(0);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    drmDirectSubmission.completionFenceAllocation = nullptr;
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    MockBufferObject mockBO(0, drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    mockBO.execReturnValue = ENXIO;
    EXPECT_FALSE(drmDirectSubmission.submit(gpuAddress, size, nullptr));
    EXPECT_EQ((uint32_t)ENXIO, drmDirectSubmission.getDispatchErrorCode());

    ringBuffer->getBufferObjectToModify(0) = initialBO;
}

HWTEST_F(DrmDirectSubmissionTest, givenCompletionFenceSupportAndExecFailureWhenSubmittingThenCompletionFenceValueIsNotIncreased) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(commandStreamReceiver);
    drmDirectSubmission.completionFenceAllocation = commandStreamReceiver.getTagAllocation();
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    MockBufferObject mockBO(0, drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    mockBO.execReturnValue = 1;
    drmDirectSubmission.completionFenceValue = 1u;
    EXPECT_FALSE(drmDirectSubmission.submit(gpuAddress, size, nullptr));
    EXPECT_EQ(1u, drmDirectSubmission.completionFenceValue);

    ringBuffer->getBufferObjectToModify(0) = initialBO;
}

HWTEST_F(DrmDirectSubmissionTest, givenTile0AndCompletionFenceSupportWhenSubmittingThenCompletionAddressAndValueArePassedToExec) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    auto completionFenceBaseGpuAddress = commandStreamReceiver.getTagAllocation()->getGpuAddress() + TagAllocationLayout::completionFenceOffset;

    DeviceBitfield firstTileBitfield{0b01};
    OsContextLinux osContextTile0(*drm, 0, 0u,
                                  EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                               PreemptionMode::ThreadGroup, firstTileBitfield));
    osContextTile0.ensureContextInitialized(false);
    commandStreamReceiver.setupContext(osContextTile0);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(commandStreamReceiver);
    drmDirectSubmission.completionFenceAllocation = commandStreamReceiver.getTagAllocation();
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    MockBufferObject mockBO(0, drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    auto initialCompletionValue = commandStreamReceiver.peekLatestSentTaskCount();

    for (auto i = 0u; i < 2; i++) {
        mockBO.passedExecParams.clear();
        EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size, nullptr));

        ASSERT_EQ(1u, mockBO.passedExecParams.size());
        EXPECT_EQ(completionFenceBaseGpuAddress, mockBO.passedExecParams[0].completionGpuAddress);

        auto expectedCompletionValue = i + 1 + initialCompletionValue;

        EXPECT_EQ(expectedCompletionValue, mockBO.passedExecParams[0].completionValue);
    }
    ringBuffer->getBufferObjectToModify(0) = initialBO;

    commandStreamReceiver.setupContext(*osContext);
}

HWTEST_F(DrmDirectSubmissionTest, givenTile1AndCompletionFenceSupportWhenSubmittingThenCompletionAddressAndValueArePassedToExec) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    auto completionFenceBaseGpuAddress = commandStreamReceiver.getTagAllocation()->getGpuAddress() + TagAllocationLayout::completionFenceOffset;

    DeviceBitfield secondTileBitfield{0b10};
    OsContextLinux osContextTile1(*drm, 0, 0u,
                                  EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                               PreemptionMode::ThreadGroup, secondTileBitfield));
    osContextTile1.ensureContextInitialized(false);
    commandStreamReceiver.setupContext(osContextTile1);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(commandStreamReceiver);
    drmDirectSubmission.completionFenceAllocation = commandStreamReceiver.getTagAllocation();
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    MockBufferObject mockBO(0, drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    auto initialCompletionValue = commandStreamReceiver.peekLatestSentTaskCount();

    for (auto i = 0u; i < 2; i++) {
        mockBO.passedExecParams.clear();
        EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size, nullptr));

        ASSERT_EQ(1u, mockBO.passedExecParams.size());
        EXPECT_EQ(completionFenceBaseGpuAddress, mockBO.passedExecParams[0].completionGpuAddress);

        auto expectedCompletionValue = i + 1 + initialCompletionValue;
        EXPECT_EQ(expectedCompletionValue, mockBO.passedExecParams[0].completionValue);
    }
    ringBuffer->getBufferObjectToModify(0) = initialBO;

    commandStreamReceiver.setupContext(*osContext);
}

HWTEST_F(DrmDirectSubmissionTest, givenTwoTilesAndCompletionFenceSupportWhenSubmittingThenCompletionAddressAndValueArePassedToExec) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = device->getUltCommandStreamReceiver<FamilyType>();
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    auto completionFenceBaseGpuAddress = commandStreamReceiver.getTagAllocation()->getGpuAddress() + TagAllocationLayout::completionFenceOffset;

    DeviceBitfield twoTilesBitfield{0b11};
    OsContextLinux osContextBothTiles(*drm, 0, 0u,
                                      EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                   PreemptionMode::ThreadGroup, twoTilesBitfield));
    osContextBothTiles.ensureContextInitialized(false);
    commandStreamReceiver.setupContext(osContextBothTiles);

    MockGraphicsAllocation workPartitionAllocation{};
    commandStreamReceiver.workPartitionAllocation = &workPartitionAllocation;

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(commandStreamReceiver);

    commandStreamReceiver.workPartitionAllocation = nullptr;

    drmDirectSubmission.completionFenceAllocation = commandStreamReceiver.getTagAllocation();
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    MockBufferObject mockBO(0, drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    auto initialCompletionValue = commandStreamReceiver.peekLatestSentTaskCount();

    for (auto i = 0u; i < 2; i++) {
        mockBO.passedExecParams.clear();
        EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size, nullptr));

        ASSERT_EQ(2u, mockBO.passedExecParams.size());
        EXPECT_EQ(completionFenceBaseGpuAddress, mockBO.passedExecParams[0].completionGpuAddress);

        auto expectedCompletionValue = i + initialCompletionValue + 1;
        EXPECT_EQ(expectedCompletionValue, mockBO.passedExecParams[0].completionValue);

        EXPECT_EQ(completionFenceBaseGpuAddress + commandStreamReceiver.getImmWritePostSyncWriteOffset(), mockBO.passedExecParams[1].completionGpuAddress);
        EXPECT_EQ(expectedCompletionValue, mockBO.passedExecParams[1].completionValue);
    }
    ringBuffer->getBufferObjectToModify(0) = initialBO;

    commandStreamReceiver.setupContext(*osContext);
}

HWTEST_F(DrmDirectSubmissionTest, givenDisabledMonitorFenceWhenDispatchSwitchRingBufferThenDispatchPipeControl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = true;
    directSubmission.ringStart = true;

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    uint64_t newAddress = 0x1234;
    directSubmission.dispatchSwitchRingBufferSection(newAddress);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();

    EXPECT_NE(pipeControl, nullptr);
    EXPECT_EQ(directSubmission.getSizeSwitchRingBufferSection(), Dispatcher::getSizeStartCommandBuffer() + Dispatcher::getSizeMonitorFence(device->getRootDeviceEnvironment()));

    directSubmission.currentTagData.tagValue--;
}

HWTEST_F(DrmDirectSubmissionTest, givenDisabledMonitorFenceWhenUpdateTagValueThenTagIsNotUpdated) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = true;
    directSubmission.ringStart = true;

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    auto currentTag = directSubmission.currentTagData.tagValue;
    directSubmission.updateTagValue(directSubmission.dispatchMonitorFenceRequired(false));

    auto updatedTag = directSubmission.currentTagData.tagValue;

    EXPECT_EQ(currentTag, updatedTag);

    directSubmission.currentTagData.tagValue--;
}

HWTEST_F(DrmDirectSubmissionTest, whenCheckForDirectSubmissionSupportThenProperValueIsReturned) {
    auto directSubmissionSupported = osContext->isDirectSubmissionSupported();

    auto &productHelper = device->getProductHelper();
    EXPECT_EQ(directSubmissionSupported, productHelper.isDirectSubmissionSupported(device->getReleaseHelper()) && executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>()->isVmBindAvailable());
}

HWTEST_F(DrmDirectSubmissionTest, givenDirectSubmissionNewResourceTlbFlushWhenDispatchCommandBufferThenTlbIsFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionNewResourceTlbFlush.set(1);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));

    directSubmission.handleNewResourcesSubmission();

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_TRUE(pipeControl->getTlbInvalidate());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));
}

HWTEST_F(DrmDirectSubmissionTest, givenDirectSubmissionLightWhenRegisterResourcesThenRestart) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(drmDirectSubmission.initialize(false));

    FlushStampTracker flushStamp(true);
    BatchBuffer batchBuffer = {};
    GraphicsAllocation *commandBuffer = nullptr;
    LinearStream stream;

    const AllocationProperties commandBufferProperties{device->getRootDeviceIndex(), 0x1000,
                                                       AllocationType::commandBuffer, device->getDeviceBitfield()};
    commandBuffer = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);

    stream.replaceGraphicsAllocation(commandBuffer);
    stream.replaceBuffer(commandBuffer->getUnderlyingBuffer(), commandBuffer->getUnderlyingBufferSize());
    stream.getSpace(0x20);

    memset(stream.getCpuBase(), 0, 0x20);

    batchBuffer.endCmdPtr = ptrOffset(stream.getCpuBase(), 0x20);
    batchBuffer.commandBufferAllocation = commandBuffer;
    batchBuffer.usedSize = 0x40;
    batchBuffer.taskStartAddress = 0x881112340000;
    batchBuffer.stream = &stream;
    batchBuffer.hasStallingCmds = true;

    ResidencyContainer residencyContainer{};
    batchBuffer.allocationsForResidency = &residencyContainer;
    drmDirectSubmission.ringStart = true;

    EXPECT_TRUE(drmDirectSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(drmDirectSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_NE(pipeControl, nullptr);
    auto *bbe = hwParse.getCommand<BATCH_BUFFER_END>();
    EXPECT_NE(bbe, nullptr);

    drmDirectSubmission.ringStart = false;
    executionEnvironment.memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(DrmDirectSubmissionTest, givenDirectSubmissionLightWhenExecTimeoutReachedThenRestart) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(drmDirectSubmission.initialize(false));

    FlushStampTracker flushStamp(true);
    BatchBuffer batchBuffer = {};
    GraphicsAllocation *commandBuffer = nullptr;
    LinearStream stream;

    const AllocationProperties commandBufferProperties{device->getRootDeviceIndex(), 0x1000,
                                                       AllocationType::commandBuffer, device->getDeviceBitfield()};
    commandBuffer = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);

    stream.replaceGraphicsAllocation(commandBuffer);
    stream.replaceBuffer(commandBuffer->getUnderlyingBuffer(), commandBuffer->getUnderlyingBufferSize());
    stream.getSpace(0x20);

    memset(stream.getCpuBase(), 0, 0x20);

    batchBuffer.endCmdPtr = ptrOffset(stream.getCpuBase(), 0x20);
    batchBuffer.commandBufferAllocation = commandBuffer;
    batchBuffer.usedSize = 0x40;
    batchBuffer.taskStartAddress = 0x881112340000;
    batchBuffer.stream = &stream;
    batchBuffer.hasStallingCmds = true;

    ResidencyContainer residencyContainer{};
    batchBuffer.allocationsForResidency = &residencyContainer;
    drmDirectSubmission.ringStart = true;
    static_cast<DrmMemoryOperationsHandler *>(executionEnvironment.rootDeviceEnvironments[device->getRootDeviceIndex()]->memoryOperationsInterface.get())->obtainAndResetNewResourcesSinceLastRingSubmit();

    drmDirectSubmission.lastUllsLightExecTimestamp = std::chrono::steady_clock::time_point{};
    drmDirectSubmission.cpuTimePointReturnValue = std::chrono::time_point<std::chrono::steady_clock>::max();
    drmDirectSubmission.callBaseGetCpuTimePoint = false;

    EXPECT_TRUE(drmDirectSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(drmDirectSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_NE(pipeControl, nullptr);
    auto *bbe = hwParse.getCommand<BATCH_BUFFER_END>();
    EXPECT_NE(bbe, nullptr);

    drmDirectSubmission.ringStart = false;
    executionEnvironment.memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(DrmDirectSubmissionTest, givenDirectSubmissionLightWhenNoRegisteredResourcesThenNoRestart) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(drmDirectSubmission.initialize(false));

    FlushStampTracker flushStamp(true);
    BatchBuffer batchBuffer = {};
    GraphicsAllocation *commandBuffer = nullptr;
    LinearStream stream;

    const AllocationProperties commandBufferProperties{device->getRootDeviceIndex(), 0x1000,
                                                       AllocationType::commandBuffer, device->getDeviceBitfield()};
    commandBuffer = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);

    stream.replaceGraphicsAllocation(commandBuffer);
    stream.replaceBuffer(commandBuffer->getUnderlyingBuffer(), commandBuffer->getUnderlyingBufferSize());
    stream.getSpace(0x20);

    memset(stream.getCpuBase(), 0, 0x20);

    batchBuffer.endCmdPtr = ptrOffset(stream.getCpuBase(), 0x20);
    batchBuffer.commandBufferAllocation = commandBuffer;
    batchBuffer.usedSize = 0x40;
    batchBuffer.taskStartAddress = 0x881112340000;
    batchBuffer.stream = &stream;
    batchBuffer.hasStallingCmds = true;

    ResidencyContainer residencyContainer{};
    batchBuffer.allocationsForResidency = &residencyContainer;
    drmDirectSubmission.ringStart = true;
    static_cast<DrmMemoryOperationsHandler *>(executionEnvironment.rootDeviceEnvironments[device->getRootDeviceIndex()]->memoryOperationsInterface.get())->obtainAndResetNewResourcesSinceLastRingSubmit();

    EXPECT_TRUE(drmDirectSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(drmDirectSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_EQ(pipeControl, nullptr);
    auto *bbe = hwParse.getCommand<BATCH_BUFFER_END>();
    EXPECT_EQ(bbe, nullptr);

    drmDirectSubmission.ringStart = false;
    executionEnvironment.memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(DrmDirectSubmissionTest, givenNewResourceBoundWhenDispatchCommandBufferThenTlbIsFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionNewResourceTlbFlush.set(-1);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    osContext->setNewResourceBound();

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));
    EXPECT_TRUE(osContext->isTlbFlushRequired());

    directSubmission.handleNewResourcesSubmission();

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_TRUE(pipeControl->getTlbInvalidate());

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));
}

HWTEST_F(DrmDirectSubmissionTest, givenNoNewResourceBoundWhenDispatchCommandBufferThenTlbIsNotFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionNewResourceTlbFlush.set(-1);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));

    directSubmission.handleNewResourcesSubmission();

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_EQ(pipeControl, nullptr);
    EXPECT_FALSE(osContext->isTlbFlushRequired());

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));
}

HWTEST_F(DrmDirectSubmissionTest, givenDirectSubmissionNewResourceTlbFlushZeroAndNewResourceBoundWhenDispatchCommandBufferThenTlbIsNotFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionNewResourceTlbFlush.set(0);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));

    directSubmission.handleNewResourcesSubmission();

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_EQ(pipeControl, nullptr);
    EXPECT_FALSE(osContext->isTlbFlushRequired());

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmDirectSubmissionTest, givenMultipleActiveTilesWhenWaitingForTagUpdateThenQueryAllActiveTiles) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    VariableBackup<uint32_t> backupWaitCount(&WaitUtils::waitCount, 1);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    uint32_t offset = directSubmission.immWritePostSyncOffset;
    EXPECT_NE(0u, offset);
    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);
    directSubmission.activeTiles = 2;

    auto pollAddress = directSubmission.tagAddress;
    *pollAddress = 10;
    pollAddress = ptrOffset(pollAddress, offset);
    *pollAddress = 10;

    CpuIntrinsicsTests::pauseCounter = 0;
    directSubmission.wait(9);
    EXPECT_EQ(2u, CpuIntrinsicsTests::pauseCounter);
}

HWTEST_F(DrmDirectSubmissionTest,
         givenRenderDispatcherAndMultiTileDeviceWhenCreatingDirectSubmissionUsingMultiTileContextThenExpectActiveTilesMatchSubDeviceCount) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    VariableBackup<bool> backup(&ImplicitScaling::apiSupport, true);
    device->deviceBitfield.set(0b11);
    device->rootCsrCreated = true;
    device->numSubDevices = 2;

    osContext = std::make_unique<OsContextLinux>(*executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>(), device->getRootDeviceIndex(), 0u,
                                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                              PreemptionMode::ThreadGroup, device->getDeviceBitfield()));
    osContext->ensureContextInitialized(false);
    EXPECT_EQ(2u, osContext->getDeviceBitfield().count());

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*device);

    device->getDefaultEngine().commandStreamReceiver->setupContext(*osContext);
    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(2u, directSubmission.activeTiles);
    EXPECT_TRUE(directSubmission.partitionedMode);
    EXPECT_FALSE(directSubmission.partitionConfigSet);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);
}

HWTEST_F(DrmDirectSubmissionTest, givenRenderDispatcherAndMultiTileDeviceWhenCreatingDirectSubmissionSingleTileContextThenExpectActiveTilesEqualsSingleTile) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    VariableBackup<bool> backup(&ImplicitScaling::apiSupport, true);
    device->deviceBitfield.set(0b11);
    device->rootCsrCreated = true;
    device->numSubDevices = 2;

    EXPECT_EQ(1u, osContext->getDeviceBitfield().count());

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*device);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(1u, directSubmission.activeTiles);
    EXPECT_FALSE(directSubmission.partitionedMode);
    EXPECT_TRUE(directSubmission.partitionConfigSet);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);
}

HWTEST_F(DrmDirectSubmissionTest, givenBlitterDispatcherAndMultiTileDeviceWhenCreatingDirectSubmissionThenExpectActiveTilesEqualsOne) {
    using Dispatcher = BlitterDispatcher<FamilyType>;

    VariableBackup<bool> backup(&ImplicitScaling::apiSupport, true);
    device->deviceBitfield.set(0b11);

    osContext = std::make_unique<OsContextLinux>(*executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>(), device->getRootDeviceIndex(), 0u,
                                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                              PreemptionMode::ThreadGroup, device->getDeviceBitfield()));
    osContext->ensureContextInitialized(false);
    EXPECT_EQ(2u, osContext->getDeviceBitfield().count());

    device->getDefaultEngine().commandStreamReceiver->setupContext(*osContext);
    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(1u, directSubmission.activeTiles);
    EXPECT_FALSE(directSubmission.partitionedMode);
    EXPECT_TRUE(directSubmission.partitionConfigSet);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);
}
HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenHandleSwitchRingBufferCalledThenPrevRingBufferHasUpdatedFenceValue) {
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    auto prevCompletionFenceVal = 1u;
    auto prevRingBufferIndex = 0u;
    auto newCompletionFenceValue = 10u;
    drmDirectSubmission.currentTagData.tagValue = newCompletionFenceValue;
    drmDirectSubmission.ringBuffers[prevRingBufferIndex].completionFence = prevCompletionFenceVal;
    drmDirectSubmission.handleSwitchRingBuffers(nullptr);
    EXPECT_EQ(drmDirectSubmission.ringBuffers[prevRingBufferIndex].completionFence, newCompletionFenceValue + 1);
}
HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenEnableRingSwitchTagUpdateWaEnabledAndRingNotStartedThenCompletionFenceIsNotUpdated) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableRingSwitchTagUpdateWa.set(1);
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    auto prevCompletionFenceVal = 1u;
    auto prevRingBufferIndex = 0u;
    auto newCompletionFenceValue = 10u;

    drmDirectSubmission.ringStart = false;

    drmDirectSubmission.currentTagData.tagValue = newCompletionFenceValue;
    drmDirectSubmission.ringBuffers[prevRingBufferIndex].completionFence = prevCompletionFenceVal;
    drmDirectSubmission.handleSwitchRingBuffers(nullptr);
    EXPECT_EQ(drmDirectSubmission.ringBuffers[prevRingBufferIndex].completionFence, prevCompletionFenceVal);
}
HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenEnableRingSwitchTagUpdateWaEnabledAndRingStartedThenCompletionFenceIsUpdated) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableRingSwitchTagUpdateWa.set(1);
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    auto prevCompletionFenceVal = 1u;
    auto prevRingBufferIndex = 0u;
    auto newCompletionFenceValue = 10u;

    drmDirectSubmission.ringStart = true;

    drmDirectSubmission.currentTagData.tagValue = newCompletionFenceValue;
    drmDirectSubmission.ringBuffers[prevRingBufferIndex].completionFence = prevCompletionFenceVal;
    drmDirectSubmission.handleSwitchRingBuffers(nullptr);
    EXPECT_EQ(drmDirectSubmission.ringBuffers[prevRingBufferIndex].completionFence, newCompletionFenceValue + 1);
    drmDirectSubmission.ringStart = false;
}
HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenEnableRingSwitchTagUpdateWaDisabledThenCompletionFenceIsUpdated) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableRingSwitchTagUpdateWa.set(0);
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    auto prevCompletionFenceVal = 1u;
    auto prevRingBufferIndex = 0u;
    auto newCompletionFenceValue = 10u;

    drmDirectSubmission.currentTagData.tagValue = newCompletionFenceValue;
    drmDirectSubmission.ringBuffers[prevRingBufferIndex].completionFence = prevCompletionFenceVal;
    drmDirectSubmission.handleSwitchRingBuffers(nullptr);
    EXPECT_EQ(drmDirectSubmission.ringBuffers[prevRingBufferIndex].completionFence, newCompletionFenceValue + 1);
}

HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenGettingDefaultInputMonitorFencePolicyThenDefaultIsTrue) {
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(drmDirectSubmission.inputMonitorFenceDispatchRequirement);
}

HWTEST_F(DrmDirectSubmissionTest,
         givenDrmDirectSubmissionWithStallingCommandInputMonitorFencePolicyWhenDispatchingWorkloadWithDisabledMonitorFenceThenDrmIgnoresInputFlag) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.DirectSubmissionMonitorFenceInputPolicy.set(0);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_FALSE(drmDirectSubmission.inputMonitorFenceDispatchRequirement);
    drmDirectSubmission.disableMonitorFence = true;

    FlushStampTracker flushStamp(true);

    EXPECT_TRUE(drmDirectSubmission.initialize(false));

    BatchBuffer batchBuffer = {};
    GraphicsAllocation *commandBuffer = nullptr;
    LinearStream stream;

    const AllocationProperties commandBufferProperties{device->getRootDeviceIndex(), 0x1000,
                                                       AllocationType::commandBuffer, device->getDeviceBitfield()};
    commandBuffer = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);

    stream.replaceGraphicsAllocation(commandBuffer);
    stream.replaceBuffer(commandBuffer->getUnderlyingBuffer(), commandBuffer->getUnderlyingBufferSize());
    stream.getSpace(0x20);

    memset(stream.getCpuBase(), 0, 0x20);

    batchBuffer.endCmdPtr = ptrOffset(stream.getCpuBase(), 0x20);
    batchBuffer.commandBufferAllocation = commandBuffer;
    batchBuffer.usedSize = 0x40;
    batchBuffer.taskStartAddress = 0x881112340000;
    batchBuffer.stream = &stream;
    batchBuffer.hasStallingCmds = true;

    EXPECT_TRUE(drmDirectSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(drmDirectSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();

    bool foundFenceUpdate = false;
    for (auto &it : hwParse.pipeControlList) {
        PIPE_CONTROL *pipeControl = reinterpret_cast<PIPE_CONTROL *>(it);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            foundFenceUpdate = true;
            break;
        }
    }
    EXPECT_FALSE(foundFenceUpdate);

    executionEnvironment.memoryManager->freeGraphicsMemory(commandBuffer);
    *drmDirectSubmission.tagAddress = 1;
}

HWTEST_F(DrmDirectSubmissionTest,
         givenDrmDirectSubmissionWithExplicitFlagInputMonitorFencePolicyWhenDispatchingWorkloadWithDisabledMonitorFenceThenDrmIgnoresInputFlag) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.DirectSubmissionMonitorFenceInputPolicy.set(1);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    EXPECT_TRUE(drmDirectSubmission.inputMonitorFenceDispatchRequirement);
    drmDirectSubmission.disableMonitorFence = true;

    FlushStampTracker flushStamp(true);

    EXPECT_TRUE(drmDirectSubmission.initialize(false));

    BatchBuffer batchBuffer = {};
    GraphicsAllocation *commandBuffer = nullptr;
    LinearStream stream;

    const AllocationProperties commandBufferProperties{device->getRootDeviceIndex(), 0x1000,
                                                       AllocationType::commandBuffer, device->getDeviceBitfield()};
    commandBuffer = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);

    stream.replaceGraphicsAllocation(commandBuffer);
    stream.replaceBuffer(commandBuffer->getUnderlyingBuffer(), commandBuffer->getUnderlyingBufferSize());
    stream.getSpace(0x20);

    memset(stream.getCpuBase(), 0, 0x20);

    batchBuffer.endCmdPtr = ptrOffset(stream.getCpuBase(), 0x20);
    batchBuffer.commandBufferAllocation = commandBuffer;
    batchBuffer.usedSize = 0x40;
    batchBuffer.taskStartAddress = 0x881112340000;
    batchBuffer.stream = &stream;
    batchBuffer.dispatchMonitorFence = true;

    EXPECT_TRUE(drmDirectSubmission.dispatchCommandBuffer(batchBuffer, flushStamp));

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(drmDirectSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();

    bool foundFenceUpdate = false;
    for (auto &it : hwParse.pipeControlList) {
        PIPE_CONTROL *pipeControl = reinterpret_cast<PIPE_CONTROL *>(it);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            foundFenceUpdate = true;
            break;
        }
    }
    EXPECT_FALSE(foundFenceUpdate);

    executionEnvironment.memoryManager->freeGraphicsMemory(commandBuffer);
    *drmDirectSubmission.tagAddress = 1;
}

HWTEST_F(DrmDirectSubmissionTest, givenGpuHangWhenWaitCalledThenGpuHangDetected) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    VariableBackup<uint32_t> backupWaitCount(&WaitUtils::waitCount, 1);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);
    directSubmission.gpuHangCheckPeriod = {};
    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    auto pollAddress = directSubmission.tagAddress;
    *pollAddress = 0;

    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    ResetStats resetStats{};
    resetStats.contextId = 0;
    resetStats.batchActive = 1;
    drm->resetStatsToReturn.push_back(resetStats);

    EXPECT_EQ(0, drm->ioctlCount.getResetStats);
    directSubmission.wait(1);
    EXPECT_EQ(1, drm->ioctlCount.getResetStats);
}

HWTEST_F(DrmDirectSubmissionTest,
         givenDirectSubmissionDisableMonitorFenceWhenStopRingIsCalledThenExpectStopCommandAndMonitorFenceDispatched) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDrmDirectSubmission<FamilyType, Dispatcher> regularDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    regularDirectSubmission.disableMonitorFence = false;
    size_t regularSizeEnd = regularDirectSubmission.getSizeEnd(false);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);
    directSubmission.setTagAddressValue = true;
    bool ret = directSubmission.allocateResources();
    directSubmission.ringStart = true;

    EXPECT_TRUE(ret);

    size_t tagUpdateSize = Dispatcher::getSizeMonitorFence(directSubmission.rootDeviceEnvironment);

    size_t disabledSizeEnd = directSubmission.getSizeEnd(false);
    EXPECT_EQ(disabledSizeEnd, regularSizeEnd + tagUpdateSize);

    directSubmission.tagValueSetValue = 0x4343123ull;
    directSubmission.tagAddressSetValue = 0xBEEF00000ull;
    directSubmission.stopRingBuffer(false);
    size_t expectedDispatchSize = disabledSizeEnd;
    EXPECT_LE(directSubmission.ringCommandStream.getUsed(), expectedDispatchSize);
    EXPECT_GE(directSubmission.ringCommandStream.getUsed() + MemoryConstants::cacheLineSize, expectedDispatchSize);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    MI_BATCH_BUFFER_END *bbEnd = hwParse.getCommand<MI_BATCH_BUFFER_END>();
    EXPECT_NE(nullptr, bbEnd);

    bool foundFenceUpdate = false;
    for (auto it = hwParse.pipeControlList.begin(); it != hwParse.pipeControlList.end(); it++) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        uint64_t data = pipeControl->getImmediateData();
        if ((directSubmission.tagAddressSetValue == NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl)) &&
            (directSubmission.tagValueSetValue == data)) {
            foundFenceUpdate = true;
            break;
        }
    }
    EXPECT_TRUE(foundFenceUpdate);
}

HWTEST2_F(DrmDirectSubmissionTest, givenRelaxedOrderingSchedulerRequiredWhenAskingForCmdsSizeThenReturnCorrectValue, IsAtLeastXeHpcCore) {
    using Dispatcher = RenderDispatcher<FamilyType>;
    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    size_t expectedBaseSemaphoreSectionSize = directSubmission.getSizePrefetchMitigation();
    if (directSubmission.isDisablePrefetcherRequired) {
        expectedBaseSemaphoreSectionSize += 2 * directSubmission.getSizeDisablePrefetcher();
    }

    directSubmission.relaxedOrderingEnabled = true;
    if (directSubmission.miMemFenceRequired) {
        expectedBaseSemaphoreSectionSize += MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronizationForDirectSubmission(device->getRootDeviceEnvironment());
    }

    EXPECT_EQ(expectedBaseSemaphoreSectionSize + RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<FamilyType>::totalSize, directSubmission.getSizeSemaphoreSection(true));
    EXPECT_EQ(expectedBaseSemaphoreSectionSize + EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), directSubmission.getSizeSemaphoreSection(false));

    size_t expectedBaseEndSize = Dispatcher::getSizeStopCommandBuffer() +
                                 Dispatcher::getSizeCacheFlush(directSubmission.rootDeviceEnvironment) +
                                 (Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer()) +
                                 MemoryConstants::cacheLineSize;
    if (directSubmission.disableMonitorFence) {
        expectedBaseEndSize += Dispatcher::getSizeMonitorFence(device->getRootDeviceEnvironment());
    }

    EXPECT_EQ(expectedBaseEndSize + directSubmission.getSizeDispatchRelaxedOrderingQueueStall(), directSubmission.getSizeEnd(true));
    EXPECT_EQ(expectedBaseEndSize, directSubmission.getSizeEnd(false));
}