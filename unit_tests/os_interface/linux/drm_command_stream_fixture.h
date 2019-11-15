/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/command_stream/preemption.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/linux/mock_drm_command_stream_receiver.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "gmock/gmock.h"

#include <algorithm>

class DrmCommandStreamTest : public ::testing::Test {
  public:
    template <typename GfxFamily>
    void SetUpT() {

        //make sure this is disabled, we don't want to test this now
        DebugManager.flags.EnableForcePin.set(false);

        mock = std::make_unique<::testing::NiceMock<DrmMockImpl>>(mockFd);

        executionEnvironment.setHwInfo(*platformDevices);
        executionEnvironment.prepareRootDeviceEnvironments(1);
        executionEnvironment.osInterface = std::make_unique<OSInterface>();
        executionEnvironment.osInterface->get()->setDrm(mock.get());

        osContext = std::make_unique<OsContextLinux>(*mock, 0u, 1, HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0],
                                                     PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);

        csr = new DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, 0, gemCloseWorkerMode::gemCloseWorkerActive);
        ASSERT_NE(nullptr, csr);
        csr->setupContext(*osContext);

        // Memory manager creates pinBB with ioctl, expect one call
        EXPECT_CALL(*mock, ioctl(::testing::_, ::testing::_))
            .Times(1);
        memoryManager = new DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerActive,
                                             DebugManager.flags.EnableForcePin.get(),
                                             true,
                                             executionEnvironment);
        executionEnvironment.memoryManager.reset(memoryManager);
        ::testing::Mock::VerifyAndClearExpectations(mock.get());

        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
    }

    template <typename GfxFamily>
    void TearDownT() {
        memoryManager->waitForDeletions();
        memoryManager->peekGemCloseWorker()->close(true);
        delete csr;
        ::testing::Mock::VerifyAndClearExpectations(mock.get());
        // Memory manager closes pinBB with ioctl, expect one call
        EXPECT_CALL(*mock, ioctl(::testing::_, ::testing::_))
            .Times(::testing::AtLeast(1));
    }

    CommandStreamReceiver *csr = nullptr;
    DrmMemoryManager *memoryManager = nullptr;
    std::unique_ptr<::testing::NiceMock<DrmMockImpl>> mock;
    const int mockFd = 33;
    static const uint64_t alignment = MemoryConstants::allocationAlignment;
    DebugManagerStateRestore dbgState;
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<OsContextLinux> osContext;
};

class DrmCommandStreamEnhancedTest : public ::testing::Test {
  public:
    std::unique_ptr<DebugManagerStateRestore> dbgState;
    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<DrmMockCustom> mock;
    CommandStreamReceiver *csr = nullptr;

    DrmMemoryManager *mm = nullptr;
    std::unique_ptr<MockDevice> device;

    template <typename GfxFamily>
    void SetUpT() {
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->incRefInternal();
        executionEnvironment->setHwInfo(*platformDevices);
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->initGmm();
        this->dbgState = std::make_unique<DebugManagerStateRestore>();
        //make sure this is disabled, we don't want to test this now
        DebugManager.flags.EnableForcePin.set(false);

        mock = std::make_unique<DrmMockCustom>();
        executionEnvironment->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->osInterface->get()->setDrm(mock.get());

        csr = new TestedDrmCommandStreamReceiver<GfxFamily>(*executionEnvironment);
        ASSERT_NE(nullptr, csr);
        mm = new DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive,
                                  DebugManager.flags.EnableForcePin.get(),
                                  true,
                                  *executionEnvironment);
        ASSERT_NE(nullptr, mm);
        executionEnvironment->memoryManager.reset(mm);
        device.reset(MockDevice::create<MockDevice>(executionEnvironment, 0u));
        device->resetCommandStreamReceiver(csr);
        ASSERT_NE(nullptr, device);
    }

    template <typename GfxFamily>
    void TearDownT() {
        executionEnvironment->decRefInternal();
    }

    template <typename GfxFamily>
    void makeResidentBufferObjects(const DrmAllocation *drmAllocation) {
        static_cast<TestedDrmCommandStreamReceiver<GfxFamily> *>(csr)->makeResidentBufferObjects(drmAllocation);
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
        friend DrmCommandStreamEnhancedTest;

      protected:
        MockBufferObject(Drm *drm, size_t size) : BufferObject(drm, 1, 0) {
            this->size = alignUp(size, 4096);
        }
    };

    MockBufferObject *createBO(size_t size) {
        return new MockBufferObject(this->mock.get(), size);
    }
};
