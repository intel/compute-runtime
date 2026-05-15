/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/internal/l0_cmdlist.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

#include <cstdint>

using namespace NEO;

namespace L0 {

namespace ult {

struct VisitExtCounters {
    uint32_t concreteCalled = 0;
    uint32_t beforeCalled = 0;
    uint32_t afterCalled = 0;

    ze_event_handle_t injectedSignalEvent = nullptr;
    ze_event_handle_t observedSignalEvent = nullptr;
    uint32_t observedNumWaitEvents = 0;
    ze_event_handle_t injectedWaitEvents[2] = {nullptr, nullptr};
};

static ze_result_t VISITOR_CCONV barrierConcreteVisitor(ze_command_list_handle_t,
                                                        ze_event_handle_t,
                                                        uint32_t,
                                                        ze_event_handle_t *,
                                                        void *userData) {
    auto *state = static_cast<VisitExtCounters *>(userData);
    ++state->concreteCalled;
    return ZE_RESULT_SUCCESS;
}

static ze_result_t VISITOR_CCONV barrierConcreteVisitorFail(ze_command_list_handle_t,
                                                            ze_event_handle_t,
                                                            uint32_t,
                                                            ze_event_handle_t *,
                                                            void *) {
    return ZE_RESULT_ERROR_UNKNOWN;
}

static void VISITOR_CCONV beforeDefaultBarrierVisitor(const char *,
                                                      void *userData,
                                                      uint32_t *numWaitEvents,
                                                      ze_event_handle_t **phWaitEvents,
                                                      ze_event_handle_t *hSignalEvent) {
    auto *state = static_cast<VisitExtCounters *>(userData);
    ++state->beforeCalled;
    *numWaitEvents = 2;
    *phWaitEvents = state->injectedWaitEvents;
    *hSignalEvent = state->injectedSignalEvent;
}

static void VISITOR_CCONV afterDefaultBarrierVisitor(const char *,
                                                     void *userData,
                                                     uint32_t *numWaitEvents,
                                                     ze_event_handle_t **,
                                                     ze_event_handle_t *hSignalEvent) {
    auto *state = static_cast<VisitExtCounters *>(userData);
    ++state->afterCalled;
    state->observedNumWaitEvents = *numWaitEvents;
    state->observedSignalEvent = *hSignalEvent;
}

using CommandListExpTest = Test<DeviceFixture>;

HWTEST_F(CommandListExpTest, givenCmdListWithSimulatedCsrWhenVerifyMemoryCalledThenExpectMemoryIsCalled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    MockAubCsr<FamilyType> mockCommandStreamReceiver("", true, *neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.callBaseExpectMemory = false;
    Mock<CommandQueue> mockCommandQueue(device, &mockCommandStreamReceiver, &desc);

    auto oldCommandQueue = commandListImmediate.cmdQImmediate;
    commandListImmediate.cmdQImmediate = &mockCommandQueue;
    void *ptr = reinterpret_cast<void *>(0x1000);
    char data[10] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zexCommandListVerifyMemory(commandList->toHandle(), ptr, data, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_TRUE(mockCommandStreamReceiver.expectMemoryCalled);

    commandListImmediate.cmdQImmediate = oldCommandQueue;
}

HWTEST_F(CommandListExpTest, givenCmdListWithSimulatedCsrWhenVerifyMemoryCalledWithInvalidParamsThenErrorIsReturned) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    MockAubCsr<FamilyType> mockCommandStreamReceiver("", true, *neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.callBaseExpectMemory = false;
    Mock<CommandQueue> mockCommandQueue(device, &mockCommandStreamReceiver, &desc);

    auto oldCommandQueue = commandListImmediate.cmdQImmediate;
    commandListImmediate.cmdQImmediate = &mockCommandQueue;
    void *ptr = reinterpret_cast<void *>(0x1000);
    char data[10] = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, L0::zexCommandListVerifyMemory(nullptr, ptr, data, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, L0::zexCommandListVerifyMemory(commandList->toHandle(), nullptr, data, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, L0::zexCommandListVerifyMemory(commandList->toHandle(), ptr, nullptr, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, L0::zexCommandListVerifyMemory(commandList->toHandle(), ptr, data, 0, zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_FALSE(mockCommandStreamReceiver.expectMemoryCalled);

    commandListImmediate.cmdQImmediate = oldCommandQueue;
}

HWTEST_F(CommandListExpTest, givenRegularCmdListWithSimulatedCsrWhenVerifyMemoryCalledThenExpectMemoryIsCalled) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0, returnValue, false));
    void *ptr = reinterpret_cast<void *>(0x1000);
    char data[10] = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zexCommandListVerifyMemory(commandList->toHandle(), ptr, data, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
}

HWTEST_F(CommandListExpTest, givenVisitExtWhenNullParametersArePassedThenErrorIsReturned) {
    ze_visit_ext_desc_t desc = {};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, L0::zeCommandListVisitExt(nullptr, &desc));

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0, returnValue, false));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, L0::zeCommandListVisitExt(commandList->toHandle(), nullptr));
}

HWTEST_F(CommandListExpTest, givenVisitExtWhenCalledForBaseCommandListThenItForwardsToVisitImplementation) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0, returnValue, false));

    ze_visit_ext_desc_t desc = {};
    desc.stype = ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.defaultOp = ZE_VISIT_EXT_DEFAULT_OP_IGNORE;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListVisitExt(commandList->toHandle(), &desc));
}

