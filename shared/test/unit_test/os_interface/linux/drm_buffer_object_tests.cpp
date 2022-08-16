/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/os_interface/linux/drm_buffer_object_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using DrmMockBufferObjectFixture = DrmBufferObjectFixture<DrmMockCustom>;
using DrmBufferObjectTest = Test<DrmMockBufferObjectFixture>;

TEST_F(DrmBufferObjectTest, WhenCallingExecThenReturnIsCorrect) {
    mock->ioctl_expected.total = 1;
    mock->ioctl_res = 0;

    ExecObject execObjectsStorage = {};
    auto ret = bo->exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0);
    EXPECT_EQ(mock->ioctl_res, ret);
    EXPECT_EQ(0u, mock->execBuffer.getFlags());
}

TEST_F(DrmBufferObjectTest, GivenInvalidParamsWhenCallingExecThenEfaultIsReturned) {
    mock->ioctl_expected.total = 3;
    mock->ioctl_res = -1;
    mock->errnoValue = EFAULT;
    ExecObject execObjectsStorage = {};
    EXPECT_EQ(EFAULT, bo->exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0));
}

TEST_F(DrmBufferObjectTest, GivenDetectedGpuHangDuringEvictUnusedAllocationsWhenCallingExecGpuHangErrorCodeIsRetrurned) {
    mock->ioctl_expected.total = 2;
    mock->ioctl_res = -1;
    mock->errnoValue = EFAULT;

    bo->callBaseEvictUnusedAllocations = false;

    ExecObject execObjectsStorage = {};
    const auto result = bo->exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0);

    EXPECT_EQ(BufferObject::gpuHangDetected, result);
}

TEST_F(DrmBufferObjectTest, WhenSettingTilingThenCallSucceeds) {
    mock->ioctl_expected.total = 1; //set_tiling
    auto tilingY = mock->getIoctlHelper()->getDrmParamValue(DrmParam::TilingY);
    auto ret = bo->setTiling(tilingY, 0);
    EXPECT_TRUE(ret);
}

TEST_F(DrmBufferObjectTest, WhenSettingSameTilingThenCallSucceeds) {
    mock->ioctl_expected.total = 0; //set_tiling
    auto tilingY = mock->getIoctlHelper()->getDrmParamValue(DrmParam::TilingY);
    bo->tilingMode = tilingY;
    auto ret = bo->setTiling(tilingY, 0);
    EXPECT_TRUE(ret);
}

TEST_F(DrmBufferObjectTest, GivenInvalidTilingWhenSettingTilingThenCallFails) {
    mock->ioctl_expected.total = 1; //set_tiling
    auto tilingY = mock->getIoctlHelper()->getDrmParamValue(DrmParam::TilingY);
    mock->ioctl_res = -1;
    auto ret = bo->setTiling(tilingY, 0);
    EXPECT_FALSE(ret);
}

TEST_F(DrmBufferObjectTest, givenBindAvailableWhenCallWaitThenNoIoctlIsCalled) {
    mock->bindAvailable = true;
    mock->ioctl_expected.total = 0;
    auto ret = bo->wait(-1);
    EXPECT_FALSE(ret);
}

TEST_F(DrmBufferObjectTest, givenAddressThatWhenSizeIsAddedCrosses32BitBoundaryWhenExecIsCalledThen48BitFlagIsSet) {
    MockExecObject execObject{};

    memset(&execObject, 0, sizeof(execObject));
    bo->setAddress(((uint64_t)1u << 32) - 0x1000u);
    bo->setSize(0x1000);
    bo->fillExecObject(execObject, osContext.get(), 0, 1);
    //base address + size > size of 32bit address space
    EXPECT_TRUE(execObject.has48BAddressSupportFlag());
}

TEST_F(DrmBufferObjectTest, givenAddressThatWhenSizeIsAddedWithin32BitBoundaryWhenExecIsCalledThen48BitFlagSet) {
    MockExecObject execObject{};

    memset(&execObject, 0, sizeof(execObject));
    bo->setAddress(((uint64_t)1u << 32) - 0x1000u);
    bo->setSize(0xFFF);
    bo->fillExecObject(execObject, osContext.get(), 0, 1);
    //base address + size < size of 32bit address space
    EXPECT_TRUE(execObject.has48BAddressSupportFlag());
}

