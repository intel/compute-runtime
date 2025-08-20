/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"

#include "gtest/gtest.h"

#include <algorithm>

template <typename GfxFamily>
struct MockDrmCsr : public DrmCommandStreamReceiver<GfxFamily> {
    using DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver;
    using DrmCommandStreamReceiver<GfxFamily>::dispatchMode;
    using DrmCommandStreamReceiver<GfxFamily>::completionFenceValuePointer;
    using DrmCommandStreamReceiver<GfxFamily>::flushInternal;
    using DrmCommandStreamReceiver<GfxFamily>::CommandStreamReceiver::taskCount;
};

class DrmCommandStreamTest : public ::testing::Test {
  public:
    template <typename GfxFamily>
    void setUpT() {
        // make sure this is disabled, we don't want to test this now
        debugManager.flags.EnableL3FlushAfterPostSync.set(0);
        debugManager.flags.EnableForcePin.set(false);

        mock = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
        auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
        mock->setupIoctlHelper(hwInfo->platform.eProductFamily);

        executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
        executionEnvironment.rootDeviceEnvironments[0]->initGmm();
        gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();

        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        mock->createVirtualMemoryAddressSpace(GfxCoreHelper::getSubDevicesCount(hwInfo));
        osContext = std::make_unique<OsContextLinux>(*mock, 0, 0u,
                                                     EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                  PreemptionHelper::getDefaultPreemptionMode(*hwInfo)));
        osContext->ensureContextInitialized(false);

        csr = new MockDrmCsr<GfxFamily>(executionEnvironment, 0, 1);
        ASSERT_NE(nullptr, csr);
        csr->setupContext(*osContext);

        mock->ioctlCallsCount = 0u;
        mock->memoryInfo.reset(new MockMemoryInfo{*mock});
        memoryManager = new DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerActive,
                                             debugManager.flags.EnableForcePin.get(),
                                             true,
                                             executionEnvironment);
        executionEnvironment.memoryManager.reset(memoryManager);
        mock->memoryInfo.reset();
        // Memory manager creates pinBB with ioctl, expect one call
        EXPECT_EQ(1u, mock->ioctlCallsCount);

        // assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        mock->ioctlCount.reset();
        mock->ioctlTearDownExpected.reset();
    }

    template <typename GfxFamily>
    void tearDownT() {
        memoryManager->waitForDeletions();
        if (memoryManager->peekGemCloseWorker()) {
            memoryManager->peekGemCloseWorker()->close(true);
        }
        delete csr;
        if (mock->ioctlTearDownExpects) {
            EXPECT_EQ(mock->ioctlCount.gemWait, mock->ioctlTearDownExpected.gemWait);
            EXPECT_EQ(mock->ioctlCount.gemClose, mock->ioctlTearDownExpected.gemClose);
        }
        // Expect 1 call with DRM_IOCTL_I915_GEM_CONTEXT_DESTROY request on destroyDrmContext
        // Expect 1 call with DRM_IOCTL_GEM_CLOSE request on BufferObject close
        mock->expectedIoctlCallsOnDestruction = mock->ioctlCallsCount + 2 + static_cast<uint32_t>(mock->virtualMemoryIds.size());
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
    GmmHelper *gmmHelper = nullptr;
};

template <typename DrmType>
class DrmCommandStreamEnhancedTemplate : public ::testing::Test {
  public:
    VariableBackup<bool> mockSipBackup{&MockSipData::useMockSip, false};
    std::unique_ptr<DebugManagerStateRestore> dbgState;
    MockExecutionEnvironment *executionEnvironment;
    DrmType *mock;
    CommandStreamReceiver *csr = nullptr;
    const uint32_t rootDeviceIndex = 0u;

    DrmMemoryManager *mm = nullptr;
    std::unique_ptr<MockDevice> device;

