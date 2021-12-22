/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_tests.h"
#include "shared/test/common/test_macros/test.h"

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
        osContext = std::make_unique<OsContextLinux>(*executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>(), 0u,
                                                     EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                  PreemptionMode::ThreadGroup, device->getDeviceBitfield()));
        osContext->ensureContextInitialized();
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
    using BaseClass::currentTagData;
    using BaseClass::disableMonitorFence;
    using BaseClass::dispatchSwitchRingBufferSection;
    using BaseClass::DrmDirectSubmission;
    using BaseClass::getSizeNewResourceHandler;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::getTagAddressValue;
    using BaseClass::handleNewResourcesSubmission;
    using BaseClass::handleResidency;
    using BaseClass::isNewResourceHandleNeeded;
    using BaseClass::partitionConfigSet;
    using BaseClass::partitionedMode;
    using BaseClass::postSyncOffset;
    using BaseClass::ringStart;
    using BaseClass::submit;
    using BaseClass::switchRingBuffers;
    using BaseClass::tagAddress;
    using BaseClass::updateTagValue;
    using BaseClass::useNotifyForPostSync;
    using BaseClass::wait;
    using BaseClass::workPartitionAllocation;

    MockDrmDirectSubmission(Device &device, OsContext &osContext) : DrmDirectSubmission<GfxFamily, Dispatcher>(device, osContext) {
        this->disableMonitorFence = false;
    }
};

using namespace NEO;

HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenCallingLinuxImplementationThenExpectInitialImplementationValues) {
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device.get(),
                                                                                          *osContext.get());

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

HWTEST_F(DrmDirectSubmissionTest, whenCreateDirectSubmissionThenValidObjectIsReturned) {
    auto directSubmission = DirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>::create(*device.get(),
                                                                                                 *osContext.get());
    EXPECT_NE(directSubmission.get(), nullptr);

    bool ret = directSubmission->initialize(false, false);
    EXPECT_TRUE(ret);
}

HWTEST_F(DrmDirectSubmissionTest, givenDisabledMonitorFenceWhenDispatchSwitchRingBufferThenDispatchPipeControl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());
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

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());
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

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());

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

HWTEST_F(DrmDirectSubmissionTest, givenNewResourceBoundhWhenDispatchCommandBufferThenTlbIsFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionNewResourceTlbFlush.set(-1);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    osContext->setNewResourceBound(true);

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), sizeof(PIPE_CONTROL));

    directSubmission.handleNewResourcesSubmission();

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_TRUE(pipeControl->getTlbInvalidate());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_FALSE(osContext->getNewResourceBound());

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), 0u);
}

HWTEST_F(DrmDirectSubmissionTest, givenNoNewResourceBoundhWhenDispatchCommandBufferThenTlbIsNotFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionNewResourceTlbFlush.set(-1);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    osContext->setNewResourceBound(false);

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), 0u);

    directSubmission.handleNewResourcesSubmission();

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_EQ(pipeControl, nullptr);
    EXPECT_FALSE(osContext->getNewResourceBound());

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), 0u);
}

HWTEST_F(DrmDirectSubmissionTest, givenDirectSubmissionNewResourceTlbFlusZeroAndNewResourceBoundhWhenDispatchCommandBufferThenTlbIsNotFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using Dispatcher = RenderDispatcher<FamilyType>;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionNewResourceTlbFlush.set(0);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);

    osContext->setNewResourceBound(true);

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), 0u);

    directSubmission.handleNewResourcesSubmission();

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(directSubmission.ringCommandStream, 0);
    hwParse.findHardwareCommands<FamilyType>();
    auto *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_EQ(pipeControl, nullptr);
    EXPECT_FALSE(osContext->getNewResourceBound());

    EXPECT_EQ(directSubmission.getSizeNewResourceHandler(), 0u);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmDirectSubmissionTest, givenMultipleActiveTilesWhenWaitingForTagUpdateThenQueryAllActiveTiles) {
    using Dispatcher = RenderDispatcher<FamilyType>;

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());

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

    osContext = std::make_unique<OsContextLinux>(*executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>(), 0u,
                                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                              PreemptionMode::ThreadGroup, device->getDeviceBitfield()));
    osContext->ensureContextInitialized();
    EXPECT_EQ(2u, osContext->getDeviceBitfield().count());

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*device);

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());

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

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());

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

    osContext = std::make_unique<OsContextLinux>(*executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>(), 0u,
                                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                              PreemptionMode::ThreadGroup, device->getDeviceBitfield()));
    osContext->ensureContextInitialized();
    EXPECT_EQ(2u, osContext->getDeviceBitfield().count());

    MockDrmDirectSubmission<FamilyType, Dispatcher> directSubmission(*device.get(),
                                                                     *osContext.get());

    EXPECT_EQ(1u, directSubmission.activeTiles);
    EXPECT_FALSE(directSubmission.partitionedMode);
    EXPECT_TRUE(directSubmission.partitionConfigSet);

    bool ret = directSubmission.allocateResources();
    EXPECT_TRUE(ret);
}