TEST_F(DrmBufferObjectTest, whenExecFailsThenPinFails) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[1024]);

    mock->ioctl_expected.total = 3;
    mock->ioctl_res = -1;
    this->mock->errnoValue = EINVAL;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(this->mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->pin(boArray, 1, osContext.get(), 0, 1);
    EXPECT_EQ(EINVAL, ret);
}

TEST_F(DrmBufferObjectTest, whenExecFailsThenValidateHostPtrFails) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[1024]);

    mock->ioctl_expected.total = 3;
    mock->ioctl_res = -1;
    this->mock->errnoValue = EINVAL;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(this->mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->validateHostPtr(boArray, 1, osContext.get(), 0, 1);
    EXPECT_EQ(EINVAL, ret);
}

TEST_F(DrmBufferObjectTest, givenResidentBOWhenPrintExecutionBufferIsSetToTrueThenDebugInformationAboutBOIsPrinted) {
    mock->ioctl_expected.total = 1;
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintExecutionBuffer.set(true);

    std::unique_ptr<uint32_t[]> buff(new uint32_t[1024]);
    std::unique_ptr<BufferObject> bo(new TestedBufferObject(this->mock.get()));
    ASSERT_NE(nullptr, bo.get());
    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    BufferObject *boArray[1] = {bo.get()};

    testing::internal::CaptureStdout();
    auto ret = bo->pin(boArray, 1, osContext.get(), 0, 1);
    EXPECT_EQ(0, ret);

    std::string output = testing::internal::GetCapturedStdout();
    auto idx = output.find("drm_i915_gem_execbuffer2 {");
    size_t expectedValue = 29;
    EXPECT_EQ(expectedValue, idx);

    idx = output.find("Buffer Object = { handle: BO-");
    EXPECT_NE(std::string::npos, idx);

    idx = output.find("Command Buffer Object = { handle: BO-");
    EXPECT_NE(std::string::npos, idx);
}

TEST_F(DrmBufferObjectTest, whenPrintBOCreateDestroyResultFlagIsSetAndCloseIsCalledOnBOThenDebugInfromationIsPrinted) {
    mock->ioctl_expected.total = 1;
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    testing::internal::CaptureStdout();
    bool result = bo->close();
    EXPECT_EQ(true, result);

    std::string output = testing::internal::GetCapturedStdout();
    size_t idx = output.find("Calling gem close on handle: BO-");
    size_t expectedValue = 0;
    EXPECT_EQ(expectedValue, idx);
}

TEST_F(DrmBufferObjectTest, whenPrintExecutionBufferIsSetToTrueThenMessageFoundInStdStream) {
    mock->ioctl_expected.total = 1;
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintExecutionBuffer.set(true);
    ExecObject execObjectsStorage = {};

    testing::internal::CaptureStdout();
    auto ret = bo->exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0);
    EXPECT_EQ(0, ret);

    std::string output = testing::internal::GetCapturedStdout();
    auto idx = output.find("drm_i915_gem_execbuffer2 {");
    size_t expectedValue = 29;
    EXPECT_EQ(expectedValue, idx);
}

TEST(DrmBufferObjectSimpleTest, givenInvalidBoWhenValidateHostptrIsCalledThenErrorIsReturned) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock.get(), 0u);
    OsContextLinux osContext(*mock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    ASSERT_NE(nullptr, mock.get());
    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(mock.get()));
    ASSERT_NE(nullptr, bo.get());

    // fail DRM_IOCTL_I915_GEM_EXECBUFFER2 in pin
    mock->ioctl_res = -1;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    mock->errnoValue = EFAULT;

    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->pin(boArray, 1, &osContext, 0, 1);
    EXPECT_EQ(EFAULT, ret);
    mock->ioctl_res = 0;
}

TEST(DrmBufferObjectSimpleTest, givenInvalidBoWhenPinIsCalledThenErrorIsReturned) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock.get(), 0u);
    OsContextLinux osContext(*mock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    ASSERT_NE(nullptr, mock.get());
    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(mock.get()));
    ASSERT_NE(nullptr, bo.get());

    // fail DRM_IOCTL_I915_GEM_EXECBUFFER2 in pin
    mock->ioctl_res = -1;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    mock->errnoValue = EFAULT;

    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->validateHostPtr(boArray, 1, &osContext, 0, 1);
    EXPECT_EQ(EFAULT, ret);
    mock->ioctl_res = 0;
}

