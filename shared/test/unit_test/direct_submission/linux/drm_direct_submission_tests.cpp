/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <memory>

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
}

struct DrmDirectSubmissionTest : public DrmMemoryManagerBasic {
    void SetUp() override {
        DrmMemoryManagerBasic::SetUp();
        executionEnvironment.incRefInternal();

        executionEnvironment.memoryManager = std::make_unique<DrmMemoryManager>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                                                DebugManager.flags.EnableForcePin.get(),
                                                                                true,
                                                                                executionEnvironment);
        device.reset(MockDevice::create<MockDevice>(&executionEnvironment, 0u));
        osContext = std::make_unique<OsContextLinux>(*executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>(), device->getRootDeviceIndex(), 0u,
                                                     EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                  PreemptionMode::ThreadGroup, device->getDeviceBitfield()));
        osContext->ensureContextInitialized();
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
    using BaseClass::dispatchSwitchRingBufferSection;
    using BaseClass::DrmDirectSubmission;
    using BaseClass::getSizeNewResourceHandler;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::getTagAddressValue;
    using BaseClass::handleNewResourcesSubmission;
    using BaseClass::handleResidency;
    using BaseClass::isCompleted;
    using BaseClass::isNewResourceHandleNeeded;
    using BaseClass::miMemFenceRequired;
    using BaseClass::partitionConfigSet;
    using BaseClass::partitionedMode;
    using BaseClass::postSyncOffset;
    using BaseClass::ringBuffers;
    using BaseClass::ringStart;
    using BaseClass::submit;
    using BaseClass::switchRingBuffers;
    using BaseClass::tagAddress;
    using BaseClass::updateTagValue;
    using BaseClass::useNotifyForPostSync;
    using BaseClass::wait;
    using BaseClass::workPartitionAllocation;
};

using namespace NEO;

HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenCallingLinuxImplementationThenExpectInitialImplementationValues) {
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);

    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    EXPECT_TRUE(drm->isDirectSubmissionActive());

    EXPECT_TRUE(drmDirectSubmission.allocateResources());

    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size));

    EXPECT_TRUE(drmDirectSubmission.handleResidency());

    EXPECT_NE(0ull, drmDirectSubmission.switchRingBuffers());

    EXPECT_EQ(0ull, drmDirectSubmission.updateTagValue());

    TagData tagData = {1ull, 1ull};
    drmDirectSubmission.getTagAddressValue(tagData);
    EXPECT_EQ(drmDirectSubmission.currentTagData.tagAddress, tagData.tagAddress);
    EXPECT_EQ(drmDirectSubmission.currentTagData.tagValue + 1, tagData.tagValue);

    *drmDirectSubmission.tagAddress = 1u;
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

    bool ret = directSubmission->initialize(false, false);
    EXPECT_TRUE(ret);
}

HWTEST_F(DrmDirectSubmissionTest, givenCompletionFenceSupportWhenCreateDrmDirectSubmissionThenTagAllocationIsSetAsCompletionFenceAllocation) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDrmCompletionFence.set(1);
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
    DebugManager.flags.EnableDrmCompletionFence.set(1);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();

    ASSERT_TRUE(drm->completionFenceSupport());

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
    EXPECT_EQ(&directSubmission.completionFenceValue, directSubmission.getCompletionValuePointer());
}

HWTEST_F(DrmDirectSubmissionTest, givenNoCompletionFenceSupportWhenGettingCompletionFencePointerThenNullptrIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDrmCompletionFence.set(0);
    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();

    ASSERT_FALSE(drm->completionFenceSupport());

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
    EXPECT_EQ(nullptr, directSubmission.getCompletionValuePointer());
}

HWTEST_F(DrmDirectSubmissionTest, givenNoCompletionFenceSupportWhenCreateDrmDirectSubmissionThenCompletionFenceAllocationIsNotSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDrmCompletionFence.set(0);
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
    DebugManager.flags.EnableDrmCompletionFence.set(0);
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
    DebugManager.flags.EnableDrmCompletionFence.set(1);

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

HWTEST_F(DrmDirectSubmissionTest, givenCompletionFenceSupportAndFenceIsNotCompletedWhenWaitOnSpecificAddressesPerOsContext) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = device->getUltCommandStreamReceiver<FamilyType>();
    memset(commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(), 0, commandStreamReceiver.getTagAllocation()->getUnderlyingBufferSize());
    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    ASSERT_TRUE(drm->completionFenceSupport());
    auto completionFenceBaseCpuAddress = reinterpret_cast<uint64_t>(commandStreamReceiver.getTagAddress()) + Drm::completionFenceOffset;
    uint32_t expectedCompletionValueToWait = 10u;

    {
        DeviceBitfield firstTileBitfield{0b01};
        OsContextLinux osContext(*drm, 0, 0u,
                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                              PreemptionMode::ThreadGroup, firstTileBitfield));
        osContext.ensureContextInitialized();
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
                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                              PreemptionMode::ThreadGroup, secondTileBitfield));
        osContext.ensureContextInitialized();
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
                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                              PreemptionMode::ThreadGroup, twoTilesBitfield));
        osContext.ensureContextInitialized();
        commandStreamReceiver.setupContext(osContext);
        drm->waitUserFenceParams.clear();
        MockGraphicsAllocation workPartitionAllocation{};
        commandStreamReceiver.workPartitionAllocation = &workPartitionAllocation;
        {
            DebugManager.flags.EnableImplicitScaling.set(1);
            MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> directSubmission(commandStreamReceiver);
            directSubmission.completionFenceValue = expectedCompletionValueToWait;
        }
        commandStreamReceiver.workPartitionAllocation = nullptr;

        EXPECT_EQ(2u, drm->waitUserFenceParams.size());
        EXPECT_EQ(expectedCompletionValueToWait, drm->waitUserFenceParams[0].value);
        EXPECT_EQ(completionFenceBaseCpuAddress, drm->waitUserFenceParams[0].address);

        EXPECT_EQ(expectedCompletionValueToWait, drm->waitUserFenceParams[1].value);
        EXPECT_EQ(completionFenceBaseCpuAddress + commandStreamReceiver.getPostSyncWriteOffset(), drm->waitUserFenceParams[1].address);
    }
    commandStreamReceiver.setupContext(*osContext);
}

