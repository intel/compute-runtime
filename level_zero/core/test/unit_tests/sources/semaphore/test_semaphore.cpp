/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/stream_capture.h"
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

using ExternalSemaphoreTest = Test<DeviceFixture>;

class MockNeoExtSemaphore : public NEO::ExternalSemaphore {
  public:
    MockNeoExtSemaphore() : NEO::ExternalSemaphore() {}

    bool enqueueWait(uint64_t *fenceValue) override {
        enqueueWaitCalledTimes++;
        lastWaitValue = (fenceValue != nullptr) ? *fenceValue : 0u;
        return enqueueWaitReturnValue;
    }

    ImportResult importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override {
        return ImportResult::success;
    }

    bool enqueueSignal(uint64_t *fenceValue) override {
        enqueueSignalCalledTimes++;
        lastSignalValue = (fenceValue != nullptr) ? *fenceValue : 0u;
        return enqueueSignalReturnValue;
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

    uint32_t enqueueWaitCalledTimes = 0u;
    uint32_t enqueueSignalCalledTimes = 0u;
    uint64_t lastWaitValue = 0u;
    uint64_t lastSignalValue = 0u;
    bool enqueueWaitReturnValue = true;
    bool enqueueSignalReturnValue = true;
    uint32_t acquireWaitFenceValueCalledTimes = 0u;
    uint32_t acquireSignalFenceValueCalledTimes = 0u;
    uint64_t lastAcquireWaitFenceValueArg = 0u;
    uint64_t lastAcquireSignalFenceValueArg = 0u;
    uint64_t acquireWaitFenceValueReturnValue = 0u;
    uint64_t acquireSignalFenceValueReturnValue = 0u;
};

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

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreWaitOperationDataWhenWaitHostFunctionIsCalledThenEnqueueWaitIsCalledForEachSemaphoreWithProvidedValue) {
    ExternalSemaphoreImp semaphore0;
    ExternalSemaphoreImp semaphore1;

    auto neoSemaphore0 = new MockNeoExtSemaphore();
    auto neoSemaphore1 = new MockNeoExtSemaphore();
    semaphore0.neoExternalSemaphore.reset(neoSemaphore0);
    semaphore1.neoExternalSemaphore.reset(neoSemaphore1);

    ExternalSemaphoreOperationData operationData{};
    operationData.semaphores.push_back({&semaphore0, 10u});
    operationData.semaphores.push_back({&semaphore1, 20u});

    ExternalSemaphoreImp::semaphoreWait(operationData);

    EXPECT_EQ(1u, neoSemaphore0->enqueueWaitCalledTimes);
    EXPECT_EQ(10u, neoSemaphore0->lastWaitValue);
    EXPECT_EQ(0u, neoSemaphore0->enqueueSignalCalledTimes);

    EXPECT_EQ(1u, neoSemaphore1->enqueueWaitCalledTimes);
    EXPECT_EQ(20u, neoSemaphore1->lastWaitValue);
    EXPECT_EQ(0u, neoSemaphore1->enqueueSignalCalledTimes);
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreSignalOperationDataWhenSignalHostFunctionIsCalledThenEnqueueSignalIsCalledForEachSemaphoreWithProvidedValue) {
    ExternalSemaphoreImp semaphore0;
    ExternalSemaphoreImp semaphore1;

    auto neoSemaphore0 = new MockNeoExtSemaphore();
    auto neoSemaphore1 = new MockNeoExtSemaphore();
    semaphore0.neoExternalSemaphore.reset(neoSemaphore0);
    semaphore1.neoExternalSemaphore.reset(neoSemaphore1);

    ExternalSemaphoreOperationData operationData{};
    operationData.semaphores.push_back({&semaphore0, 30u});
    operationData.semaphores.push_back({&semaphore1, 40u});

    ExternalSemaphoreImp::semaphoreSignal(operationData);

    EXPECT_EQ(1u, neoSemaphore0->enqueueSignalCalledTimes);
    EXPECT_EQ(30u, neoSemaphore0->lastSignalValue);
    EXPECT_EQ(0u, neoSemaphore0->enqueueWaitCalledTimes);

    EXPECT_EQ(1u, neoSemaphore1->enqueueSignalCalledTimes);
    EXPECT_EQ(40u, neoSemaphore1->lastSignalValue);
    EXPECT_EQ(0u, neoSemaphore1->enqueueWaitCalledTimes);
}

