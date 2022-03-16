/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include "drm/i915_drm.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} //namespace NEO
using namespace NEO;

class DrmCommandStreamTestL0 : public ::testing::Test {
  public:
    template <typename GfxFamily>
    void SetUpT() {

        //make sure this is disabled, we don't want to test this now
        DebugManager.flags.EnableForcePin.set(false);
        auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();

        mock = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
        mock->setupIoctlHelper(hwInfo->platform.eProductFamily);

        executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

        mock->createVirtualMemoryAddressSpace(HwHelper::getSubDevicesCount(hwInfo));
        osContext = std::make_unique<OsContextLinux>(*mock, 0u,
                                                     EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0],
                                                                                                  PreemptionHelper::getDefaultPreemptionMode(*hwInfo)));
        osContext->ensureContextInitialized();

        csr = new DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerActive);
        ASSERT_NE(nullptr, csr);
        csr->setupContext(*osContext);

        mock->ioctlCallsCount = 0u;
        memoryManager = new DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerActive,
                                             DebugManager.flags.EnableForcePin.get(),
                                             true,
                                             executionEnvironment);
        executionEnvironment.memoryManager.reset(memoryManager);
        // Memory manager creates pinBB with ioctl, expect one call
        EXPECT_EQ(1u, mock->ioctlCallsCount);

        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
    }

    template <typename GfxFamily>
    void TearDownT() {
        memoryManager->waitForDeletions();
        memoryManager->peekGemCloseWorker()->close(true);
        delete csr;
        // Expect 1 call with DRM_IOCTL_I915_GEM_CONTEXT_DESTROY request on destroyDrmContext
        // Expect 1 call with DRM_IOCTL_GEM_CLOSE request on BufferObject close
        mock->expectedIoctlCallsOnDestruction = mock->ioctlCallsCount + 2;
        mock->expectIoctlCallsOnDestruction = true;
    }

    CommandStreamReceiver *csr = nullptr;
    DrmMemoryManager *memoryManager = nullptr;
    DrmMock *mock = nullptr;
    const int mockFd = 33;
    static const uint64_t alignment = MemoryConstants::allocationAlignment;
    DebugManagerStateRestore dbgState;
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<OsContextLinux> osContext;
};

template <typename GfxFamily>
struct MockDrmCsrL0 : public DrmCommandStreamReceiver<GfxFamily> {
    using DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver;
    using DrmCommandStreamReceiver<GfxFamily>::dispatchMode;
};

HWTEST_TEMPLATED_F(DrmCommandStreamTestL0, givenL0ApiConfigWhenCreatingDrmCsrThenEnableImmediateDispatch) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    MockDrmCsrL0<FamilyType> csr(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);
}