TEST(DrmBufferObjectSimpleTest, givenBufferObjectWhenConstructedWithASizeThenTheSizeIsInitialized) {
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<DrmMockCustom> drmMock(new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]));
    std::unique_ptr<BufferObject> bo(new BufferObject(drmMock.get(), 3, 1, 0x1000, 1));

    EXPECT_EQ(0x1000u, bo->peekSize());
}

TEST(DrmBufferObjectSimpleTest, givenArrayOfBosWhenPinnedThenAllBosArePinned) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]));
    ASSERT_NE(nullptr, mock.get());
    OsContextLinux osContext(*mock, 0u, EngineDescriptorHelper::getDefaultDescriptor());

    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(mock.get()));
    ASSERT_NE(nullptr, bo.get());
    mock->ioctl_res = 0;

    std::unique_ptr<TestedBufferObject> boToPin(new TestedBufferObject(mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin2(new TestedBufferObject(mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin3(new TestedBufferObject(mock.get()));

    ASSERT_NE(nullptr, boToPin.get());
    ASSERT_NE(nullptr, boToPin2.get());
    ASSERT_NE(nullptr, boToPin3.get());

    BufferObject *array[3] = {boToPin.get(), boToPin2.get(), boToPin3.get()};

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    auto ret = bo->pin(array, 3, &osContext, 0, 1);
    EXPECT_EQ(mock->ioctl_res, ret);

    EXPECT_LT(0u, mock->execBuffer.getBatchLen());
    EXPECT_EQ(4u, mock->execBuffer.getBufferCount()); // 3 bos to pin plus 1 exec bo
    EXPECT_EQ(reinterpret_cast<uintptr_t>(boToPin->execObjectPointerFilled), mock->execBuffer.getBuffersPtr());
    EXPECT_NE(nullptr, boToPin2->execObjectPointerFilled);
    EXPECT_NE(nullptr, boToPin3->execObjectPointerFilled);

    bo->setAddress(0llu);
}

TEST(DrmBufferObjectSimpleTest, givenArrayOfBosWhenValidatedThenAllBosArePinned) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]));
    ASSERT_NE(nullptr, mock.get());
    OsContextLinux osContext(*mock, 0u, EngineDescriptorHelper::getDefaultDescriptor());

    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(mock.get()));
    ASSERT_NE(nullptr, bo.get());
    mock->ioctl_res = 0;

    std::unique_ptr<TestedBufferObject> boToPin(new TestedBufferObject(mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin2(new TestedBufferObject(mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin3(new TestedBufferObject(mock.get()));

    ASSERT_NE(nullptr, boToPin.get());
    ASSERT_NE(nullptr, boToPin2.get());
    ASSERT_NE(nullptr, boToPin3.get());

    BufferObject *array[3] = {boToPin.get(), boToPin2.get(), boToPin3.get()};

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    auto ret = bo->validateHostPtr(array, 3, &osContext, 0, 1);
    EXPECT_EQ(mock->ioctl_res, ret);

    EXPECT_LT(0u, mock->execBuffer.getBatchLen());
    EXPECT_EQ(4u, mock->execBuffer.getBufferCount()); // 3 bos to pin plus 1 exec bo
    EXPECT_EQ(reinterpret_cast<uintptr_t>(boToPin->execObjectPointerFilled), mock->execBuffer.getBuffersPtr());
    EXPECT_NE(nullptr, boToPin2->execObjectPointerFilled);
    EXPECT_NE(nullptr, boToPin3->execObjectPointerFilled);

    bo->setAddress(0llu);
}

TEST_F(DrmBufferObjectTest, givenDeleterWhenBufferObjectIsCreatedAndDeletedThenCloseIsCalled) {
    mock->ioctl_cnt.reset();
    mock->ioctl_expected.reset();

    {
        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new BufferObject(mock.get(), 3, 1, 0x1000, 1));
    }

    EXPECT_EQ(1, mock->ioctl_cnt.gemClose);
    mock->ioctl_cnt.reset();
    mock->ioctl_expected.reset();
}

TEST(DrmBufferObject, givenPerContextVmRequiredWhenBoCreatedThenBindInfoIsInitializedToOsContextCount) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->getRootDeviceEnvironment().executionEnvironment.setDebuggingEnabled();
    device->getExecutionEnvironment()->calculateMaxOsContextCount();
    DrmMock drm(*(device->getExecutionEnvironment()->rootDeviceEnvironments[0].get()));
    EXPECT_TRUE(drm.isPerContextVMRequired());
    auto osContextCount = device->getExecutionEnvironment()->memoryManager->getRegisteredEnginesCount();
    MockBufferObject bo(&drm, 3, 0, 0, osContextCount);

    EXPECT_EQ(osContextCount, bo.bindInfo.size());

    for (auto &iter : bo.bindInfo) {
        for (uint32_t i = 0; i < EngineLimits::maxHandleCount; i++) {
            EXPECT_FALSE(iter[i]);
        }
    }
}

TEST(DrmBufferObject, givenDrmIoctlReturnsErrorNotSupportedThenBufferObjectReturnsError) {
    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingEnabled();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockReturnErrorNotSupported *drm = new DrmMockReturnErrorNotSupported(*executionEnvironment->rootDeviceEnvironments[0]);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);

    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto osContextCount = device->getExecutionEnvironment()->memoryManager->getRegisteredEnginesCount();
    MockBufferObject bo(drm, 3, 0, 0, osContextCount);

    std::unique_ptr<OsContextLinux> osContext;
    osContext.reset(new OsContextLinux(*drm, 0u, EngineDescriptorHelper::getDefaultDescriptor()));

    ExecObject execObjectsStorage = {};
    auto ret = bo.exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, 0, 0);
    EXPECT_NE(0, ret);
}

