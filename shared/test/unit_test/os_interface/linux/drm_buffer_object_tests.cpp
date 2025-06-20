/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/os_interface/linux/drm_buffer_object_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using DrmMockBufferObjectFixture = DrmBufferObjectFixture<DrmMockCustom>;
using DrmBufferObjectTest = Test<DrmMockBufferObjectFixture>;

TEST_F(DrmBufferObjectTest, WhenCallingExecThenReturnIsCorrect) {
    mock->ioctlExpected.total = 1;
    mock->ioctlRes = 0;

    ExecObject execObjectsStorage = {};
    auto ret = bo->exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0);
    EXPECT_EQ(mock->ioctlRes, ret);
    EXPECT_EQ(0u, mock->execBuffer.getFlags());
}

TEST_F(DrmBufferObjectTest, GivenInvalidParamsWhenCallingExecThenEfaultIsReturned) {
    mock->ioctlExpected.total = 3;
    mock->ioctlRes = -1;
    mock->errnoValue = EFAULT;
    ExecObject execObjectsStorage = {};
    EXPECT_EQ(EFAULT, bo->exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0));
}

TEST_F(DrmBufferObjectTest, GivenDetectedGpuHangDuringEvictUnusedAllocationsWhenCallingExecGpuHangErrorCodeIsRetrurned) {
    mock->ioctlExpected.total = 2;
    mock->ioctlRes = -1;
    mock->errnoValue = EFAULT;

    bo->callBaseEvictUnusedAllocations = false;

    ExecObject execObjectsStorage = {};
    const auto result = bo->exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0);

    EXPECT_EQ(BufferObject::gpuHangDetected, result);
}

TEST_F(DrmBufferObjectTest, WhenSettingTilingThenCallSucceeds) {
    mock->ioctlExpected.total = 1; // set_tiling
    auto tilingY = mock->getIoctlHelper()->getDrmParamValue(DrmParam::tilingY);
    auto ret = bo->setTiling(tilingY, 0);
    EXPECT_TRUE(ret);
}

TEST_F(DrmBufferObjectTest, WhenSettingSameTilingThenCallSucceeds) {
    mock->ioctlExpected.total = 0; // set_tiling
    auto tilingY = mock->getIoctlHelper()->getDrmParamValue(DrmParam::tilingY);
    bo->tilingMode = tilingY;
    auto ret = bo->setTiling(tilingY, 0);
    EXPECT_TRUE(ret);
}

TEST_F(DrmBufferObjectTest, GivenInvalidTilingWhenSettingTilingThenCallFails) {
    mock->ioctlExpected.total = 1; // set_tiling
    auto tilingY = mock->getIoctlHelper()->getDrmParamValue(DrmParam::tilingY);
    mock->ioctlRes = -1;
    auto ret = bo->setTiling(tilingY, 0);
    EXPECT_FALSE(ret);
}

TEST_F(DrmBufferObjectTest, givenBindAvailableWhenCallWaitThenNoIoctlIsCalled) {
    mock->bindAvailable = true;
    mock->ioctlExpected.total = 0;
    auto ret = bo->wait(-1);
    EXPECT_FALSE(ret);
}

TEST_F(DrmBufferObjectTest, givenAddressThatWhenSizeIsAddedCrosses32BitBoundaryWhenExecIsCalledThen48BitFlagIsSet) {
    MockExecObject execObject{};

    memset(&execObject, 0, sizeof(execObject));
    bo->setAddress(((uint64_t)1u << 32) - 0x1000u);
    bo->setSize(0x1000);
    bo->fillExecObject(execObject, osContext.get(), 0, 1);
    // base address + size > size of 32bit address space
    EXPECT_TRUE(execObject.has48BAddressSupportFlag());
}

TEST_F(DrmBufferObjectTest, givenAddressThatWhenSizeIsAddedWithin32BitBoundaryWhenExecIsCalledThen48BitFlagSet) {
    MockExecObject execObject{};

    memset(&execObject, 0, sizeof(execObject));
    bo->setAddress(((uint64_t)1u << 32) - 0x1000u);
    bo->setSize(0xFFF);
    bo->fillExecObject(execObject, osContext.get(), 0, 1);
    // base address + size < size of 32bit address space
    EXPECT_TRUE(execObject.has48BAddressSupportFlag());
}

TEST_F(DrmBufferObjectTest, whenExecFailsThenPinFails) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[1024]);

    mock->ioctlExpected.total = 3;
    mock->ioctlRes = -1;
    this->mock->errnoValue = EINVAL;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(rootDeviceIndex, this->mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->pin(boArray, 1, osContext.get(), 0, 1);
    EXPECT_EQ(EINVAL, ret);
}

TEST_F(DrmBufferObjectTest, givenDirectSubmissionLightWhenValidateHostptrThenStopDirectSubmission) {
    mock->ioctlExpected.total = -1;
    executionEnvironment.initializeMemoryManager();
    MockCommandStreamReceiver csr(executionEnvironment, 0, 0b1);
    OsContextLinux osContextLinux(*mock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    executionEnvironment.memoryManager->createAndRegisterOsContext(&csr, EngineDescriptorHelper::getDefaultDescriptor());
    auto &engine = executionEnvironment.memoryManager->getRegisteredEngines(0)[0];
    auto backupContext = engine.osContext;
    auto &mutableEngine = const_cast<EngineControl &>(engine);
    mutableEngine.osContext = &osContextLinux;
    executionEnvironment.memoryManager->getRegisteredEngines(0)[0].osContext->setDirectSubmissionActive();
    std::unique_ptr<uint32_t[]> buff(new uint32_t[1024]);

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(rootDeviceIndex, this->mock.get()));
    ASSERT_NE(nullptr, boToPin.get());
    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    BufferObject *boArray[1] = {boToPin.get()};
    EXPECT_EQ(csr.stopDirectSubmissionCalledTimes, 0u);

    bo->validateHostPtr(boArray, 1, osContext.get(), 0, 1);

    EXPECT_EQ(csr.stopDirectSubmissionCalledTimes, 1u);
    mutableEngine.osContext = backupContext;
}