HWTEST_F(ExternalSemaphoreTest, givenPrintExternalSemaphoreOperationResultsEnabledWhenSemaphoreWaitIsCalledThenDebugMessageIsPrintedForEachSemaphore) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreOperationResults.set(1);

    ExternalSemaphoreImp semaphore0;
    ExternalSemaphoreImp semaphore1;

    auto neoSemaphore0 = new MockNeoExtSemaphore();
    auto neoSemaphore1 = new MockNeoExtSemaphore();
    neoSemaphore0->enqueueWaitReturnValue = true;
    neoSemaphore1->enqueueWaitReturnValue = false;
    semaphore0.neoExternalSemaphore.reset(neoSemaphore0);
    semaphore1.neoExternalSemaphore.reset(neoSemaphore1);

    ExternalSemaphoreOperationData operationData{};
    operationData.semaphores.push_back({&semaphore0, 10u});
    operationData.semaphores.push_back({&semaphore1, 20u});

    StreamCapture capture;
    capture.captureStdout();
    ExternalSemaphoreImp::semaphoreWait(operationData);
    std::string output = capture.getCapturedStdout();

    char expected[512];
    snprintf(expected, sizeof(expected),
             "ExternalSemaphoreImp::semaphoreWait semaphore=%p value=10 result=1\n"
             "ExternalSemaphoreImp::semaphoreWait semaphore=%p value=20 result=0\n",
             static_cast<void *>(&semaphore0),
             static_cast<void *>(&semaphore1));
    EXPECT_STREQ(expected, output.c_str());
}

HWTEST_F(ExternalSemaphoreTest, givenPrintExternalSemaphoreOperationResultsDisabledWhenSemaphoreWaitIsCalledThenNoDebugMessageIsPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreOperationResults.set(0);

    ExternalSemaphoreImp semaphore0;
    semaphore0.neoExternalSemaphore.reset(new MockNeoExtSemaphore());

    ExternalSemaphoreOperationData operationData{};
    operationData.semaphores.push_back({&semaphore0, 10u});

    StreamCapture capture;
    capture.captureStdout();
    ExternalSemaphoreImp::semaphoreWait(operationData);
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.empty());
}

HWTEST_F(ExternalSemaphoreTest, givenPrintExternalSemaphoreOperationResultsEnabledWhenSemaphoreSignalIsCalledThenDebugMessageIsPrintedForEachSemaphore) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreOperationResults.set(1);

    ExternalSemaphoreImp semaphore0;
    ExternalSemaphoreImp semaphore1;

    auto neoSemaphore0 = new MockNeoExtSemaphore();
    auto neoSemaphore1 = new MockNeoExtSemaphore();
    neoSemaphore0->enqueueSignalReturnValue = true;
    neoSemaphore1->enqueueSignalReturnValue = false;
    semaphore0.neoExternalSemaphore.reset(neoSemaphore0);
    semaphore1.neoExternalSemaphore.reset(neoSemaphore1);

    ExternalSemaphoreOperationData operationData{};
    operationData.semaphores.push_back({&semaphore0, 30u});
    operationData.semaphores.push_back({&semaphore1, 40u});

    StreamCapture capture;
    capture.captureStdout();
    ExternalSemaphoreImp::semaphoreSignal(operationData);
    std::string output = capture.getCapturedStdout();

    char expected[512];
    snprintf(expected, sizeof(expected),
             "ExternalSemaphoreImp::semaphoreSignal semaphore=%p value=30 result=1\n"
             "ExternalSemaphoreImp::semaphoreSignal semaphore=%p value=40 result=0\n",
             static_cast<void *>(&semaphore0),
             static_cast<void *>(&semaphore1));
    EXPECT_STREQ(expected, output.c_str());
}

