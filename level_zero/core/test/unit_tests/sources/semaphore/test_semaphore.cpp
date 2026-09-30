/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

using namespace NEO;
#include "gtest/gtest.h"

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamilyImmediate;
} // namespace L0

namespace L0 {
namespace ult {

class MockNeoExtSemaphore : public NEO::ExternalSemaphore {
  public:
    using NEO::ExternalSemaphore::syncHandle;

    explicit MockNeoExtSemaphore(NEO::OSInterface *osInterface) {
        this->osInterface = osInterface;
    }

    ImportResult importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override {
        return ImportResult::success;
    }

    uint64_t acquireWaitFenceValue(uint64_t fenceValue) override {
        acquireWaitFenceValueCalledTimes++;
        lastAcquireWaitFenceValueArg = fenceValue;
        return acquireWaitFenceValueReturnValue;
    }

    uint64_t acquireSignalFenceValue(uint64_t fenceValue) override {
        acquireSignalFenceValueCalledTimes++;
        lastAcquireSignalFenceValueArg = fenceValue;
        return acquireSignalFenceValueReturnValue;
    }

    uint32_t cpuWaitCalledTimes = 0u;
    uint32_t cpuSignalCalledTimes = 0u;
    uint64_t lastCpuWaitValue = 0u;
    uint64_t lastCpuSignalValue = 0u;
    bool cpuWaitReturnValue = true;
    bool cpuSignalReturnValue = true;
    uint32_t acquireWaitFenceValueCalledTimes = 0u;
    uint32_t acquireSignalFenceValueCalledTimes = 0u;
    uint64_t lastAcquireWaitFenceValueArg = 0u;
    uint64_t lastAcquireSignalFenceValueArg = 0u;
    uint64_t acquireWaitFenceValueReturnValue = 0u;
    uint64_t acquireSignalFenceValueReturnValue = 0u;
};

class MockExtSemDriverModel : public NEO::MockDriverModel {
  public:
    bool waitExternalSemaphoresFromCpu(std::span<const NEO::ExternalSemaphoreOperation> waits) override {
        cpuWaitCalls++;
        lastCpuOperationCount = waits.size();
        bool result = true;

        for (const auto &wait : waits) {
            auto &mockSemaphore = const_cast<MockNeoExtSemaphore &>(static_cast<const MockNeoExtSemaphore &>(*wait.semaphore));
            mockSemaphore.cpuWaitCalledTimes++;
            mockSemaphore.lastCpuWaitValue = wait.fenceValue;
            result &= mockSemaphore.cpuWaitReturnValue;
        }

        return result;
    }

    bool signalExternalSemaphoresFromCpu(std::span<const NEO::ExternalSemaphoreOperation> signals) override {
        cpuSignalCalls++;
        lastCpuOperationCount = signals.size();
        bool result = true;

        for (const auto &signal : signals) {
            auto &mockSemaphore = const_cast<MockNeoExtSemaphore &>(static_cast<const MockNeoExtSemaphore &>(*signal.semaphore));
            mockSemaphore.cpuSignalCalledTimes++;
            mockSemaphore.lastCpuSignalValue = signal.fenceValue;
            result &= mockSemaphore.cpuSignalReturnValue;
        }

        return result;
    }

    uint32_t cpuWaitCalls = 0u;
    uint32_t cpuSignalCalls = 0u;
    size_t lastCpuOperationCount = 0u;
};

struct ExternalSemaphoreFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();

        auto &osInterface = neoDevice->getRootDeviceEnvironmentRef().osInterface;
        osInterface = std::make_unique<NEO::OSInterface>();
        osInterface->setDriverModel(std::make_unique<MockExtSemDriverModel>());
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }
};

using ExternalSemaphoreTest = Test<ExternalSemaphoreFixture>;

