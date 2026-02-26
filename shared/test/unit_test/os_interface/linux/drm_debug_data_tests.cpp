/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/xe/eudebug/mock_eudebug_interface.h"
#include "shared/test/common/os_interface/linux/xe/mock_ioctl_helper_xe.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
struct DebugDataTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

        auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(*drm);
        xeIoctlHelper->euDebugInterface = std::make_unique<MockEuDebugInterface>();
        auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
        static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::upstream);
        drm->ioctlHelper.reset(xeIoctlHelper.release());

        ioctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

        deviceBitfield = DeviceBitfield(1 << 3);
    }

    void TearDown() override {
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    DrmMock *drm = nullptr;
    MockIoctlHelperXe *ioctlHelper = nullptr;
    DeviceBitfield deviceBitfield;
};

TEST_F(DebugDataTest, givenUpstreamDebuggerWhenCallBindAddDebugDataThenIoctlCalledWithCorrectDataAndFenceIsProgrammedAndValueIsUpdated) {

    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    drm->pagingFence[0] = 0x12345;
    drm->requirePerContextVM = false;
    ioctlHelper->mockCreateDrmContext = true;
    ioctlHelper->mockAddDebugDataAndCreateBindOpVec = true;
    ioctlHelper->mockBindAddDebugData = true;
    ioctlHelper->fenceAddr = castToUint64(&(drm->pagingFence[0]));
    ioctlHelper->fenceVal = initFenceValue;

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    std::unique_ptr<NEO::Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment.release(), 0));

    MockOsContextLinux osContext(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, 1);

    auto ret = drm->bindAddDebugData(static_cast<NEO::OsContext *>(&osContext), &bo, 0, true);

    EXPECT_EQ(0, ret);

    EXPECT_TRUE(ioctlHelper->addDebugDataAndCreateBindOpVecCalled);
    EXPECT_TRUE(ioctlHelper->bindAddDebugDataCalled);
    EXPECT_EQ(drm->fenceVal[0], initFenceValue + 1);

    EXPECT_EQ(ioctlHelper->bindVmId, 0u);
    EXPECT_TRUE(ioctlHelper->bindAdd);

    auto &debugDataVec = ioctlHelper->bindAddDebugDataVec;
    EXPECT_EQ(debugDataVec.size(), 1u);

    EXPECT_EQ(debugDataVec[0].base.name, 0u);
    EXPECT_EQ(debugDataVec[0].base.nextExtension, 0u);
    EXPECT_EQ(debugDataVec[0].base.pad, 0u);
    EXPECT_EQ(debugDataVec[0].addr, 0x1234u);
    EXPECT_EQ(debugDataVec[0].range, 0x1000u);
    EXPECT_EQ(debugDataVec[0].flags, 1u);
    EXPECT_EQ(debugDataVec[0].pseudopath, 0x1u);

    auto vmBindExtUserFence = ioctlHelper->bindAddExtUserFence;
    EXPECT_NE(vmBindExtUserFence, nullptr);
    auto userFenceExtension = reinterpret_cast<MockIoctlHelperXe::UserFenceExtension *>(vmBindExtUserFence);
    EXPECT_EQ(userFenceExtension->addr, castToUint64(&drm->pagingFence[0]));
    EXPECT_EQ(userFenceExtension->value, initFenceValue + 1);

    EXPECT_EQ(ioctlHelper->addDebugDataVmId, 0u);
}

TEST_F(DebugDataTest, givenUpstreamDebuggerAndPerContextVMRequiredWhenCallBindAddDebugDataThenIoctlCalledWithCorrectDataAndFenceIsProgrammedAndValueIsUpdated) {

    uint64_t initFenceValue = 10u;
    drm->requirePerContextVM = true;
    ioctlHelper->mockCreateDrmContext = true;
    ioctlHelper->mockAddDebugDataAndCreateBindOpVec = true;
    ioctlHelper->mockBindAddDebugData = true;

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    std::unique_ptr<NEO::Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment.release(), 0));

    MockOsContextLinux osContext(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    osContext.drmVmIds = {20, 1, 2, 3};

    osContext.pagingFence[0] = 0x12345;
    osContext.fenceVal[0] = initFenceValue;
    static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->fenceAddr = castToUint64(&osContext.pagingFence[0]);
    static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->fenceVal = initFenceValue;
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, 1);
    auto ret = drm->bindAddDebugData(static_cast<NEO::OsContext *>(&osContext), &bo, 0, true);

    EXPECT_EQ(0, ret);
    EXPECT_TRUE(ioctlHelper->addDebugDataAndCreateBindOpVecCalled);
    EXPECT_TRUE(ioctlHelper->bindAddDebugDataCalled);

    EXPECT_EQ(ioctlHelper->bindVmId, 20u);

    auto &debugDataVec = ioctlHelper->bindAddDebugDataVec;
    EXPECT_EQ(debugDataVec.size(), 1u);

    EXPECT_EQ(debugDataVec[0].base.name, 0u);
    EXPECT_EQ(debugDataVec[0].base.nextExtension, 0u);
    EXPECT_EQ(debugDataVec[0].base.pad, 0u);
    EXPECT_EQ(debugDataVec[0].addr, 0x1234u);
    EXPECT_EQ(debugDataVec[0].range, 0x1000u);
    EXPECT_EQ(debugDataVec[0].flags, 1u);
    EXPECT_EQ(debugDataVec[0].pseudopath, 0x1u);

    auto vmBindExtUserFence = ioctlHelper->bindAddExtUserFence;
    EXPECT_NE(vmBindExtUserFence, nullptr);
    auto userFenceExtension = reinterpret_cast<MockIoctlHelperXe::UserFenceExtension *>(vmBindExtUserFence);
    EXPECT_EQ(userFenceExtension->addr, castToUint64(&osContext.pagingFence[0]));
    EXPECT_EQ(userFenceExtension->value, initFenceValue + 1);

    EXPECT_EQ(ioctlHelper->addDebugDataVmId, 20u);
}