HWTEST_F(CommandListExpTest, givenVisitExtWithInvalidPnextStypeThenInvalidEnumerationIsReturned) {
    Mock<CommandList> cmdList;
    cmdList.flatCapture = std::make_unique<RecordedApiCommands>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(cmdList.toHandle(), nullptr, 0, nullptr));

    ze_base_desc_t invalidDesc{};
    invalidDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;

    ze_visit_ext_desc_t desc{};
    desc.stype = ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &invalidDesc;
    desc.defaultOp = ZE_VISIT_EXT_DEFAULT_OP_IGNORE;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, L0::zeCommandListVisitExt(cmdList.toHandle(), &desc));
}

HWTEST_F(CommandListExpTest, givenVisitExtWithConcreteVisitorNullFunctionNameThenInvalidNullPointerIsReturned) {
    Mock<CommandList> cmdList;
    cmdList.flatCapture = std::make_unique<RecordedApiCommands>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(cmdList.toHandle(), nullptr, 0, nullptr));

    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = nullptr;
    concreteVisitor.callback = reinterpret_cast<void *>(barrierConcreteVisitor);

    ze_visit_ext_desc_t desc{};
    desc.stype = ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;
    desc.defaultOp = ZE_VISIT_EXT_DEFAULT_OP_IGNORE;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, L0::zeCommandListVisitExt(cmdList.toHandle(), &desc));
}

HWTEST_F(CommandListExpTest, givenVisitExtWithConcreteVisitorUnknownFunctionNameThenInvalidArgumentIsReturned) {
    Mock<CommandList> cmdList;
    cmdList.flatCapture = std::make_unique<RecordedApiCommands>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(cmdList.toHandle(), nullptr, 0, nullptr));

    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = "unknownApiName";
    concreteVisitor.callback = reinterpret_cast<void *>(barrierConcreteVisitor);

    ze_visit_ext_desc_t desc{};
    desc.stype = ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;
    desc.defaultOp = ZE_VISIT_EXT_DEFAULT_OP_IGNORE;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeCommandListVisitExt(cmdList.toHandle(), &desc));
}

HWTEST_F(CommandListExpTest, givenVisitExtWithConcreteVisitorWhenCallbackMatchesThenConcreteVisitorIsInvokedAndDefaultOpIsSkipped) {
    Mock<CommandList> srcCmdList;
    Mock<CommandList> reappendTargetCmdList;
    srcCmdList.flatCapture = std::make_unique<RecordedApiCommands>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(srcCmdList.toHandle(), nullptr, 0, nullptr));

    VisitExtCounters state{};

    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = "zeCommandListAppendBarrier";
    concreteVisitor.callback = reinterpret_cast<void *>(barrierConcreteVisitor);

    ze_visit_ext_desc_t desc{};
    desc.stype = ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;
    desc.userData = &state;
    desc.defaultOp = ZE_VISIT_EXT_DEFAULT_OP_REAPPEND;
    desc.hReappendTargetCmdList = reappendTargetCmdList.toHandle();
    desc.beforeDefaultOpClb = beforeDefaultBarrierVisitor;
    desc.afterDefaultOpClb = afterDefaultBarrierVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListVisitExt(srcCmdList.toHandle(), &desc));
    EXPECT_EQ(1u, state.concreteCalled);
    EXPECT_EQ(0u, state.beforeCalled);
    EXPECT_EQ(0u, state.afterCalled);
    EXPECT_EQ(0u, reappendTargetCmdList.appendBarrierCalled);
}