TEST(DrmBufferObject, givenPerContextVmRequiredWhenBoBoundAndUnboundThenCorrectBindInfoIsUpdated) {
    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingEnabled();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing *drm = new DrmMockNonFailing(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_TRUE(drm->isPerContextVMRequired());

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);

    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto osContextCount = device->getExecutionEnvironment()->memoryManager->getRegisteredEnginesCount();
    MockBufferObject bo(drm, 3, 0, 0, osContextCount);

    EXPECT_EQ(osContextCount, bo.bindInfo.size());

    auto contextId = device->getExecutionEnvironment()->memoryManager->getRegisteredEnginesCount() / 2;
    auto osContext = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines()[contextId].osContext;
    osContext->ensureContextInitialized();

    bo.bind(osContext, 0);
    EXPECT_TRUE(bo.bindInfo[contextId][0]);

    bo.unbind(osContext, 0);
    EXPECT_FALSE(bo.bindInfo[contextId][0]);
}

TEST(DrmBufferObject, givenPrintBOBindingResultWhenBOBindAndUnbindSucceedsThenPrintDebugInformationAboutBOBindingResult) {
    struct DrmMockToSucceedBindBufferObject : public DrmMock {
        DrmMockToSucceedBindBufferObject(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}
        int bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) override { return 0; }
        int unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) override { return 0; }
    };

    DebugManagerStateRestore restore;
    DebugManager.flags.PrintBOBindingResult.set(true);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingEnabled();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto drm = new DrmMockToSucceedBindBufferObject(*executionEnvironment->rootDeviceEnvironments[0]);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);

    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto osContextCount = device->getExecutionEnvironment()->memoryManager->getRegisteredEnginesCount();
    MockBufferObject bo(drm, 3, 0, 0, osContextCount);

    EXPECT_EQ(osContextCount, bo.bindInfo.size());

    auto contextId = device->getExecutionEnvironment()->memoryManager->getRegisteredEnginesCount() / 2;
    auto osContext = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines()[contextId].osContext;
    osContext->ensureContextInitialized();

    testing::internal::CaptureStdout();

    bo.bind(osContext, 0);
    EXPECT_TRUE(bo.bindInfo[contextId][0]);

    std::string bindOutput = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(bindOutput.c_str(), "bind BO-0 to VM 0, drmVmId = 1, range: 0 - 0, size: 0, result: 0\n");

    testing::internal::CaptureStdout();

    bo.unbind(osContext, 0);
    EXPECT_FALSE(bo.bindInfo[contextId][0]);

    std::string unbindOutput = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(unbindOutput.c_str(), "unbind BO-0 from VM 0, drmVmId = 1, range: 0 - 0, size: 0, result: 0\n");
}