TEST_F(DrmBufferObjectTest, whenExecFailsThenValidateHostPtrFails) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[1024]);

    mock->ioctlExpected.total = 3;
    mock->ioctlRes = -1;
    this->mock->errnoValue = EINVAL;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(rootDeviceIndex, this->mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->validateHostPtr(boArray, 1, osContext.get(), 0, 1);
    EXPECT_EQ(EINVAL, ret);
}

TEST_F(DrmBufferObjectTest, givenResidentBOWhenPrintExecutionBufferIsSetToTrueThenDebugInformationAboutBOIsPrinted) {
    mock->ioctlExpected.total = 1;
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExecutionBuffer.set(true);

    std::unique_ptr<uint32_t[]> buff(new uint32_t[1024]);
    std::unique_ptr<BufferObject> bo(new TestedBufferObject(rootDeviceIndex, this->mock.get()));
    ASSERT_NE(nullptr, bo.get());
    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    BufferObject *boArray[1] = {bo.get()};

    StreamCapture capture;
    capture.captureStdout();
    auto ret = bo->pin(boArray, 1, osContext.get(), 0, 1);
    EXPECT_EQ(0, ret);

    std::string output = capture.getCapturedStdout();
    auto idx = output.find("drm_i915_gem_execbuffer2 {");
    size_t expectedValue = 29;
    EXPECT_EQ(expectedValue, idx);

    idx = output.find("Buffer Object = { handle: BO-");
    EXPECT_NE(std::string::npos, idx);

    idx = output.find("Command Buffer Object = { handle: BO-");
    EXPECT_NE(std::string::npos, idx);
}

TEST_F(DrmBufferObjectTest, whenPrintBOCreateDestroyResultFlagIsSetAndCloseIsCalledOnBOThenDebugInfromationIsPrinted) {
    mock->ioctlExpected.total = 1;
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOCreateDestroyResult.set(true);

    StreamCapture capture;
    capture.captureStdout();
    bool result = bo->close();
    EXPECT_EQ(true, result);

    std::string output = capture.getCapturedStdout();
    size_t idx = output.find("Calling gem close on handle: BO-");
    size_t expectedValue = 0;
    EXPECT_EQ(expectedValue, idx);
}

TEST_F(DrmBufferObjectTest, whenPrintBOCreateDestroyResultFlagIsSetAndCloseIsCalledButHandleIsSharedThenDebugInfromationIsPrintedThatCloseIsSkipped) {
    mock->ioctlExpected.total = 1;
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOCreateDestroyResult.set(true);

    {
        MockBufferObjectHandleWrapper sharedBoHandleWrapper = bo->acquireSharedOwnershipOfBoHandle();
        EXPECT_TRUE(bo->isBoHandleShared());

        StreamCapture capture;
        capture.captureStdout();
        bool result = bo->close();
        EXPECT_EQ(true, result);

        std::string output = capture.getCapturedStdout();
        size_t idx = output.find("Skipped closing BO-");
        size_t expectedValue = 0u;
        EXPECT_EQ(expectedValue, idx);
    }

    StreamCapture capture;
    capture.captureStdout();
    bool result = bo->close();
    EXPECT_EQ(true, result);

    std::string output = capture.getCapturedStdout();
    size_t idx = output.find("Calling gem close on handle: BO-");
    size_t expectedValue = 0;
    EXPECT_EQ(expectedValue, idx);
}

TEST_F(DrmBufferObjectTest, whenPrintExecutionBufferIsSetToTrueThenMessageFoundInStdStream) {
    mock->ioctlExpected.total = 1;
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExecutionBuffer.set(true);
    ExecObject execObjectsStorage = {};

    StreamCapture capture;
    capture.captureStdout();
    auto ret = bo->exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0);
    EXPECT_EQ(0, ret);

    std::string output = capture.getCapturedStdout();

    auto idx = output.find("Exec called with drmVmId = " + std::to_string(mock->getVmIdForContext(*osContext.get(), 0)));
    uint32_t expectedValue = 0;
    EXPECT_EQ(expectedValue, idx);

    idx = output.find("drm_i915_gem_execbuffer2 {");
    expectedValue = 29;
    EXPECT_EQ(expectedValue, idx);
}

TEST(DrmBufferObjectSimpleTest, givenInvalidBoWhenValidateHostptrIsCalledThenErrorIsReturned) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    MockExecutionEnvironment executionEnvironment;
    auto mock = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock.get(), 0u, false);
    OsContextLinux osContext(*mock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    ASSERT_NE(nullptr, mock.get());
    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(0u, mock.get()));
    ASSERT_NE(nullptr, bo.get());

    // fail DRM_IOCTL_I915_GEM_EXECBUFFER2 in pin
    mock->ioctlRes = -1;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(0u, mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    mock->errnoValue = EFAULT;

    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->pin(boArray, 1, &osContext, 0, 1);
    EXPECT_EQ(EFAULT, ret);
    mock->ioctlRes = 0;
}

TEST(DrmBufferObjectSimpleTest, givenInvalidBoWhenPinIsCalledThenErrorIsReturned) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    MockExecutionEnvironment executionEnvironment;
    auto mock = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock.get(), 0u, false);
    OsContextLinux osContext(*mock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    ASSERT_NE(nullptr, mock.get());
    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(0u, mock.get()));
    ASSERT_NE(nullptr, bo.get());

    // fail DRM_IOCTL_I915_GEM_EXECBUFFER2 in pin
    mock->ioctlRes = -1;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(0u, mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    mock->errnoValue = EFAULT;

    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->validateHostPtr(boArray, 1, &osContext, 0, 1);
    EXPECT_EQ(EFAULT, ret);
    mock->ioctlRes = 0;
}