HWTEST_F(DrmDirectSubmissionTest, givenNoCompletionFenceSupportWhenSubmittingThenNoCompletionAddressIsPassedToExec) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDrmCompletionFence.set(0);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device->getDefaultEngine().commandStreamReceiver);
    drmDirectSubmission.completionFenceAllocation = nullptr;
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    MockBufferObject mockBO(drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    for (auto i = 0; i < 2; i++) {
        mockBO.passedExecParams.clear();
        EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size));

        ASSERT_EQ(1u, mockBO.passedExecParams.size());
        EXPECT_EQ(0u, mockBO.passedExecParams[0].completionGpuAddress);
        EXPECT_EQ(0u, mockBO.passedExecParams[0].completionValue);
    }
    ringBuffer->getBufferObjectToModify(0) = initialBO;
}

HWTEST_F(DrmDirectSubmissionTest, givenTile0AndCompletionFenceSupportWhenSubmittingThenCompletionAddressAndValueArePassedToExec) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    auto completionFenceBaseGpuAddress = commandStreamReceiver.getTagAllocation()->getGpuAddress() + Drm::completionFenceOffset;

    DeviceBitfield firstTileBitfield{0b01};
    OsContextLinux osContextTile0(*drm, 0, 0u,
                                  EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                               PreemptionMode::ThreadGroup, firstTileBitfield));
    osContextTile0.ensureContextInitialized();
    commandStreamReceiver.setupContext(osContextTile0);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(commandStreamReceiver);
    drmDirectSubmission.completionFenceAllocation = commandStreamReceiver.getTagAllocation();
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    MockBufferObject mockBO(drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    for (auto i = 0u; i < 2; i++) {
        mockBO.passedExecParams.clear();
        EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size));

        ASSERT_EQ(1u, mockBO.passedExecParams.size());
        EXPECT_EQ(completionFenceBaseGpuAddress, mockBO.passedExecParams[0].completionGpuAddress);
        EXPECT_EQ(i + 1, mockBO.passedExecParams[0].completionValue);
    }
    ringBuffer->getBufferObjectToModify(0) = initialBO;

    commandStreamReceiver.setupContext(*osContext);
}

HWTEST_F(DrmDirectSubmissionTest, givenTile1AndCompletionFenceSupportWhenSubmittingThenCompletionAddressAndValueArePassedToExec) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = *device->getDefaultEngine().commandStreamReceiver;
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    auto completionFenceBaseGpuAddress = commandStreamReceiver.getTagAllocation()->getGpuAddress() + Drm::completionFenceOffset;

    DeviceBitfield secondTileBitfield{0b10};
    OsContextLinux osContextTile1(*drm, 0, 0u,
                                  EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                               PreemptionMode::ThreadGroup, secondTileBitfield));
    osContextTile1.ensureContextInitialized();
    commandStreamReceiver.setupContext(osContextTile1);

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(commandStreamReceiver);
    drmDirectSubmission.completionFenceAllocation = commandStreamReceiver.getTagAllocation();
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    MockBufferObject mockBO(drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    for (auto i = 0u; i < 2; i++) {
        mockBO.passedExecParams.clear();
        EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size));

        ASSERT_EQ(1u, mockBO.passedExecParams.size());
        EXPECT_EQ(completionFenceBaseGpuAddress, mockBO.passedExecParams[0].completionGpuAddress);
        EXPECT_EQ(i + 1, mockBO.passedExecParams[0].completionValue);
    }
    ringBuffer->getBufferObjectToModify(0) = initialBO;

    commandStreamReceiver.setupContext(*osContext);
}