HWTEST_F(ExternalSemaphoreTest, givenInvalidDescriptorAndImportExternalSemaphoreExpIsCalledThenInvalidArgumentsIsReturned) {
    ze_device_handle_t hDevice = device->toHandle();
    const ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    ze_result_t result = zeDeviceImportExternalSemaphoreExt(hDevice, &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST_F(ExternalSemaphoreTest, givenValidParametersWhenReleaseExternalSemaphoreIsCalledThenSuccessIsReturned) {
    auto externalSemaphoreImp = new ExternalSemaphoreImp();
    ze_result_t result = externalSemaphoreImp->releaseExternalSemaphore();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ExternalSemaphoreTest, givenInvalidDescriptorWhenInitializeIsCalledThenInvalidArgumentIsReturned) {
    ze_device_handle_t hDevice = device->toHandle();
    const ze_external_semaphore_ext_desc_t desc = {};
    auto externalSemaphoreImp = new ExternalSemaphoreImp();
    ze_result_t result = externalSemaphoreImp->initialize(hDevice, &desc);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    delete externalSemaphoreImp;
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreWaitOperationDataWhenSemaphoreWaitIsCalledThenDriverModelWaitsOnAllSemaphoresWithProvidedValuesInSingleCall) {
    ExternalSemaphoreImp semaphore0;
    ExternalSemaphoreImp semaphore1;

    semaphore0.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore0 = static_cast<MockNeoExtSemaphore *>(semaphore0.neoExternalSemaphore.get());
    semaphore1.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore1 = static_cast<MockNeoExtSemaphore *>(semaphore1.neoExternalSemaphore.get());

    MockExtSemDriverModel driverModel;
    ExternalSemaphoreOperationData operationData{};
    operationData.add(semaphore0, 10u);
    operationData.add(semaphore1, 20u);

    ExternalSemaphoreImp::semaphoreWait(driverModel, operationData);

    EXPECT_EQ(1u, neoSemaphore0->cpuWaitCalledTimes);
    EXPECT_EQ(10u, neoSemaphore0->lastCpuWaitValue);
    EXPECT_EQ(0u, neoSemaphore0->cpuSignalCalledTimes);

    EXPECT_EQ(1u, neoSemaphore1->cpuWaitCalledTimes);
    EXPECT_EQ(20u, neoSemaphore1->lastCpuWaitValue);
    EXPECT_EQ(0u, neoSemaphore1->cpuSignalCalledTimes);

    EXPECT_EQ(1u, driverModel.cpuWaitCalls);
    EXPECT_EQ(2u, driverModel.lastCpuOperationCount);
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreSignalOperationDataWhenSemaphoreSignalIsCalledThenDriverModelSignalsAllSemaphoresWithProvidedValuesInSingleCall) {
    ExternalSemaphoreImp semaphore0;
    ExternalSemaphoreImp semaphore1;

    semaphore0.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore0 = static_cast<MockNeoExtSemaphore *>(semaphore0.neoExternalSemaphore.get());
    semaphore1.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore1 = static_cast<MockNeoExtSemaphore *>(semaphore1.neoExternalSemaphore.get());

    MockExtSemDriverModel driverModel;
    ExternalSemaphoreOperationData operationData{};
    operationData.add(semaphore0, 30u);
    operationData.add(semaphore1, 40u);

    ExternalSemaphoreImp::semaphoreSignal(driverModel, operationData);

    EXPECT_EQ(1u, neoSemaphore0->cpuSignalCalledTimes);
    EXPECT_EQ(30u, neoSemaphore0->lastCpuSignalValue);
    EXPECT_EQ(0u, neoSemaphore0->cpuWaitCalledTimes);

    EXPECT_EQ(1u, neoSemaphore1->cpuSignalCalledTimes);
    EXPECT_EQ(40u, neoSemaphore1->lastCpuSignalValue);
    EXPECT_EQ(0u, neoSemaphore1->cpuWaitCalledTimes);

    EXPECT_EQ(1u, driverModel.cpuSignalCalls);
    EXPECT_EQ(2u, driverModel.lastCpuOperationCount);
}

HWTEST_F(ExternalSemaphoreTest, givenPrintExternalSemaphoreOperationResultsEnabledWhenSemaphoreWaitIsCalledThenDebugMessageIsPrintedForEachSemaphore) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreOperationResults.set(1);

    ExternalSemaphoreImp semaphore0;
    ExternalSemaphoreImp semaphore1;

    semaphore0.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore0 = static_cast<MockNeoExtSemaphore *>(semaphore0.neoExternalSemaphore.get());
    semaphore1.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore1 = static_cast<MockNeoExtSemaphore *>(semaphore1.neoExternalSemaphore.get());
    neoSemaphore0->cpuWaitReturnValue = true;
    neoSemaphore1->cpuWaitReturnValue = false;
    neoSemaphore0->syncHandle = 0x10;
    neoSemaphore1->syncHandle = 0x20;

    MockExtSemDriverModel driverModel;
    ExternalSemaphoreOperationData operationData{};
    operationData.add(semaphore0, 10u);
    operationData.add(semaphore1, 20u);

    StreamCapture capture;
    capture.captureStdout();
    ExternalSemaphoreImp::semaphoreWait(driverModel, operationData);
    std::string output = capture.getCapturedStdout();

    char expected[512];
    snprintf(expected, sizeof(expected),
             "ExternalSemaphoreImp::semaphoreWait semaphore=%p handle=0x10 value=10 result=0\n"
             "ExternalSemaphoreImp::semaphoreWait semaphore=%p handle=0x20 value=20 result=0\n",
             static_cast<void *>(neoSemaphore0),
             static_cast<void *>(neoSemaphore1));
    EXPECT_STREQ(expected, output.c_str());
}

HWTEST_F(ExternalSemaphoreTest, givenPrintExternalSemaphoreOperationResultsDisabledWhenSemaphoreWaitIsCalledThenNoDebugMessageIsPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreOperationResults.set(0);

    ExternalSemaphoreImp semaphore0;
    semaphore0.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());

    MockExtSemDriverModel driverModel;
    ExternalSemaphoreOperationData operationData{};
    operationData.add(semaphore0, 10u);

    StreamCapture capture;
    capture.captureStdout();
    ExternalSemaphoreImp::semaphoreWait(driverModel, operationData);
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.empty());
}