TEST(DrmBufferObjectSimpleTest, givenBufferObjectWhenConstructedWithASizeThenTheSizeIsInitialized) {
    MockExecutionEnvironment executionEnvironment;
    auto drmMock = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    std::unique_ptr<BufferObject> bo(new BufferObject(0u, drmMock.get(), 3, 1, 0x1000, 1));

    EXPECT_EQ(0x1000u, bo->peekSize());
}

TEST(DrmBufferObjectSimpleTest, givenArrayOfBosWhenPinnedThenAllBosArePinned) {
    const auto rootDeviceIndex = 0u;
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    MockExecutionEnvironment executionEnvironment;
    auto mock = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, mock.get());
    OsContextLinux osContext(*mock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());

    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(rootDeviceIndex, mock.get()));
    ASSERT_NE(nullptr, bo.get());
    mock->ioctlRes = 0;

    std::unique_ptr<TestedBufferObject> boToPin(new TestedBufferObject(rootDeviceIndex, mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin2(new TestedBufferObject(rootDeviceIndex, mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin3(new TestedBufferObject(rootDeviceIndex, mock.get()));

    ASSERT_NE(nullptr, boToPin.get());
    ASSERT_NE(nullptr, boToPin2.get());
    ASSERT_NE(nullptr, boToPin3.get());

    BufferObject *array[3] = {boToPin.get(), boToPin2.get(), boToPin3.get()};

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    auto ret = bo->pin(array, 3, &osContext, 0, 1);
    EXPECT_EQ(mock->ioctlRes, ret);

    EXPECT_LT(0u, mock->execBuffer.getBatchLen());
    EXPECT_EQ(4u, mock->execBuffer.getBufferCount()); // 3 bos to pin plus 1 exec bo
    EXPECT_EQ(reinterpret_cast<uintptr_t>(boToPin->execObjectPointerFilled), mock->execBuffer.getBuffersPtr());
    EXPECT_NE(nullptr, boToPin2->execObjectPointerFilled);
    EXPECT_NE(nullptr, boToPin3->execObjectPointerFilled);

    bo->setAddress(0llu);
}

TEST(DrmBufferObjectSimpleTest, givenArrayOfBosWhenValidatedThenAllBosArePinned) {
    const auto rootDeviceIndex = 0u;
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    MockExecutionEnvironment executionEnvironment;
    auto mock = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, mock.get());
    OsContextLinux osContext(*mock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());

    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(rootDeviceIndex, mock.get()));
    ASSERT_NE(nullptr, bo.get());
    mock->ioctlRes = 0;

    std::unique_ptr<TestedBufferObject> boToPin(new TestedBufferObject(rootDeviceIndex, mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin2(new TestedBufferObject(rootDeviceIndex, mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin3(new TestedBufferObject(rootDeviceIndex, mock.get()));

    ASSERT_NE(nullptr, boToPin.get());
    ASSERT_NE(nullptr, boToPin2.get());
    ASSERT_NE(nullptr, boToPin3.get());

    BufferObject *array[3] = {boToPin.get(), boToPin2.get(), boToPin3.get()};

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    auto ret = bo->validateHostPtr(array, 3, &osContext, 0, 1);
    EXPECT_EQ(mock->ioctlRes, ret);

    EXPECT_LT(0u, mock->execBuffer.getBatchLen());
    EXPECT_EQ(4u, mock->execBuffer.getBufferCount()); // 3 bos to pin plus 1 exec bo
    EXPECT_EQ(reinterpret_cast<uintptr_t>(boToPin->execObjectPointerFilled), mock->execBuffer.getBuffersPtr());
    EXPECT_NE(nullptr, boToPin2->execObjectPointerFilled);
    EXPECT_NE(nullptr, boToPin3->execObjectPointerFilled);

    bo->setAddress(0llu);
}

TEST_F(DrmBufferObjectTest, givenDeleterWhenBufferObjectIsCreatedAndDeletedThenCloseIsCalled) {
    mock->ioctlCnt.reset();
    mock->ioctlExpected.reset();

    {
        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new BufferObject(0u, mock.get(), 3, 1, 0x1000, 1));
    }

    EXPECT_EQ(1, mock->ioctlCnt.gemClose);
    mock->ioctlCnt.reset();
    mock->ioctlExpected.reset();
}

TEST(DrmBufferObject, givenOfflineDebuggingModeWhenQueryingIsPerContextVMRequiredThenPerContextVMIsDisabled) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->getRootDeviceEnvironment().executionEnvironment.setDebuggingMode(NEO::DebuggingMode::offline);
    DrmMock drm(*(device->getExecutionEnvironment()->rootDeviceEnvironments[0].get()));
    EXPECT_FALSE(drm.isPerContextVMRequired());
}

TEST(DrmBufferObject, givenPerContextVmRequiredWhenBoCreatedThenBindInfoIsInitializedToOsContextCount) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->getRootDeviceEnvironment().executionEnvironment.setDebuggingMode(NEO::DebuggingMode::online);
    device->getExecutionEnvironment()->calculateMaxOsContextCount();
    DrmMock drm(*(device->getExecutionEnvironment()->rootDeviceEnvironments[0].get()));
    EXPECT_TRUE(drm.isPerContextVMRequired());
    auto osContextCount = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex()).size();
    MockBufferObject bo(device->getRootDeviceIndex(), &drm, 3, 0, 0, osContextCount);

    EXPECT_EQ(osContextCount, bo.bindInfo.size());

    for (auto &iter : bo.bindInfo) {
        for (uint32_t i = 0; i < EngineLimits::maxHandleCount; i++) {
            EXPECT_FALSE(iter[i]);
        }
    }
}

TEST(DrmBufferObject, givenDrmIoctlReturnsErrorNotSupportedThenBufferObjectReturnsError) {
    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMock *drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->execBufferResult = -1;
    drm->errnoRetVal = EOPNOTSUPP;
    drm->baseErrno = false;
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);

    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);

    std::unique_ptr<OsContextLinux> osContext;
    osContext.reset(new OsContextLinux(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor()));

    ExecObject execObjectsStorage = {};
    auto ret = bo.exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0);
    EXPECT_NE(0, ret);
}