TEST(DrmBufferObject, givenPrintBOBindingResultWhenBOBindAndUnbindFailsThenPrintDebugInformationAboutBOBindingResultWithErrno) {
    struct DrmMockToFailBindBufferObject : public DrmMock {
        DrmMockToFailBindBufferObject(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}
        int bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) override { return -1; }
        int unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) override { return -1; }
        int getErrno() override { return EINVAL; }
    };

    DebugManagerStateRestore restore;
    DebugManager.flags.PrintBOBindingResult.set(true);

    auto executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->setDebuggingEnabled();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto drm = new DrmMockToFailBindBufferObject(*executionEnvironment->rootDeviceEnvironments[0]);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);

    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

    auto osContextCount = device->getExecutionEnvironment()->memoryManager->getRegisteredEnginesCount();
    MockBufferObject bo(drm, 3, 0, 0, osContextCount);

    EXPECT_EQ(osContextCount, bo.bindInfo.size());

    auto contextId = device->getExecutionEnvironment()->memoryManager->getRegisteredEnginesCount() / 2;
    auto osContext = device->getExecutionEnvironment()->memoryManager->getRegisteredEngines()[contextId].osContext;
    osContext->ensureContextInitialized();

    testing::internal::CaptureStderr();

    bo.bind(osContext, 0);
    EXPECT_FALSE(bo.bindInfo[contextId][0]);

    std::string bindOutput = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(hasSubstr(bindOutput, "bind BO-0 to VM 0, drmVmId = 1, range: 0 - 0, size: 0, result: -1, errno: 22"));

    testing::internal::CaptureStderr();
    bo.bindInfo[contextId][0] = true;

    bo.unbind(osContext, 0);
    EXPECT_TRUE(bo.bindInfo[contextId][0]);

    std::string unbindOutput = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(hasSubstr(unbindOutput, "unbind BO-0 from VM 0, drmVmId = 1, range: 0 - 0, size: 0, result: -1, errno: 22"));
}

TEST(DrmBufferObject, whenBindExtHandleAddedThenItIsStored) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    bo.addBindExtHandle(4);

    EXPECT_EQ(1u, bo.bindExtHandles.size());
    EXPECT_EQ(4u, bo.bindExtHandles[0]);

    EXPECT_EQ(1u, bo.getBindExtHandles().size());
    EXPECT_EQ(4u, bo.getBindExtHandles()[0]);
}

TEST(DrmBufferObject, whenMarkForCapturedCalledThenIsMarkedForCaptureReturnsTrue) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockBufferObject bo(&drm, 3, 0, 0, 1);
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
    DebugManager.flags.UseAsyncDrmExec.set(1);

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

    MockBufferObject bo(&drm, 3, 0, 0, 1);
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

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    bo.setAddress(address);
    auto boAddress = bo.peekAddress();
    EXPECT_EQ(boAddress, expectedAddress);
}

TEST_F(DrmBufferObjectTest, givenBoIsCreatedWhenPageFaultIsSupportedThenExplicitResidencyIsRequiredByDefault) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    DrmMock drm(*(executionEnvironment.rootDeviceEnvironments[0].get()));

    for (auto isPageFaultSupported : {false, true}) {
        drm.pageFaultSupported = isPageFaultSupported;
        MockBufferObject bo(&drm, 3, 0, 0, 1);
        EXPECT_EQ(isPageFaultSupported, bo.isExplicitResidencyRequired());
    }
}

TEST_F(DrmBufferObjectTest, whenBoRequiresExplicitResidencyThenTheCorrespondingQueryReturnsCorrectValue) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    DrmMock drm(*(executionEnvironment.rootDeviceEnvironments[0].get()));
    MockBufferObject bo(&drm, 3, 0, 0, 1);

    for (auto required : {false, true}) {
        bo.requireExplicitResidency(required);
        EXPECT_EQ(required, bo.isExplicitResidencyRequired());
    }
}