HWTEST_F(ExternalSemaphoreTest, givenPrintExternalSemaphoreOperationResultsEnabledWhenSemaphoreSignalIsCalledThenDebugMessageIsPrintedForEachSemaphore) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreOperationResults.set(1);

    ExternalSemaphoreImp semaphore0;
    ExternalSemaphoreImp semaphore1;

    semaphore0.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore0 = static_cast<MockNeoExtSemaphore *>(semaphore0.neoExternalSemaphore.get());
    semaphore1.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore1 = static_cast<MockNeoExtSemaphore *>(semaphore1.neoExternalSemaphore.get());
    neoSemaphore0->cpuSignalReturnValue = true;
    neoSemaphore1->cpuSignalReturnValue = false;
    neoSemaphore0->syncHandle = 0x30;
    neoSemaphore1->syncHandle = 0x40;

    MockExtSemDriverModel driverModel;
    ExternalSemaphoreOperationData operationData{};
    operationData.add(semaphore0, 30u);
    operationData.add(semaphore1, 40u);

    StreamCapture capture;
    capture.captureStdout();
    ExternalSemaphoreImp::semaphoreSignal(driverModel, operationData);
    std::string output = capture.getCapturedStdout();

    char expected[512];
    snprintf(expected, sizeof(expected),
             "ExternalSemaphoreImp::semaphoreSignal semaphore=%p handle=0x30 value=30 result=0\n"
             "ExternalSemaphoreImp::semaphoreSignal semaphore=%p handle=0x40 value=40 result=0\n",
             static_cast<void *>(neoSemaphore0),
             static_cast<void *>(neoSemaphore1));
    EXPECT_STREQ(expected, output.c_str());
}