TEST(DrmBufferObject, givenPerContextVmRequiredWhenBoBoundAndUnboundThenCorrectBindInfoIsUpdated) {
    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing *drm = new DrmMockNonFailing(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_TRUE(drm->isPerContextVMRequired());

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);

    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);

    EXPECT_EQ(osContextCount, bo.bindInfo.size());

    auto contextId = osContextCount / 2;
    auto osContext = engines[contextId].osContext;
    osContext->ensureContextInitialized(false);

    bo.bind(osContext, 0, false);
    EXPECT_TRUE(bo.bindInfo[contextId][0]);

    bo.unbind(osContext, 0);
    EXPECT_FALSE(bo.bindInfo[contextId][0]);
}

TEST(DrmBufferObject, givenPrintBOBindingResultWhenBOBindAndUnbindSucceedsThenPrintDebugInformationAboutBOBindingResult) {
    struct DrmMockToSucceedBindBufferObject : public DrmMock {
        DrmMockToSucceedBindBufferObject(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}
        int bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo, const bool forcePagingFence) override { return 0; }
        int unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) override { return 0; }
    };

    DebugManagerStateRestore restore;
    debugManager.flags.PrintBOBindingResult.set(true);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto drm = new DrmMockToSucceedBindBufferObject(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->latestCreatedVmId = 1;

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);

    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);

    EXPECT_EQ(osContextCount, bo.bindInfo.size());

    auto contextId = osContextCount / 2;
    auto osContext = engines[contextId].osContext;
    osContext->ensureContextInitialized(false);

    StreamCapture capture;
    capture.captureStdout();

    bo.bind(osContext, 0, false);
    EXPECT_TRUE(bo.bindInfo[contextId][0]);

    std::string bindOutput = capture.getCapturedStdout();
    std::stringstream expected;
    expected << "bind BO-0 to VM " << drm->latestCreatedVmId << ", vmHandleId = 0"
             << ", range: 0 - 0, size: 0, result: 0\n";
    EXPECT_STREQ(bindOutput.c_str(), expected.str().c_str()) << bindOutput;
    expected.str("");

    capture.captureStdout();

    bo.unbind(osContext, 0);
    EXPECT_FALSE(bo.bindInfo[contextId][0]);

    std::string unbindOutput = capture.getCapturedStdout();
    expected << "unbind BO-0 from VM " << drm->latestCreatedVmId << ", vmHandleId = 0"
             << ", range: 0 - 0, size: 0, result: 0\n";
    EXPECT_STREQ(unbindOutput.c_str(), expected.str().c_str()) << unbindOutput;
}

TEST(DrmBufferObject, givenPrintBOBindingResultWhenBOBindAndUnbindFailsThenPrintDebugInformationAboutBOBindingResultWithErrno) {
    struct DrmMockToFailBindBufferObject : public DrmMock {
        DrmMockToFailBindBufferObject(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}
        int bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo, const bool forcePagingFence) override { return -1; }
        int unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) override { return -1; }
        int getErrno() override { return EINVAL; }
    };

    DebugManagerStateRestore restore;
    debugManager.flags.PrintBOBindingResult.set(true);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto drm = new DrmMockToFailBindBufferObject(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->latestCreatedVmId = 1;

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);

    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);

    EXPECT_EQ(osContextCount, bo.bindInfo.size());

    auto contextId = osContextCount / 2;
    auto osContext = engines[contextId].osContext;
    osContext->ensureContextInitialized(false);

    testing::internal::CaptureStderr();

    bo.bind(osContext, 0, false);
    EXPECT_FALSE(bo.bindInfo[contextId][0]);

    std::string bindOutput = testing::internal::GetCapturedStderr();
    std::stringstream expected;
    expected << "bind BO-0 to VM " << drm->latestCreatedVmId << ", vmHandleId = 0"
             << ", range: 0 - 0, size: 0, result: -1, errno: 22\n";
    EXPECT_TRUE(hasSubstr(expected.str(), expected.str())) << bindOutput;
    expected.str("");
    testing::internal::CaptureStderr();
    bo.bindInfo[contextId][0] = true;

    bo.unbind(osContext, 0);
    EXPECT_TRUE(bo.bindInfo[contextId][0]);

    std::string unbindOutput = testing::internal::GetCapturedStderr();
    expected << "unbind BO-0 from VM " << drm->latestCreatedVmId << ", vmHandleId = 0"
             << ", range: 0 - 0, size: 0, result: -1, errno: 22";
    EXPECT_TRUE(hasSubstr(unbindOutput, expected.str())) << unbindOutput;
}

TEST(DrmBufferObject, givenDrmWhenBindOperationFailsThenFenceValueNotGrow) {
    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = true;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->vmBindResult = -1;
    ioctlHelper->isWaitBeforeBindRequiredResult = true;
    drm->ioctlHelper.reset(ioctlHelper.release());

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();
    auto contextId = osContextCount / 2;
    auto osContext = engines[contextId].osContext;
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->bindBufferObject(osContext, 0, &bo, false);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue);
}