HWTEST_F(CommandListExpTest, givenVisitExtWithConcreteVisitorWhenCallbackFailsThenErrorIsPropagated) {
    Mock<CommandList> srcCmdList;
    srcCmdList.flatCapture = std::make_unique<RecordedApiCommands>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(srcCmdList.toHandle(), nullptr, 0, nullptr));

    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = "zeCommandListAppendBarrier";
    concreteVisitor.callback = reinterpret_cast<void *>(barrierConcreteVisitorFail);

    ze_visit_ext_desc_t desc{};
    desc.stype = ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;
    desc.defaultOp = ZE_VISIT_EXT_DEFAULT_OP_REAPPEND;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeCommandListVisitExt(srcCmdList.toHandle(), &desc));
}

HWTEST_F(CommandListExpTest, givenVisitExtIgnoreDefaultOpWithBeforeAfterCallbacksWhenNoConcreteVisitorThenCallbacksAreInvoked) {
    Mock<CommandList> srcCmdList;
    srcCmdList.flatCapture = std::make_unique<RecordedApiCommands>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(srcCmdList.toHandle(), nullptr, 0, nullptr));

    VisitExtCounters state{};
    state.injectedSignalEvent = reinterpret_cast<ze_event_handle_t>(static_cast<uintptr_t>(0x101));
    state.injectedWaitEvents[0] = reinterpret_cast<ze_event_handle_t>(static_cast<uintptr_t>(0x111));
    state.injectedWaitEvents[1] = reinterpret_cast<ze_event_handle_t>(static_cast<uintptr_t>(0x121));

    ze_visit_ext_desc_t desc{};
    desc.stype = ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.userData = &state;
    desc.defaultOp = ZE_VISIT_EXT_DEFAULT_OP_IGNORE;
    desc.beforeDefaultOpClb = beforeDefaultBarrierVisitor;
    desc.afterDefaultOpClb = afterDefaultBarrierVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListVisitExt(srcCmdList.toHandle(), &desc));
    EXPECT_EQ(0u, state.concreteCalled);
    EXPECT_EQ(1u, state.beforeCalled);
    EXPECT_EQ(1u, state.afterCalled);
    EXPECT_EQ(2u, state.observedNumWaitEvents);
    EXPECT_EQ(state.injectedSignalEvent, state.observedSignalEvent);
}

HWTEST_F(CommandListExpTest, givenVisitExtReappendDefaultOpWithBeforeAfterCallbacksWhenNoConcreteVisitorThenDefaultOpAndCallbacksAreInvoked) {
    Mock<CommandList> srcCmdList;
    Mock<CommandList> reappendTargetCmdList;
    Mock<Event> signalEvent;
    Mock<Event> waitEvent0;
    Mock<Event> waitEvent1;
    srcCmdList.flatCapture = std::make_unique<RecordedApiCommands>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(srcCmdList.toHandle(), nullptr, 0, nullptr));

    VisitExtCounters state{};
    state.injectedSignalEvent = signalEvent.toHandle();
    state.injectedWaitEvents[0] = waitEvent0.toHandle();
    state.injectedWaitEvents[1] = waitEvent1.toHandle();

    ze_visit_ext_desc_t desc{};
    desc.stype = ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.userData = &state;
    desc.hReappendTargetCmdList = reappendTargetCmdList.toHandle();
    desc.defaultOp = ZE_VISIT_EXT_DEFAULT_OP_REAPPEND;
    desc.beforeDefaultOpClb = beforeDefaultBarrierVisitor;
    desc.afterDefaultOpClb = afterDefaultBarrierVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListVisitExt(srcCmdList.toHandle(), &desc));
    EXPECT_EQ(0u, state.concreteCalled);
    EXPECT_EQ(1u, state.beforeCalled);
    EXPECT_EQ(1u, state.afterCalled);
    EXPECT_EQ(1u, reappendTargetCmdList.appendBarrierCalled);
    EXPECT_EQ(2u, state.observedNumWaitEvents);
    EXPECT_EQ(state.injectedSignalEvent, state.observedSignalEvent);
}

} // namespace ult
} // namespace L0