HWTEST_F(ExternalSemaphoreTest, givenPrintExternalSemaphoreOperationResultsDisabledWhenSemaphoreSignalIsCalledThenNoDebugMessageIsPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreOperationResults.set(0);

    ExternalSemaphoreImp semaphore0;
    semaphore0.neoExternalSemaphore.reset(new MockNeoExtSemaphore());

    ExternalSemaphoreOperationData operationData{};
    operationData.semaphores.push_back({&semaphore0, 30u});

    StreamCapture capture;
    capture.captureStdout();
    ExternalSemaphoreImp::semaphoreSignal(operationData);
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.empty());
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenAppendWaitExternalSemaphoresIsCalledOnRegularCmdListThenWaitHostFunctionIsAppended) {
    MockCommandListExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ExternalSemaphoreImp semaphore;
    auto neoSemaphore = new MockNeoExtSemaphore();
    neoSemaphore->acquireWaitFenceValueReturnValue = 456u;
    semaphore.neoExternalSemaphore.reset(neoSemaphore);
    ze_external_semaphore_ext_handle_t hSemaphore = semaphore.toHandle();
    ze_external_semaphore_wait_params_ext_t waitParams = {};
    waitParams.value = 123u;

    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, cmdList.appendHostFunctionCalledTimes);
    EXPECT_EQ(&MockCommandListExtSem<FamilyType::gfxCoreFamily>::semaphoreWaitHostFunction, cmdList.capturedHostFunction);

    ASSERT_NE(nullptr, cmdList.capturedUserData);
    auto hostFunctionData = static_cast<MockCommandListExtSem<FamilyType::gfxCoreFamily>::ExternalSemaphoreHostFunctionData *>(cmdList.capturedUserData);
    ASSERT_EQ(1u, hostFunctionData->operationData.semaphores.size());
    EXPECT_EQ(&semaphore, hostFunctionData->operationData.semaphores[0].first);
    EXPECT_EQ(1u, neoSemaphore->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore->acquireSignalFenceValueCalledTimes);
    EXPECT_EQ(123u, neoSemaphore->lastAcquireWaitFenceValueArg);
    EXPECT_EQ(456u, hostFunctionData->operationData.semaphores[0].second);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenAppendSignalExternalSemaphoresIsCalledOnRegularCmdListThenSignalHostFunctionIsAppended) {
    MockCommandListExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ExternalSemaphoreImp semaphore;
    auto neoSemaphore = new MockNeoExtSemaphore();
    neoSemaphore->acquireSignalFenceValueReturnValue = 654u;
    semaphore.neoExternalSemaphore.reset(neoSemaphore);
    ze_external_semaphore_ext_handle_t hSemaphore = semaphore.toHandle();
    ze_external_semaphore_signal_params_ext_t signalParams = {};
    signalParams.value = 321u;

    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, cmdList.appendHostFunctionCalledTimes);
    EXPECT_EQ(&MockCommandListExtSem<FamilyType::gfxCoreFamily>::semaphoreSignalHostFunction, cmdList.capturedHostFunction);

    ASSERT_NE(nullptr, cmdList.capturedUserData);
    auto hostFunctionData = static_cast<MockCommandListExtSem<FamilyType::gfxCoreFamily>::ExternalSemaphoreHostFunctionData *>(cmdList.capturedUserData);
    ASSERT_EQ(1u, hostFunctionData->operationData.semaphores.size());
    EXPECT_EQ(&semaphore, hostFunctionData->operationData.semaphores[0].first);
    EXPECT_EQ(1u, neoSemaphore->acquireSignalFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(321u, neoSemaphore->lastAcquireSignalFenceValueArg);
    EXPECT_EQ(654u, hostFunctionData->operationData.semaphores[0].second);
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
    auto neoSemaphore = new MockNeoExtSemaphore();
    neoSemaphore->acquireWaitFenceValueReturnValue = 77u;
    semaphore.neoExternalSemaphore.reset(neoSemaphore);
    ze_external_semaphore_ext_handle_t hSemaphore = semaphore.toHandle();
    ze_external_semaphore_wait_params_ext_t waitParams = {};
    waitParams.value = 55u;

    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, cmdList.appendHostFunctionCalledTimes);
    EXPECT_EQ(&MockCommandListExtSem<FamilyType::gfxCoreFamily>::semaphoreWaitHostFunction, cmdList.capturedHostFunction);

    ASSERT_NE(nullptr, cmdList.capturedUserData);
    auto hostFunctionData = static_cast<MockCommandListExtSem<FamilyType::gfxCoreFamily>::ExternalSemaphoreHostFunctionData *>(cmdList.capturedUserData);
    ASSERT_EQ(1u, hostFunctionData->operationData.semaphores.size());
    EXPECT_EQ(&semaphore, hostFunctionData->operationData.semaphores[0].first);
    EXPECT_EQ(1u, neoSemaphore->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore->acquireSignalFenceValueCalledTimes);
    EXPECT_EQ(55u, neoSemaphore->lastAcquireWaitFenceValueArg);
    EXPECT_EQ(77u, hostFunctionData->operationData.semaphores[0].second);
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
    auto neoSemaphore = new MockNeoExtSemaphore();
    neoSemaphore->acquireSignalFenceValueReturnValue = 88u;
    semaphore.neoExternalSemaphore.reset(neoSemaphore);
    ze_external_semaphore_ext_handle_t hSemaphore = semaphore.toHandle();
    ze_external_semaphore_signal_params_ext_t signalParams = {};
    signalParams.value = 66u;

    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, cmdList.appendHostFunctionCalledTimes);
    EXPECT_EQ(&MockCommandListExtSem<FamilyType::gfxCoreFamily>::semaphoreSignalHostFunction, cmdList.capturedHostFunction);

    ASSERT_NE(nullptr, cmdList.capturedUserData);
    auto hostFunctionData = static_cast<MockCommandListExtSem<FamilyType::gfxCoreFamily>::ExternalSemaphoreHostFunctionData *>(cmdList.capturedUserData);
    ASSERT_EQ(1u, hostFunctionData->operationData.semaphores.size());
    EXPECT_EQ(&semaphore, hostFunctionData->operationData.semaphores[0].first);
    EXPECT_EQ(1u, neoSemaphore->acquireSignalFenceValueCalledTimes);
    EXPECT_EQ(0u, neoSemaphore->acquireWaitFenceValueCalledTimes);
    EXPECT_EQ(66u, neoSemaphore->lastAcquireSignalFenceValueArg);
    EXPECT_EQ(88u, hostFunctionData->operationData.semaphores[0].second);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenSemaphoreWaitHostFunctionIsCalledOnRegularCmdListThenHostFunctionDataIsPreserved) {
    MockCommandListExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ExternalSemaphoreImp semaphore;
    auto neoSemaphore = new MockNeoExtSemaphore();
    semaphore.neoExternalSemaphore.reset(neoSemaphore);

    ExternalSemaphoreOperationData operationData;
    operationData.semaphores.push_back(std::pair(&semaphore, 7u));
    auto &hostFunctionData = cmdList.externalSemaphoreHostFunctionData.emplace_back(cmdList, std::move(operationData));
    hostFunctionData.self = std::prev(cmdList.externalSemaphoreHostFunctionData.end());

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily>::semaphoreWaitHostFunction(&hostFunctionData);
    EXPECT_EQ(cmdList.externalSemaphoreHostFunctionData.size(), 1u);
    EXPECT_EQ(neoSemaphore->enqueueWaitCalledTimes, 1u);
    EXPECT_EQ(neoSemaphore->lastWaitValue, 7u);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreWhenSemaphoreSignalHostFunctionIsCalledOnRegularCmdListThenHostFunctionDataIsPreserved) {
    MockCommandListExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ExternalSemaphoreImp semaphore;
    auto neoSemaphore = new MockNeoExtSemaphore();
    semaphore.neoExternalSemaphore.reset(neoSemaphore);

    ExternalSemaphoreOperationData operationData;
    operationData.semaphores.push_back(std::pair(&semaphore, 7u));
    auto &hostFunctionData = cmdList.externalSemaphoreHostFunctionData.emplace_back(cmdList, std::move(operationData));
    hostFunctionData.self = std::prev(cmdList.externalSemaphoreHostFunctionData.end());

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily>::semaphoreSignalHostFunction(&hostFunctionData);
    EXPECT_EQ(cmdList.externalSemaphoreHostFunctionData.size(), 1u);
    EXPECT_EQ(neoSemaphore->enqueueSignalCalledTimes, 1u);
    EXPECT_EQ(neoSemaphore->lastSignalValue, 7u);
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
    auto neoSemaphore = new MockNeoExtSemaphore();
    semaphore.neoExternalSemaphore.reset(neoSemaphore);

    ExternalSemaphoreOperationData operationData;
    operationData.semaphores.push_back(std::pair(&semaphore, 7u));
    auto &hostFunctionData = cmdList.externalSemaphoreHostFunctionData.emplace_back(cmdList, std::move(operationData));
    hostFunctionData.self = std::prev(cmdList.externalSemaphoreHostFunctionData.end());

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily>::semaphoreWaitHostFunction(&hostFunctionData);
    EXPECT_EQ(cmdList.externalSemaphoreHostFunctionData.size(), 0u);
    EXPECT_EQ(neoSemaphore->enqueueWaitCalledTimes, 1u);
    EXPECT_EQ(neoSemaphore->lastWaitValue, 7u);
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
    auto neoSemaphore = new MockNeoExtSemaphore();
    semaphore.neoExternalSemaphore.reset(neoSemaphore);

    ExternalSemaphoreOperationData operationData;
    operationData.semaphores.push_back(std::pair(&semaphore, 7u));
    auto &hostFunctionData = cmdList.externalSemaphoreHostFunctionData.emplace_back(cmdList, std::move(operationData));
    hostFunctionData.self = std::prev(cmdList.externalSemaphoreHostFunctionData.end());

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily>::semaphoreSignalHostFunction(&hostFunctionData);
    EXPECT_EQ(cmdList.externalSemaphoreHostFunctionData.size(), 0u);
    EXPECT_EQ(neoSemaphore->enqueueSignalCalledTimes, 1u);
    EXPECT_EQ(neoSemaphore->lastSignalValue, 7u);
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoresWhenAreImportedToDeviceIsCalledThenTrueIsReturnedOnlyIfAllUseDeviceOsInterface) {
    NEO::OSInterface otherDeviceOsInterface;
    ExternalSemaphoreImp semaphores[3];
    ze_external_semaphore_ext_handle_t hSemaphores[3];
    for (uint32_t i = 0; i < 3; i++) {
        semaphores[i].neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();
        semaphores[i].neoExternalSemaphore->osInterface = device->getOsInterface();
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
    semaphore0.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();
    semaphore1.neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();
    semaphore0.neoExternalSemaphore->osInterface = device->getOsInterface();
    semaphore1.neoExternalSemaphore->osInterface = &otherDeviceOsInterface;
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