TEST(DrmBufferObject, givenDrmWhenBindOperationSucceedsThenFenceValueGrow) {

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.set(1);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = true;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isWaitBeforeBindRequiredResult = true;
    drm->ioctlHelper.reset(ioctlHelper.release());

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();
    auto contextId = osContextCount / 2;
    auto osContext = engines[contextId].osContext;
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->bindBufferObject(osContext, 0, &bo, false);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue + 1);
}

using ::testing::Combine;
using ::testing::Values;
using ::testing::WithParamInterface;

class DrmBufferObjectBindTestWithForcePagingFenceSucceeds : public ::testing::TestWithParam<std::tuple<int32_t, bool, bool>> {};

TEST_P(DrmBufferObjectBindTestWithForcePagingFenceSucceeds, givenDrmWhenBindOperationSucceedsWithForcePagingFenceThenFenceValueGrow) {
    int32_t waitOnUserFenceAfterBindAndUnbindVal = std::get<0>(GetParam());
    bool isVMBindImmediateSupportedVal = std::get<1>(GetParam());
    bool isWaitBeforeBindRequiredResultVal = std::get<2>(GetParam());

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.set(waitOnUserFenceAfterBindAndUnbindVal);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    class MockDrmWithWaitUserFence : public DrmMock {
      public:
        MockDrmWithWaitUserFence(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}

        int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt,
                          uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
            waitUserFenceCalled = true;
            return 0;
        }

        bool waitUserFenceCalled = false;
    };

    auto drm = new MockDrmWithWaitUserFence(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = isVMBindImmediateSupportedVal;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isWaitBeforeBindRequiredResult = isWaitBeforeBindRequiredResultVal;
    drm->ioctlHelper.reset(ioctlHelper.release());

    auto osContext = new OsContextLinux(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext->ensureContextInitialized(false);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    // Make sure to disable the debugging mode
    executionEnvironment->setDebuggingMode(DebuggingMode::disabled);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();

    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->bindBufferObject(osContext, 0, &bo, true);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue + 1);
    EXPECT_TRUE(drm->waitUserFenceCalled);
    delete osContext;
}

INSTANTIATE_TEST_SUITE_P(
    DrmBufferObjectBindTestsWithForcePagingFenceSucceeds,
    DrmBufferObjectBindTestWithForcePagingFenceSucceeds,
    Combine(
        Values(0, 1),
        Values(true, false),
        Values(true, false)));

class DrmBufferObjectBindTestWithForcePagingFenceFalseWaitUserFenceNotCalled : public ::testing::TestWithParam<std::tuple<int32_t, bool, bool>> {};

TEST_P(DrmBufferObjectBindTestWithForcePagingFenceFalseWaitUserFenceNotCalled, givenDrmWhenBindOperationSucceedsWithForcePagingFenceFalseThenFenceValueDoesNotGrow) {
    int32_t waitOnUserFenceAfterBindAndUnbindVal = std::get<0>(GetParam());
    bool isVMBindImmediateSupportedVal = std::get<1>(GetParam());
    bool isWaitBeforeBindRequiredResultVal = std::get<2>(GetParam());

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.set(waitOnUserFenceAfterBindAndUnbindVal);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    class MockDrmWithWaitUserFence : public DrmMock {
      public:
        MockDrmWithWaitUserFence(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}

        int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt,
                          uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
            waitUserFenceCalled = true;
            return 0;
        }

        bool waitUserFenceCalled = false;
    };

    auto drm = new MockDrmWithWaitUserFence(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = isVMBindImmediateSupportedVal;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isWaitBeforeBindRequiredResult = isWaitBeforeBindRequiredResultVal;
    drm->ioctlHelper.reset(ioctlHelper.release());

    auto osContext = new OsContextLinux(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext->ensureContextInitialized(false);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    // Make sure to disable the debugging mode
    executionEnvironment->setDebuggingMode(DebuggingMode::disabled);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();

    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->bindBufferObject(osContext, 0, &bo, false);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue);
    EXPECT_FALSE(drm->waitUserFenceCalled);
    delete osContext;
}

INSTANTIATE_TEST_SUITE_P(
    DrmBufferObjectBindTestsWithForcePagingFenceFalseWaitUserFenceNotCalled,
    DrmBufferObjectBindTestWithForcePagingFenceFalseWaitUserFenceNotCalled,
    Values(
        std::make_tuple(0, true, false),
        std::make_tuple(0, false, false),
        std::make_tuple(0, false, true),
        std::make_tuple(1, true, false),
        std::make_tuple(1, false, false),
        std::make_tuple(1, false, true)));

TEST(DrmBufferObject, givenDrmWhenBindOperationSucceedsWithForcePagingFenceWithDebuggingEnabledWithWaitOnUserFenceAfterBindAndUnbindAndNotUseVMBindImmediateThenFenceValueDoesNotGrow) {

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.set(1);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    class MockDrmWithWaitUserFence : public DrmMock {
      public:
        MockDrmWithWaitUserFence(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}

        int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt,
                          uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
            waitUserFenceCalled = true;
            return 0;
        }

        bool waitUserFenceCalled = false;
    };

    auto drm = new MockDrmWithWaitUserFence(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    // Making the useVMBindImmediate() false
    drm->isVMBindImmediateSupported = false;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isWaitBeforeBindRequiredResult = true;
    drm->ioctlHelper.reset(ioctlHelper.release());

    auto osContext = new OsContextLinux(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext->ensureContextInitialized(false);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    // Make sure to enable the debugging mode
    executionEnvironment->setDebuggingMode(DebuggingMode::online);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();

    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->bindBufferObject(osContext, 0, &bo, true);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue);
    EXPECT_FALSE(drm->waitUserFenceCalled);
    delete osContext;
}