HWTEST_F(ExternalSemaphoreTest, givenPrintExternalSemaphoreOperationResultsDisabledWhenSemaphoreSignalIsCalledThenNoDebugMessageIsPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreOperationResults.set(0);

    ExternalSemaphoreImp semaphore0;
    semaphore0.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());

    MockExtSemDriverModel driverModel;
    ExternalSemaphoreOperationData operationData{};
    operationData.add(semaphore0, 30u);

    StreamCapture capture;
    capture.captureStdout();
    ExternalSemaphoreImp::semaphoreSignal(driverModel, operationData);
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.empty());
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenAppendWaitExternalSemaphoresIsCalledOnRegularCmdListThenWaitHostFunctionIsAppended) {
    MockCommandListExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ExternalSemaphoreImp semaphore;
    semaphore.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore = static_cast<MockNeoExtSemaphore *>(semaphore.neoExternalSemaphore.get());
    neoSemaphore->acquireWaitFenceValueReturnValue = 456u;
    ze_external_semaphore_ext_handle_t hSemaphore = semaphore.toHandle();
    ze_external_semaphore_wait_params_ext_t waitParams = {};
    waitParams.value = 123u;

    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, cmdList.appendHostFunctionCalledTimes);
    EXPECT_EQ(&MockCommandListExtSem<FamilyType::gfxCoreFamily>::semaphoreWaitHostFunction, cmdList.capturedHostFunction);

    ASSERT_NE(nullptr, cmdList.capturedUserData);
    auto hostFunctionData = static_cast<MockCommandListExtSem<FamilyType::gfxCoreFamily>::ExternalSemaphoreHostFunctionData *>(cmdList.capturedUserData);
    ASSERT_EQ(1u, hostFunctionData->operationData.operations.size());
    EXPECT_EQ(neoSemaphore, hostFunctionData->operationData.operations[0].semaphore);
    EXPECT_EQ(1u, neoSemaphore->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore->acquireSignalFenceValueCalledTimes);
    EXPECT_EQ(123u, neoSemaphore->lastAcquireWaitFenceValueArg);
    EXPECT_EQ(456u, hostFunctionData->operationData.operations[0].fenceValue);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenAppendSignalExternalSemaphoresIsCalledOnRegularCmdListThenSignalHostFunctionIsAppended) {
    MockCommandListExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ExternalSemaphoreImp semaphore;
    semaphore.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore = static_cast<MockNeoExtSemaphore *>(semaphore.neoExternalSemaphore.get());
    neoSemaphore->acquireSignalFenceValueReturnValue = 654u;
    ze_external_semaphore_ext_handle_t hSemaphore = semaphore.toHandle();
    ze_external_semaphore_signal_params_ext_t signalParams = {};
    signalParams.value = 321u;

    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, cmdList.appendHostFunctionCalledTimes);
    EXPECT_EQ(&MockCommandListExtSem<FamilyType::gfxCoreFamily>::semaphoreSignalHostFunction, cmdList.capturedHostFunction);

    ASSERT_NE(nullptr, cmdList.capturedUserData);
    auto hostFunctionData = static_cast<MockCommandListExtSem<FamilyType::gfxCoreFamily>::ExternalSemaphoreHostFunctionData *>(cmdList.capturedUserData);
    ASSERT_EQ(1u, hostFunctionData->operationData.operations.size());
    EXPECT_EQ(neoSemaphore, hostFunctionData->operationData.operations[0].semaphore);
    EXPECT_EQ(1u, neoSemaphore->acquireSignalFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(321u, neoSemaphore->lastAcquireSignalFenceValueArg);
    EXPECT_EQ(654u, hostFunctionData->operationData.operations[0].fenceValue);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenAppendWaitExternalSemaphoresIsCalledOnImmediateCmdListThenWaitHostFunctionIsAppended) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);

    ExternalSemaphoreImp semaphore;
    semaphore.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore = static_cast<MockNeoExtSemaphore *>(semaphore.neoExternalSemaphore.get());
    neoSemaphore->acquireWaitFenceValueReturnValue = 77u;
    ze_external_semaphore_ext_handle_t hSemaphore = semaphore.toHandle();
    ze_external_semaphore_wait_params_ext_t waitParams = {};
    waitParams.value = 55u;

    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, cmdList.appendHostFunctionCalledTimes);
    EXPECT_EQ(&MockCommandListExtSem<FamilyType::gfxCoreFamily>::semaphoreWaitHostFunction, cmdList.capturedHostFunction);

    ASSERT_NE(nullptr, cmdList.capturedUserData);
    auto hostFunctionData = static_cast<MockCommandListExtSem<FamilyType::gfxCoreFamily>::ExternalSemaphoreHostFunctionData *>(cmdList.capturedUserData);
    ASSERT_EQ(1u, hostFunctionData->operationData.operations.size());
    EXPECT_EQ(neoSemaphore, hostFunctionData->operationData.operations[0].semaphore);
    EXPECT_EQ(1u, neoSemaphore->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore->acquireSignalFenceValueCalledTimes);
    EXPECT_EQ(55u, neoSemaphore->lastAcquireWaitFenceValueArg);
    EXPECT_EQ(77u, hostFunctionData->operationData.operations[0].fenceValue);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenAppendSignalExternalSemaphoresIsCalledOnImmediateCmdListThenSignalHostFunctionIsAppended) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);

    ExternalSemaphoreImp semaphore;
    semaphore.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore = static_cast<MockNeoExtSemaphore *>(semaphore.neoExternalSemaphore.get());
    neoSemaphore->acquireSignalFenceValueReturnValue = 88u;
    ze_external_semaphore_ext_handle_t hSemaphore = semaphore.toHandle();
    ze_external_semaphore_signal_params_ext_t signalParams = {};
    signalParams.value = 66u;

    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, cmdList.appendHostFunctionCalledTimes);
    EXPECT_EQ(&MockCommandListExtSem<FamilyType::gfxCoreFamily>::semaphoreSignalHostFunction, cmdList.capturedHostFunction);

    ASSERT_NE(nullptr, cmdList.capturedUserData);
    auto hostFunctionData = static_cast<MockCommandListExtSem<FamilyType::gfxCoreFamily>::ExternalSemaphoreHostFunctionData *>(cmdList.capturedUserData);
    ASSERT_EQ(1u, hostFunctionData->operationData.operations.size());
    EXPECT_EQ(neoSemaphore, hostFunctionData->operationData.operations[0].semaphore);
    EXPECT_EQ(1u, neoSemaphore->acquireSignalFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(66u, neoSemaphore->lastAcquireSignalFenceValueArg);
    EXPECT_EQ(88u, hostFunctionData->operationData.operations[0].fenceValue);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenSemaphoreWaitHostFunctionIsCalledOnRegularCmdListThenHostFunctionDataIsPreserved) {
    MockCommandListExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ExternalSemaphoreImp semaphore;
    semaphore.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore = static_cast<MockNeoExtSemaphore *>(semaphore.neoExternalSemaphore.get());

    ExternalSemaphoreOperationData operationData;
    operationData.add(semaphore, 7u);
    auto &hostFunctionData = cmdList.externalSemaphoreHostFunctionData.emplace_back(cmdList, std::move(operationData));
    hostFunctionData.self = std::prev(cmdList.externalSemaphoreHostFunctionData.end());

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily>::semaphoreWaitHostFunction(&hostFunctionData);
    EXPECT_EQ(cmdList.externalSemaphoreHostFunctionData.size(), 1u);
    EXPECT_EQ(neoSemaphore->cpuWaitCalledTimes, 1u);
    EXPECT_EQ(neoSemaphore->lastCpuWaitValue, 7u);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenSemaphoreSignalHostFunctionIsCalledOnRegularCmdListThenHostFunctionDataIsPreserved) {
    MockCommandListExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ExternalSemaphoreImp semaphore;
    semaphore.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore = static_cast<MockNeoExtSemaphore *>(semaphore.neoExternalSemaphore.get());

    ExternalSemaphoreOperationData operationData;
    operationData.add(semaphore, 7u);
    auto &hostFunctionData = cmdList.externalSemaphoreHostFunctionData.emplace_back(cmdList, std::move(operationData));
    hostFunctionData.self = std::prev(cmdList.externalSemaphoreHostFunctionData.end());

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily>::semaphoreSignalHostFunction(&hostFunctionData);
    EXPECT_EQ(cmdList.externalSemaphoreHostFunctionData.size(), 1u);
    EXPECT_EQ(neoSemaphore->cpuSignalCalledTimes, 1u);
    EXPECT_EQ(neoSemaphore->lastCpuSignalValue, 7u);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenSemaphoreWaitHostFunctionIsCalledOnImmediateCmdListThenHostFunctionDataIsNotPreserved) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);

    ExternalSemaphoreImp semaphore;
    semaphore.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore = static_cast<MockNeoExtSemaphore *>(semaphore.neoExternalSemaphore.get());

    ExternalSemaphoreOperationData operationData;
    operationData.add(semaphore, 7u);
    auto &hostFunctionData = cmdList.externalSemaphoreHostFunctionData.emplace_back(cmdList, std::move(operationData));
    hostFunctionData.self = std::prev(cmdList.externalSemaphoreHostFunctionData.end());

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily>::semaphoreWaitHostFunction(&hostFunctionData);
    EXPECT_EQ(cmdList.externalSemaphoreHostFunctionData.size(), 0u);
    EXPECT_EQ(neoSemaphore->cpuWaitCalledTimes, 1u);
    EXPECT_EQ(neoSemaphore->lastCpuWaitValue, 7u);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenSemaphoreSignalHostFunctionIsCalledOnImmediateCmdListThenHostFunctionDataIsNotPreserved) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);

    ExternalSemaphoreImp semaphore;
    semaphore.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    auto neoSemaphore = static_cast<MockNeoExtSemaphore *>(semaphore.neoExternalSemaphore.get());

    ExternalSemaphoreOperationData operationData;
    operationData.add(semaphore, 7u);
    auto &hostFunctionData = cmdList.externalSemaphoreHostFunctionData.emplace_back(cmdList, std::move(operationData));
    hostFunctionData.self = std::prev(cmdList.externalSemaphoreHostFunctionData.end());

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily>::semaphoreSignalHostFunction(&hostFunctionData);
    EXPECT_EQ(cmdList.externalSemaphoreHostFunctionData.size(), 0u);
    EXPECT_EQ(neoSemaphore->cpuSignalCalledTimes, 1u);
    EXPECT_EQ(neoSemaphore->lastCpuSignalValue, 7u);
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoresWhenAreImportedToDeviceIsCalledThenTrueIsReturnedOnlyIfAllUseDeviceOsInterface) {
    NEO::OSInterface otherDeviceOsInterface;
    ExternalSemaphoreImp semaphores[3];
    ze_external_semaphore_ext_handle_t hSemaphores[3];
    for (uint32_t i = 0; i < 3; i++) {
        semaphores[i].neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
        hSemaphores[i] = semaphores[i].toHandle();
    }
    semaphores[2].neoExternalSemaphore->osInterface = &otherDeviceOsInterface;

    EXPECT_TRUE(ExternalSemaphoreImp::areImportedToDevice(*device, 0, nullptr));
    EXPECT_TRUE(ExternalSemaphoreImp::areImportedToDevice(*device, 2, hSemaphores));
    EXPECT_FALSE(ExternalSemaphoreImp::areImportedToDevice(*device, 3, hSemaphores));
    EXPECT_FALSE(ExternalSemaphoreImp::areImportedToDevice(*device, 1, &hSemaphores[2]));
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreImportedToDifferentDeviceWhenAppendingWaitOrSignalThenInvalidArgumentIsReturnedAndFenceValuesAreNotAcquired) {
    MockCommandListExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    NEO::OSInterface otherDeviceOsInterface;
    ExternalSemaphoreImp semaphore0;
    ExternalSemaphoreImp semaphore1;
    semaphore0.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(device->getOsInterface());
    semaphore1.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>(&otherDeviceOsInterface);
    auto neoSemaphore0 = static_cast<MockNeoExtSemaphore *>(semaphore0.neoExternalSemaphore.get());
    auto neoSemaphore1 = static_cast<MockNeoExtSemaphore *>(semaphore1.neoExternalSemaphore.get());
    ze_external_semaphore_ext_handle_t hSemaphores[] = {semaphore0.toHandle(), semaphore1.toHandle()};

    ze_external_semaphore_wait_params_ext_t waitParams[2] = {};
    ze_external_semaphore_signal_params_ext_t signalParams[2] = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, cmdList.appendWaitExternalSemaphores(2, hSemaphores, waitParams, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, cmdList.appendSignalExternalSemaphores(2, hSemaphores, signalParams, nullptr, 0, nullptr));

    EXPECT_EQ(0u, cmdList.appendHostFunctionCalledTimes);
    EXPECT_EQ(0u, neoSemaphore0->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore0->acquireSignalFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore1->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore1->acquireSignalFenceValueCalledTimes);
}

} // namespace ult
} // namespace L0