HWTEST_F(DrmDirectSubmissionTest, givenTwoTilesAndCompletionFenceSupportWhenSubmittingThenCompletionAddressAndValueArePassedToExec) {
    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDrmCompletionFence.set(1);

    auto &commandStreamReceiver = device->getUltCommandStreamReceiver<FamilyType>();
    auto drm = executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>();
    auto completionFenceBaseGpuAddress = commandStreamReceiver.getTagAllocation()->getGpuAddress() + Drm::completionFenceOffset;

    DeviceBitfield twoTilesBitfield{0b11};
    OsContextLinux osContextBothTiles(*drm, 0, 0u,
                                      EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                   PreemptionMode::ThreadGroup, twoTilesBitfield));
    osContextBothTiles.ensureContextInitialized();
    commandStreamReceiver.setupContext(osContextBothTiles);

    MockGraphicsAllocation workPartitionAllocation{};
    commandStreamReceiver.workPartitionAllocation = &workPartitionAllocation;

    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(commandStreamReceiver);

    commandStreamReceiver.workPartitionAllocation = nullptr;

    drmDirectSubmission.completionFenceAllocation = commandStreamReceiver.getTagAllocation();
    EXPECT_TRUE(drmDirectSubmission.allocateResources());
    auto ringBuffer = static_cast<DrmAllocation *>(drmDirectSubmission.ringBuffers[drmDirectSubmission.currentRingBuffer].ringBuffer);
    auto initialBO = ringBuffer->getBufferObjectToModify(0);

    MockBufferObject mockBO(drm);
    ringBuffer->getBufferObjectToModify(0) = &mockBO;

    for (auto i = 0u; i < 2; i++) {
        mockBO.passedExecParams.clear();
        EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size));

        ASSERT_EQ(2u, mockBO.passedExecParams.size());
        EXPECT_EQ(completionFenceBaseGpuAddress, mockBO.passedExecParams[0].completionGpuAddress);
        EXPECT_EQ(i + 1, mockBO.passedExecParams[0].completionValue);

        EXPECT_EQ(completionFenceBaseGpuAddress + commandStreamReceiver.getPostSyncWriteOffset(), mockBO.passedExecParams[1].completionGpuAddress);
        EXPECT_EQ(i + 1, mockBO.passedExecParams[1].completionValue);
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
    EXPECT_EQ(directSubmission.getSizeSwitchRingBufferSection(), Dispatcher::getSizeStartCommandBuffer() + Dispatcher::getSizeMonitorFence(device->getHardwareInfo()));

    directSubmission.currentTagData.tagValue--;
}

HWTEST_F(DrmDirectSubmissionTest, givenDisabledMonitorFenceWhenUpdateTagValueThenTagIsNotUpdated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);
    directSubmission.disableMonitorFence = true;
    directSubmission.ringStart = true;

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    auto currentTag = directSubmission.currentTagData.tagValue;
    directSubmission.updateTagValue();

    auto updatedTag = directSubmission.currentTagData.tagValue;

    EXPECT_EQ(currentTag, updatedTag);

    directSubmission.currentTagData.tagValue--;
}

HWTEST_F(DrmDirectSubmissionTest, whenCheckForDirectSubmissionSupportThenProperValueIsReturned) {
    auto directSubmissionSupported = osContext->isDirectSubmissionSupported(device->getHardwareInfo());

    auto hwInfoConfig = HwInfoConfig::get(device->getHardwareInfo().platform.eProductFamily);
    EXPECT_EQ(directSubmissionSupported, hwInfoConfig->isDirectSubmissionSupported(device->getHardwareInfo()) && executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>()->isVmBindAvailable());
}

HWTEST_F(DrmDirectSubmissionTest, givenDirectSubmissionNewResourceTlbFlushWhenDispatchCommandBufferThenTlbIsFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionNewResourceTlbFlush.set(1);

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

HWTEST_F(DrmDirectSubmissionTest, givenNewResourceBoundWhenDispatchCommandBufferThenTlbIsFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionNewResourceTlbFlush.set(-1);

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
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_FALSE(osContext->isTlbFlushRequired());

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));
}

HWTEST_F(DrmDirectSubmissionTest, givenNoNewResourceBoundWhenDispatchCommandBufferThenTlbIsNotFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionNewResourceTlbFlush.set(-1);

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
    DebugManager.flags.DirectSubmissionNewResourceTlbFlush.set(0);

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

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    uint32_t offset = directSubmission.postSyncOffset;
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
                                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                              PreemptionMode::ThreadGroup, device->getDeviceBitfield()));
    osContext->ensureContextInitialized();
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
                                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                              PreemptionMode::ThreadGroup, device->getDeviceBitfield()));
    osContext->ensureContextInitialized();
    EXPECT_EQ(2u, osContext->getDeviceBitfield().count());

    device->getDefaultEngine().commandStreamReceiver->setupContext(*osContext);
    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device->getDefaultEngine().commandStreamReceiver);

    EXPECT_EQ(1u, directSubmission.activeTiles);
    EXPECT_FALSE(directSubmission.partitionedMode);
    EXPECT_TRUE(directSubmission.partitionConfigSet);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);
}