TEST(DrmBufferObject, givenDrmWhenUnBindOperationFailsThenFenceValueNotGrow) {
    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = true;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->vmUnbindResult = -1;
    ioctlHelper->isWaitBeforeBindRequiredResult = true;
    drm->ioctlHelper.reset(ioctlHelper.release());

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();
    auto contextId = osContextCount / 2;
    auto osContext = engines[contextId].osContext;
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->unbindBufferObject(osContext, 0, &bo);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue);
}

TEST(DrmBufferObject, givenDrmWhenUnBindOperationSucceedsThenFenceValueGrow) {
    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = true;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isWaitBeforeBindRequiredResult = true;
    drm->ioctlHelper.reset(ioctlHelper.release());

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();
    auto contextId = osContextCount / 2;
    auto osContext = engines[contextId].osContext;
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->unbindBufferObject(osContext, 0, &bo);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue + 1);
}

TEST(DrmBufferObject, givenDrmWhenUnBindOperationSucceedsAndForceUserFenceUponUnbindAndDisableFenceWaitThenFenceValueGrow) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceUponUnbind.set(1);
    debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.set(0);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = true;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isWaitBeforeBindRequiredResult = true;
    drm->ioctlHelper.reset(ioctlHelper.release());

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();
    auto contextId = osContextCount / 2;
    auto osContext = engines[contextId].osContext;
    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->unbindBufferObject(osContext, 0, &bo);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue + 1);
}

TEST(DrmBufferObject, givenDrmWhenUnBindOperationSucceedsAndForceFenceWaitThenFenceValueGrow) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceUponUnbind.set(1);
    debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.set(1);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    class MockDrmWithWaitUserFence : public DrmMock {
      public:
        MockDrmWithWaitUserFence(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}

        int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt,
                          uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
            waitUserFenceCalled = true;
            return 0;
        }

        bool waitUserFenceCalled = false;
    };

    auto drm = new MockDrmWithWaitUserFence(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = true;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isWaitBeforeBindRequiredResult = true;
    drm->ioctlHelper.reset(ioctlHelper.release());

    auto osContext = new OsContextLinux(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext->ensureContextInitialized(false);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();

    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->unbindBufferObject(static_cast<OsContext *>(osContext), 0, &bo);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue + 1);
    EXPECT_TRUE(drm->waitUserFenceCalled);
    delete osContext;
}

TEST(DrmBufferObject, givenDrmWhenUnBindOperationSucceedsWaitBeforeBindFalseAndForceFenceWaitButWaitNotTrueThenFenceDoesNotGrow) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceUponUnbind.set(0);
    debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.set(1);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    class MockDrmWithWaitUserFence : public DrmMock {
      public:
        MockDrmWithWaitUserFence(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}

        int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt,
                          uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
            waitUserFenceCalled = true;
            return 0;
        }

        bool waitUserFenceCalled = false;
    };

    auto drm = new MockDrmWithWaitUserFence(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = true;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isWaitBeforeBindRequiredResult = false;
    drm->ioctlHelper.reset(ioctlHelper.release());

    auto osContext = new OsContextLinux(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext->ensureContextInitialized(false);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();

    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->unbindBufferObject(static_cast<OsContext *>(osContext), 0, &bo);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue);
    EXPECT_FALSE(drm->waitUserFenceCalled);
    delete osContext;
}

TEST(DrmBufferObject, givenDrmWhenUnBindOperationSucceedsWaitBeforeBindTrueAndForceFenceWaitButNotVmBindImmediateThenWaitUserFenceNotCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceUponUnbind.set(1);
    debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.set(1);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    class MockDrmWithWaitUserFence : public DrmMock {
      public:
        MockDrmWithWaitUserFence(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}

        int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt,
                          uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
            waitUserFenceCalled = true;
            return 0;
        }

        bool waitUserFenceCalled = false;
    };

    auto drm = new MockDrmWithWaitUserFence(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->requirePerContextVM = false;
    drm->isVMBindImmediateSupported = false;
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isWaitBeforeBindRequiredResult = true;
    drm->ioctlHelper.reset(ioctlHelper.release());

    auto osContext = new OsContextLinux(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext->ensureContextInitialized(false);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    uint64_t initFenceValue = 10u;
    drm->fenceVal[0] = initFenceValue;
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto &engines = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
    auto osContextCount = engines.size();

    MockBufferObject bo(device->getRootDeviceIndex(), drm, 3, 0, 0, osContextCount);
    drm->unbindBufferObject(static_cast<OsContext *>(osContext), 0, &bo);

    EXPECT_EQ(drm->fenceVal[0], initFenceValue);
    EXPECT_FALSE(drm->waitUserFenceCalled);
    delete osContext;
}

TEST(DrmBufferObject, whenBindExtHandleAddedThenItIsStored) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.addBindExtHandle(4);

    EXPECT_EQ(1u, bo.bindExtHandles.size());
    EXPECT_EQ(4u, bo.bindExtHandles[0]);

    EXPECT_EQ(1u, bo.getBindExtHandles().size());
    EXPECT_EQ(4u, bo.getBindExtHandles()[0]);
}

TEST(DrmBufferObject, whenMarkForCapturedCalledThenIsMarkedForCaptureReturnsTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    EXPECT_FALSE(bo.isMarkedForCapture());

    bo.markForCapture();
    EXPECT_TRUE(bo.isMarkedForCapture());
}