    template <typename GfxFamily>
    void setUpT() {
        executionEnvironment = new MockExecutionEnvironment();
        executionEnvironment->incRefInternal();
        executionEnvironment->initGmm();
        this->dbgState = std::make_unique<DebugManagerStateRestore>();
        // make sure this is disabled, we don't want to test this now
        debugManager.flags.EnableForcePin.set(false);
        debugManager.flags.EnableL3FlushAfterPostSync.set(0);

        mock = DrmType::create(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]).release();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, rootDeviceIndex, false);

        csr = new TestedDrmCommandStreamReceiver<GfxFamily>(*executionEnvironment, rootDeviceIndex, 1);
        ASSERT_NE(nullptr, csr);
        mm = new DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive,
                                  debugManager.flags.EnableForcePin.get(),
                                  true,
                                  *executionEnvironment);
        ASSERT_NE(nullptr, mm);
        executionEnvironment->memoryManager.reset(mm);
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
        executionEnvironment->initializeMemoryManager();
        device.reset(MockDevice::create<MockDevice>(executionEnvironment, rootDeviceIndex));
        device->resetCommandStreamReceiver(csr);
        ASSERT_NE(nullptr, device);
    }

    template <typename GfxFamily>
    void tearDownT() {
        executionEnvironment->decRefInternal();
        device.reset();
    }

    template <typename GfxFamily>
    void makeResidentBufferObjects(OsContext *osContext, DrmAllocation *drmAllocation) {
        drmAllocation->bindBOs(osContext, 0u, &static_cast<TestedDrmCommandStreamReceiver<GfxFamily> *>(csr)->residency, false, false);
    }

    template <typename GfxFamily>
    bool isResident(BufferObject *bo) const {
        auto &residency = this->getResidencyVector<GfxFamily>();
        return std::find(residency.begin(), residency.end(), bo) != residency.end();
    }

    template <typename GfxFamily>
    const std::vector<BufferObject *> &getResidencyVector() const {
        return static_cast<const TestedDrmCommandStreamReceiver<GfxFamily> *>(csr)->residency;
    }

  protected:
    class MockBufferObject : public BufferObject {
        friend DrmCommandStreamEnhancedTemplate<DrmType>;

      protected:
        MockBufferObject(uint32_t rootDeviceIndex, Drm *drm, size_t size) : BufferObject(rootDeviceIndex, drm, CommonConstants::unsupportedPatIndex, 1, 0, 16u) {
            this->size = alignUp(size, 4096);
        }
    };

    MockBufferObject *createBO(size_t size) {
        return new MockBufferObject(0, this->mock, size);
    }
};

using DrmCommandStreamEnhancedTest = DrmCommandStreamEnhancedTemplate<DrmMockCustom>;

template <typename T>
class DrmCommandStreamEnhancedWithFailingExecTemplate : public ::testing::Test {
  public:
    std::unique_ptr<DebugManagerStateRestore> dbgState;
    MockExecutionEnvironment *executionEnvironment;
    T *mock;
    CommandStreamReceiver *csr = nullptr;
    const uint32_t rootDeviceIndex = 0u;

    DrmMemoryManager *mm = nullptr;
    std::unique_ptr<MockDevice> device;

    template <typename GfxFamily>
    void setUpT() {
        executionEnvironment = new MockExecutionEnvironment();
        executionEnvironment->incRefInternal();
        executionEnvironment->initGmm();
        this->dbgState = std::make_unique<DebugManagerStateRestore>();
        // make sure this is disabled, we don't want to test this now
        debugManager.flags.EnableForcePin.set(false);

        mock = T::create(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]).release();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, rootDeviceIndex, false);

        csr = new TestedDrmCommandStreamReceiverWithFailingExec<GfxFamily>(*executionEnvironment, rootDeviceIndex, 1);
        ASSERT_NE(nullptr, csr);
        mm = new DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive,
                                  debugManager.flags.EnableForcePin.get(),
                                  true,
                                  *executionEnvironment);
        ASSERT_NE(nullptr, mm);
        executionEnvironment->memoryManager.reset(mm);
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
        executionEnvironment->initializeMemoryManager();
        device.reset(MockDevice::create<MockDevice>(executionEnvironment, rootDeviceIndex));
        device->resetCommandStreamReceiver(csr);
        ASSERT_NE(nullptr, device);
    }

    template <typename GfxFamily>
    void tearDownT() {
        executionEnvironment->decRefInternal();
    }

    template <typename GfxFamily>
    void makeResidentBufferObjects(OsContext *osContext, DrmAllocation *drmAllocation) {
        drmAllocation->bindBOs(osContext, 0u, &static_cast<TestedDrmCommandStreamReceiver<GfxFamily> *>(csr)->residency, false, false);
    }

    template <typename GfxFamily>
    bool isResident(BufferObject *bo) const {
        auto &residency = this->getResidencyVector<GfxFamily>();
        return std::find(residency.begin(), residency.end(), bo) != residency.end();
    }

    template <typename GfxFamily>
    const std::vector<BufferObject *> &getResidencyVector() const {
        return static_cast<const TestedDrmCommandStreamReceiver<GfxFamily> *>(csr)->residency;
    }
};

using DrmCommandStreamEnhancedWithFailingExec = DrmCommandStreamEnhancedWithFailingExecTemplate<DrmMockCustom>;

struct DrmCommandStreamDirectSubmissionTest : public DrmCommandStreamEnhancedTest {
    template <typename GfxFamily>
    void setUpT() {
        debugManager.flags.EnableDirectSubmission.set(1u);
        debugManager.flags.DirectSubmissionFlatRingBuffer.set(0);
        DrmCommandStreamEnhancedTest::setUpT<GfxFamily>();
        auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto engineType = device->getDefaultEngine().osContext->getEngineType();
        hwInfo->capabilityTable.directSubmissionEngines.data[engineType].engineSupported = true;
        device->finalizeRayTracing();
        csr->initDirectSubmission();
    }

    template <typename GfxFamily>
    void tearDownT() {
        csr->stopDirectSubmission(false, false);
        this->dbgState.reset();
        DrmCommandStreamEnhancedTest::tearDownT<GfxFamily>();
    }

    DebugManagerStateRestore restorer;
};