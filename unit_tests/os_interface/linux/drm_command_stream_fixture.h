/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/linux/mock_drm_command_stream_receiver.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "gmock/gmock.h"

class DrmCommandStreamFixture {
  public:
    void SetUp() {

        //make sure this is disabled, we don't want to test this now
        DebugManager.flags.EnableForcePin.set(false);

        mock = std::make_unique<::testing::NiceMock<DrmMockImpl>>(mockFd);

        executionEnvironment.setHwInfo(*platformDevices);
        executionEnvironment.osInterface = std::make_unique<OSInterface>();
        executionEnvironment.osInterface->get()->setDrm(mock.get());

        osContext = std::make_unique<OsContextLinux>(*mock, 0u, 1, HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0],
                                                     PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);

        csr = new DrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(executionEnvironment, gemCloseWorkerMode::gemCloseWorkerActive);
        ASSERT_NE(nullptr, csr);
        executionEnvironment.commandStreamReceivers.resize(1);
        executionEnvironment.commandStreamReceivers[0].push_back(std::unique_ptr<CommandStreamReceiver>(csr));
        csr->setupContext(*osContext);

        // Memory manager creates pinBB with ioctl, expect one call
        EXPECT_CALL(*mock, ioctl(::testing::_, ::testing::_))
            .Times(1);
        memoryManager = new DrmMemoryManager(gemCloseWorkerActive,
                                             DebugManager.flags.EnableForcePin.get(),
                                             true,
                                             executionEnvironment);
        executionEnvironment.memoryManager.reset(memoryManager);
        ::testing::Mock::VerifyAndClearExpectations(mock.get());

        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
    }

    void TearDown() {
        memoryManager->waitForDeletions();
        memoryManager->peekGemCloseWorker()->close(true);
        executionEnvironment.commandStreamReceivers.clear();
        ::testing::Mock::VerifyAndClearExpectations(mock.get());
        // Memory manager closes pinBB with ioctl, expect one call
        EXPECT_CALL(*mock, ioctl(::testing::_, ::testing::_))
            .Times(::testing::AtLeast(1));
    }

    DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *csr = nullptr;
    DrmMemoryManager *memoryManager = nullptr;
    std::unique_ptr<::testing::NiceMock<DrmMockImpl>> mock;
    const int mockFd = 33;
    static const uint64_t alignment = MemoryConstants::allocationAlignment;
    DebugManagerStateRestore dbgState;
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<OsContextLinux> osContext;
};

class DrmCommandStreamEnhancedFixture {
  public:
    std::unique_ptr<DebugManagerStateRestore> dbgState;
    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<DrmMockCustom> mock;
    DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *csr = nullptr;

    DrmMemoryManager *mm = nullptr;
    std::unique_ptr<MockDevice> device;

    virtual void SetUp() {
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->incRefInternal();
        executionEnvironment->setHwInfo(*platformDevices);
        executionEnvironment->initGmm();
        this->dbgState = std::make_unique<DebugManagerStateRestore>();
        //make sure this is disabled, we don't want to test this now
        DebugManager.flags.EnableForcePin.set(false);

        mock = std::make_unique<DrmMockCustom>();
        executionEnvironment->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->osInterface->get()->setDrm(mock.get());

        tCsr = new TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*executionEnvironment);
        csr = tCsr;
        ASSERT_NE(nullptr, csr);
        mm = new DrmMemoryManager(gemCloseWorkerInactive,
                                  DebugManager.flags.EnableForcePin.get(),
                                  true,
                                  *executionEnvironment);
        ASSERT_NE(nullptr, mm);
        executionEnvironment->memoryManager.reset(mm);
        device.reset(MockDevice::create<MockDevice>(executionEnvironment, 0u));
        device->resetCommandStreamReceiver(tCsr);
        ASSERT_NE(nullptr, device);
    }

    virtual void TearDown() {
        executionEnvironment->decRefInternal();
    }

    void makeResidentBufferObjects(const DrmAllocation *drmAllocation) {
        tCsr->makeResidentBufferObjects(drmAllocation);
    }

    bool isResident(BufferObject *bo) {
        return tCsr->isResident(bo);
    }

    const BufferObject *getResident(BufferObject *bo) {
        return tCsr->getResident(bo);
    }

  protected:
    TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *tCsr = nullptr;

    class MockBufferObject : public BufferObject {
        friend DrmCommandStreamEnhancedFixture;

      protected:
        MockBufferObject(Drm *drm, size_t size) : BufferObject(drm, 1) {
            this->size = alignUp(size, 4096);
        }
    };

    MockBufferObject *createBO(size_t size) {
        return new MockBufferObject(this->mock.get(), size);
    }
};