TEST_F(DrmBufferObjectTest, givenBoMarkedForCaptureWhenFillingExecObjectThenCaptureFlagIsSet) {
    MockExecObject execObject{};

    memset(&execObject, 0, sizeof(execObject));
    bo->markForCapture();
    bo->setAddress(0x45000);
    bo->setSize(0x1000);
    bo->fillExecObject(execObject, osContext.get(), 0, 1);

    EXPECT_TRUE(execObject.hasCaptureFlag());
}

TEST_F(DrmBufferObjectTest, givenAsyncDebugFlagWhenFillingExecObjectThenFlagIsSet) {
    MockExecObject execObject{};
    DebugManagerStateRestore restorer;
    debugManager.flags.UseAsyncDrmExec.set(1);

    memset(&execObject, 0, sizeof(execObject));
    bo->setAddress(0x45000);
    bo->setSize(0x1000);
    bo->fillExecObject(execObject, osContext.get(), 0, 1);

    EXPECT_TRUE(execObject.hasAsyncFlag());
}

TEST_F(DrmBufferObjectTest, given47bitAddressWhenSetThenIsAddressNotCanonized) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper()->setAddressWidth(48);

    DrmMock drm(*(executionEnvironment.rootDeviceEnvironments[0].get()));

    uint64_t address = maxNBitValue(47) - maxNBitValue(5);

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.setAddress(address);
    auto boAddress = bo.peekAddress();
    EXPECT_EQ(boAddress, address);
}

TEST_F(DrmBufferObjectTest, given48bitAddressWhenSetThenAddressIsCanonized) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper()->setAddressWidth(48);

    DrmMock drm(*(executionEnvironment.rootDeviceEnvironments[0].get()));

    uint64_t address = maxNBitValue(48) - maxNBitValue(5);
    uint64_t expectedAddress = std::numeric_limits<uint64_t>::max() - maxNBitValue(5);

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.setAddress(address);
    auto boAddress = bo.peekAddress();
    EXPECT_EQ(boAddress, expectedAddress);
}

TEST_F(DrmBufferObjectTest, givenBoIsCreatedWhenPageFaultIsSupportedThenExplicitResidencyIsRequiredByDefault) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    DrmMock drm(*(executionEnvironment.rootDeviceEnvironments[0].get()));

    for (auto isPageFaultSupported : {false, true}) {
        drm.pageFaultSupported = isPageFaultSupported;
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        EXPECT_EQ(isPageFaultSupported, bo.isExplicitResidencyRequired());
    }
}

TEST_F(DrmBufferObjectTest, whenBoRequiresExplicitResidencyThenTheCorrespondingQueryReturnsCorrectValue) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    DrmMock drm(*(executionEnvironment.rootDeviceEnvironments[0].get()));
    MockBufferObject bo(0, &drm, 3, 0, 0, 1);

    for (auto required : {false, true}) {
        bo.requireExplicitResidency(required);
        EXPECT_EQ(required, bo.isExplicitResidencyRequired());
    }
}

TEST(DrmBufferObjectHandleWrapperTest, GivenWrapperConstructedFromNonSharedHandleThenControlBlockIsNotCreatedAndInternalHandleIsStored) {
    constexpr int boHandle{5};
    MockBufferObjectHandleWrapper boHandleWrapper{boHandle, 1u};

    EXPECT_EQ(nullptr, boHandleWrapper.controlBlock);
    EXPECT_EQ(boHandle, boHandleWrapper.getBoHandle());
    EXPECT_EQ(1u, boHandleWrapper.getRootDeviceIndex());
}

TEST(DrmBufferObjectHandleWrapperTest, GivenWrapperConstructedFromNonSharedHandleWhenAskingIfCanBeClosedThenReturnTrue) {
    constexpr int boHandle{21};
    MockBufferObjectHandleWrapper boHandleWrapper{boHandle, 1u};

    EXPECT_TRUE(boHandleWrapper.canCloseBoHandle());
}

TEST(DrmBufferObjectHandleWrapperTest, GivenWrapperWhenSettingNewValueThenStoreIt) {
    constexpr int boHandle{13};
    MockBufferObjectHandleWrapper boHandleWrapper{boHandle, 1u};

    boHandleWrapper.setBoHandle(-1);
    boHandleWrapper.setRootDeviceIndex(4u);
    EXPECT_EQ(-1, boHandleWrapper.getBoHandle());
    EXPECT_EQ(4u, boHandleWrapper.getRootDeviceIndex());
}

TEST(DrmBufferObjectHandleWrapperTest, GivenWrapperConstructedFromNonSharedHandleWhenMakingItSharedThenControlBlockIsCreatedAndReferenceCounterIsValid) {
    constexpr int boHandle{85};
    MockBufferObjectHandleWrapper firstBoHandleWrapper{boHandle, 1u};
    MockBufferObjectHandleWrapper secondBoHandleWrapper = firstBoHandleWrapper.acquireSharedOwnership();

    ASSERT_NE(nullptr, firstBoHandleWrapper.controlBlock);
    ASSERT_NE(nullptr, secondBoHandleWrapper.controlBlock);

    EXPECT_EQ(firstBoHandleWrapper.controlBlock, secondBoHandleWrapper.controlBlock);
    EXPECT_EQ(firstBoHandleWrapper.boHandle, secondBoHandleWrapper.boHandle);

    EXPECT_EQ(2, firstBoHandleWrapper.controlBlock->refCount);
}

TEST(DrmBufferObjectHandleWrapperTest, GivenMoreThanOneSharedHandleWrapperWhenAskingIfHandleCanBeClosedThenReturnFalse) {
    constexpr int boHandle{121};
    MockBufferObjectHandleWrapper firstBoHandleWrapper{boHandle, 1u};
    MockBufferObjectHandleWrapper secondBoHandleWrapper = firstBoHandleWrapper.acquireSharedOwnership();

    EXPECT_FALSE(firstBoHandleWrapper.canCloseBoHandle());
    EXPECT_FALSE(secondBoHandleWrapper.canCloseBoHandle());
}