TEST_F(DebugDataTest, givenUpstreamDebuggerWhenCallBindAddDebugDataAndIsAddFalseThenCallSucceedsAndFenceValueIsNotIncremented) {

    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    drm->requirePerContextVM = false;
    ioctlHelper->mockAddDebugDataAndCreateBindOpVec = true;
    ioctlHelper->mockBindAddDebugData = true;
    ioctlHelper->mockCreateDrmContext = true;

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    std::unique_ptr<NEO::Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment.release(), 0));

    MockOsContextLinux osContext(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, 1);

    auto ret = drm->bindAddDebugData(static_cast<NEO::OsContext *>(&osContext), &bo, 0, false);

    EXPECT_EQ(0, ret);
    EXPECT_TRUE(ioctlHelper->addDebugDataAndCreateBindOpVecCalled);
    EXPECT_TRUE(ioctlHelper->bindAddDebugDataCalled);
    EXPECT_EQ(drm->fenceVal[0], initFenceValue);

    EXPECT_EQ(ioctlHelper->bindVmId, 0u);
    EXPECT_EQ(ioctlHelper->addDebugDataVmId, 0u);
}

TEST_F(DebugDataTest, givenUpstreamDebuggerAndPerContextVmRequiredWhenCallBindAddDebugDataAndIsAddFalseThenCallSucceedsAndFenceValueIsNotIncremented) {

    uint64_t initFenceValue = 10u;
    drm->requirePerContextVM = true;
    ioctlHelper->mockAddDebugDataAndCreateBindOpVec = true;
    ioctlHelper->mockBindAddDebugData = true;
    ioctlHelper->mockCreateDrmContext = true;

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    std::unique_ptr<NEO::Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment.release(), 0));

    MockOsContextLinux osContext(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    osContext.drmVmIds = {20, 1, 2, 3};
    osContext.pagingFence[0] = 0x12345;
    osContext.fenceVal[0] = initFenceValue;
    static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->fenceAddr = castToUint64(&osContext.pagingFence[0]);
    static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->fenceVal = initFenceValue;

    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, 1);
    auto ret = drm->bindAddDebugData(static_cast<NEO::OsContext *>(&osContext), &bo, 0, false);

    EXPECT_EQ(0, ret);
    EXPECT_TRUE(ioctlHelper->addDebugDataAndCreateBindOpVecCalled);
    EXPECT_TRUE(ioctlHelper->bindAddDebugDataCalled);
    EXPECT_EQ(osContext.fenceVal[0], initFenceValue);

    EXPECT_EQ(ioctlHelper->bindVmId, 20u);
    EXPECT_EQ(ioctlHelper->addDebugDataVmId, 20u);
}

TEST_F(DebugDataTest, givenUpstreamDebuggerWhenCallBindAddDebugDataAndCreateBindOpVecReturnsNulloptThenReturnZeroAndIoctlBindAddDebugDataNotCalled) {

    drm->requirePerContextVM = false;
    ioctlHelper->mockAddDebugDataAndCreateBindOpVec = true;
    ioctlHelper->mockBindAddDebugData = true;
    ioctlHelper->mockCreateDrmContext = true;
    ioctlHelper->failAddDebugDataAndCreateBindOpVec = true;

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    std::unique_ptr<NEO::Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment.release(), 0));

    MockOsContextLinux osContext(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, 1);

    auto ret = drm->bindAddDebugData(static_cast<NEO::OsContext *>(&osContext), &bo, 0, false);

    EXPECT_EQ(0, ret);
    EXPECT_TRUE(static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->addDebugDataAndCreateBindOpVecCalled);
    EXPECT_FALSE(static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->bindAddDebugDataCalled);
    EXPECT_EQ(static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->bindVmId, 0u);
    EXPECT_FALSE(static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->bindAdd);
}

TEST_F(DebugDataTest, givenUpstreamDebuggerWhenIoctlBindAddDebugDataFailsWhenCallBindAddDebugDataThenIoctlErrorReturned) {

    drm->requirePerContextVM = false;
    ioctlHelper->mockCreateDrmContext = true;
    ioctlHelper->failBindAddDebugData = true;
    ioctlHelper->mockAddDebugDataAndCreateBindOpVec = true;

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    std::unique_ptr<NEO::Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment.release(), 0));

    MockOsContextLinux osContext(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, 1);

    auto ret = drm->bindAddDebugData(static_cast<NEO::OsContext *>(&osContext), &bo, 0, true);

    EXPECT_EQ(-1, ret);
    EXPECT_TRUE(static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->addDebugDataAndCreateBindOpVecCalled);
    EXPECT_TRUE(static_cast<MockIoctlHelperXe *>(drm->ioctlHelper.get())->bindAddDebugDataCalled);

    // Expect still return error when removing debug data
    ret = drm->bindAddDebugData(static_cast<NEO::OsContext *>(&osContext), &bo, 0, false);
    EXPECT_EQ(-1, ret);
}