TEST(DrmBufferObjectHandleWrapperTest, GivenControlBlockCreatedWhenOnlyOneReferenceLeftThenHandleCanBeClosed) {
    constexpr int boHandle{121};
    MockBufferObjectHandleWrapper firstBoHandleWrapper{boHandle, 1u};

    {
        MockBufferObjectHandleWrapper secondBoHandleWrapper = firstBoHandleWrapper.acquireSharedOwnership();

        ASSERT_NE(nullptr, firstBoHandleWrapper.controlBlock);
        ASSERT_NE(nullptr, secondBoHandleWrapper.controlBlock);

        EXPECT_EQ(firstBoHandleWrapper.controlBlock, secondBoHandleWrapper.controlBlock);
        EXPECT_EQ(2, firstBoHandleWrapper.controlBlock->refCount);
    }

    EXPECT_TRUE(firstBoHandleWrapper.canCloseBoHandle());
}

TEST(DrmBufferObjectHandleWrapperTest, GivenControlBlockCreatedWhenOnlyWeakReferencesLeftThenItIsNotDestroyed) {
    constexpr int boHandle{777};
    auto firstBoHandleWrapper = std::make_unique<MockBufferObjectHandleWrapper>(boHandle, 1u);
    MockBufferObjectHandleWrapper weakHandleWrapper = firstBoHandleWrapper->acquireWeakOwnership();

    ASSERT_NE(nullptr, firstBoHandleWrapper->controlBlock);
    ASSERT_NE(nullptr, weakHandleWrapper.controlBlock);
    EXPECT_EQ(firstBoHandleWrapper->controlBlock, weakHandleWrapper.controlBlock);

    firstBoHandleWrapper.reset();

    EXPECT_EQ(0, weakHandleWrapper.controlBlock->refCount);
    EXPECT_EQ(1, weakHandleWrapper.controlBlock->weakRefCount);
}

TEST(DrmBufferObjectHandleWrapperTest, GivenControlBlockCreatedWhenWeakReferencesLeftAndOnlyOneStrongReferenceLeftThenHandleCanBeClosed) {
    constexpr int boHandle{353};
    MockBufferObjectHandleWrapper firstBoHandleWrapper{boHandle, 1u};
    MockBufferObjectHandleWrapper firstWeakHandleWrapper = firstBoHandleWrapper.acquireWeakOwnership();
    MockBufferObjectHandleWrapper secondWeakHandleWrapper = firstBoHandleWrapper.acquireWeakOwnership();

    ASSERT_NE(nullptr, firstBoHandleWrapper.controlBlock);
    ASSERT_NE(nullptr, firstWeakHandleWrapper.controlBlock);
    ASSERT_NE(nullptr, secondWeakHandleWrapper.controlBlock);

    EXPECT_EQ(firstBoHandleWrapper.controlBlock, firstWeakHandleWrapper.controlBlock);
    EXPECT_EQ(firstBoHandleWrapper.controlBlock, secondWeakHandleWrapper.controlBlock);

    EXPECT_EQ(1, firstBoHandleWrapper.controlBlock->refCount);
    EXPECT_EQ(2, firstBoHandleWrapper.controlBlock->weakRefCount);

    EXPECT_TRUE(firstBoHandleWrapper.canCloseBoHandle());
}

TEST(DrmBufferObjectHandleWrapperTest, GivenWrapperWhenConstructingMoreThanTwoSharedResourcesControlBlockRemainsTheSameAndReferenceCounterIsUpdatedOnCreationAndDestruction) {
    constexpr int boHandle{85};
    MockBufferObjectHandleWrapper firstBoHandleWrapper{boHandle, 1u};
    MockBufferObjectHandleWrapper secondBoHandleWrapper = firstBoHandleWrapper.acquireSharedOwnership();

    ASSERT_EQ(firstBoHandleWrapper.controlBlock, secondBoHandleWrapper.controlBlock);

    auto controlBlock = firstBoHandleWrapper.controlBlock;
    ASSERT_NE(nullptr, controlBlock);
    EXPECT_EQ(2, controlBlock->refCount);

    {
        MockBufferObjectHandleWrapper thirdBoHandleWrapper = firstBoHandleWrapper.acquireSharedOwnership();
        EXPECT_EQ(firstBoHandleWrapper.boHandle, thirdBoHandleWrapper.boHandle);

        ASSERT_EQ(controlBlock, thirdBoHandleWrapper.controlBlock);
        EXPECT_EQ(3, controlBlock->refCount);
    }

    EXPECT_EQ(2, controlBlock->refCount);
}

TEST(DrmBufferObjectHandleWrapperTest, GivenWrapperWhenMoveConstructingAnotherObjectThenInternalDataIsCleared) {
    constexpr int boHandle{27};
    MockBufferObjectHandleWrapper firstBoHandleWrapper{boHandle, 1u};
    MockBufferObjectHandleWrapper secondBoHandleWrapper = firstBoHandleWrapper.acquireSharedOwnership();

    auto oldControlBlock = firstBoHandleWrapper.controlBlock;
    auto oldBoHandle = firstBoHandleWrapper.boHandle;

    MockBufferObjectHandleWrapper anotherWrapper{std::move(firstBoHandleWrapper)};
    EXPECT_EQ(oldControlBlock, anotherWrapper.controlBlock);
    EXPECT_EQ(oldBoHandle, anotherWrapper.boHandle);

    EXPECT_EQ(nullptr, firstBoHandleWrapper.controlBlock);
    EXPECT_EQ(-1, firstBoHandleWrapper.boHandle);

    EXPECT_EQ(2, secondBoHandleWrapper.controlBlock->refCount);
}
