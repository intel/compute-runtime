/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_assert_handler.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/event_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_fence.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include <chrono>

namespace L0 {
namespace ult {

using CommandListImmediateWithAssert = Test<DeviceFixture>;

TEST(KernelAssert, GivenKernelWithAssertWhenDestroyedThenAssertIsChecked) {
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(neoDevice);

    auto assertHandler = new MockAssertHandler(neoDevice);
    neoDevice->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    Mock<Module> module(&l0Device, nullptr, ModuleType::user);

    {
        Mock<KernelImp> kernel;
        kernel.setModule(&module);

        kernel.descriptor.kernelAttributes.flags.usesAssert = true;
    }

    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
}

TEST(KernelAssert, GivenKernelWithAssertWhenNoModuleAssignedAndKernelDestroyedThenAssertIsNotChecked) {
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(neoDevice);

    auto assertHandler = new MockAssertHandler(neoDevice);
    neoDevice->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    {
        Mock<KernelImp> kernel;

        kernel.descriptor.kernelAttributes.flags.usesAssert = true;
    }

    EXPECT_EQ(0u, assertHandler->printAssertAndAbortCalled);
}

TEST(KernelAssert, GivenKernelWithAssertWhenNoAssertHandlerOnDestroyThenDestructorDoesNotCrash) {
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(neoDevice);

    Mock<Module> module(&l0Device, nullptr, ModuleType::user);

    {
        Mock<KernelImp> kernel;
        kernel.setModule(&module);

        kernel.descriptor.kernelAttributes.flags.usesAssert = true;
    }
}

TEST(KernelAssert, GivenKernelWithAssertWhenSettingAssertBufferThenAssertBufferIsAddedToResidencyAndCrossThreadDataPatched) {
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(neoDevice);

    auto assertHandler = new MockAssertHandler(neoDevice);
    neoDevice->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    Mock<Module> module(&l0Device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;
    kernel.descriptor.payloadMappings.implicitArgs.assertBufferAddress.stateless = 0;
    kernel.descriptor.payloadMappings.implicitArgs.assertBufferAddress.pointerSize = sizeof(uintptr_t);
    kernel.privateState.crossThreadData.resize(16, 0x0);

    kernel.setAssertBuffer();

    auto assertBufferAddress = assertHandler->getAssertBuffer()->getGpuAddressToPatch();

    EXPECT_TRUE(memcmp(kernel.getCrossThreadDataSpan().begin(), &assertBufferAddress, sizeof(assertBufferAddress)) == 0);
    EXPECT_TRUE(std::find(kernel.getInternalResidencyContainer().begin(), kernel.getInternalResidencyContainer().end(), assertHandler->getAssertBuffer()) != kernel.getInternalResidencyContainer().end());
}

TEST(KernelAssert, GivenKernelWithAssertAndImplicitArgsWhenInitializingKernelThenImplicitArgsAssertBufferPtrIsSet) {
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(neoDevice);

    auto assertHandler = new MockAssertHandler(neoDevice);
    neoDevice->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    MockModule module(&l0Device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);

    kernel.descriptor.kernelMetadata.kernelName = "test";
    kernel.descriptor.kernelAttributes.flags.usesAssert = true;
    kernel.descriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    kernel.descriptor.payloadMappings.implicitArgs.assertBufferAddress.stateless = 0;
    kernel.descriptor.payloadMappings.implicitArgs.assertBufferAddress.pointerSize = sizeof(uintptr_t);
    kernel.privateState.crossThreadData.resize(16, 0x0);

    module.data = &kernel.immutableData;
    char heap[8];
    kernel.info.heapInfo.pKernelHeap = heap;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "test";

    auto result = kernel.initialize(&kernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto assertBufferAddress = assertHandler->getAssertBuffer()->getGpuAddressToPatch();
    auto implicitArgs = kernel.getImplicitArgs();
    ASSERT_NE(nullptr, implicitArgs);
    if (implicitArgs->v0.header.structVersion == 0) {
        EXPECT_EQ(assertBufferAddress, implicitArgs->v0.assertBufferPtr);
    } else if (implicitArgs->v1.header.structVersion == 1) {
        EXPECT_EQ(assertBufferAddress, implicitArgs->v1.assertBufferPtr);
    } else if (implicitArgs->v2.header.structVersion == 2) {
        EXPECT_EQ(assertBufferAddress, implicitArgs->v2.assertBufferPtr);
    }
}

TEST(KernelAssert, GivenNoAssertHandlerWhenKernelWithAssertSetsAssertBufferThenAssertHandlerIsCreated) {
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(neoDevice);

    Mock<Module> module(&l0Device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;
    kernel.descriptor.payloadMappings.implicitArgs.assertBufferAddress.stateless = 0;
    kernel.descriptor.payloadMappings.implicitArgs.assertBufferAddress.pointerSize = sizeof(uintptr_t);
    kernel.privateState.crossThreadData.resize(16, 0x0);

    kernel.setAssertBuffer();
    EXPECT_NE(nullptr, neoDevice->getRootDeviceEnvironmentRef().assertHandler.get());
}

TEST(CommandListAssertTest, GivenCmdListWhenKernelWithAssertAppendedThenHasKernelWithAssertIsSetTrue) {
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(neoDevice);
    ze_result_t returnValue;

    Mock<Module> module(&l0Device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(NEO::defaultHwInfo->platform.eProductFamily, &l0Device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(commandList->hasKernelWithAssert());
}

TEST(CommandListAssertTest, GivenCmdListWithAppendedAssertKernelWhenResetThenKernelWithAssertAppendedIsFalse) {
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(neoDevice);
    ze_result_t returnValue;

    std::unique_ptr<ult::WhiteBox<L0::CommandListImp>> commandList(ult::CommandList::whiteboxCast(CommandList::create(NEO::defaultHwInfo->platform.eProductFamily,
                                                                                                                      &l0Device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));

    commandList->kernelWithAssertAppended = true;
    EXPECT_TRUE(commandList->hasKernelWithAssert());

    commandList->reset();
    EXPECT_FALSE(commandList->kernelWithAssertAppended);
    EXPECT_FALSE(commandList->hasKernelWithAssert());
}

TEST_F(CommandListImmediateWithAssert, GivenImmediateCmdListWithSyncModeWhenKernelWithAssertAppendedThenHasKernelWithAssertIsSetFalseAfterFlush) {
    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    ze_result_t result;
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = 0;
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(NEO::defaultHwInfo->platform.eProductFamily, device, &desc, false,
                                                                              NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t groupCount{1, 1, 1};

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
    EXPECT_FALSE(commandList->hasKernelWithAssert());
}

TEST_F(CommandListImmediateWithAssert, GivenImmediateCmdListWithASynchronousModeWhenKernelWithAssertAppendedThenHasKernelWithAssertIsSetFalseAfterSynchronize) {
    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    ze_result_t result;
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = 0;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(NEO::defaultHwInfo->platform.eProductFamily, device, &desc, false,
                                                                              NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t groupCount{1, 1, 1};

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(commandList->hasKernelWithAssert());

    result = commandList->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
    EXPECT_FALSE(commandList->hasKernelWithAssert());
}

TEST_F(CommandListImmediateWithAssert, GivenImmediateCmdListWhenKernelWithAssertAppendedInAsyncModeThenHasKernelWithAssertIsSetFalseWithNextAppendCall) {
    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    ze_result_t result;
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = 0;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(NEO::defaultHwInfo->platform.eProductFamily, device, &desc, false,
                                                                              NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t groupCount{1, 1, 1};

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(commandList->hasKernelWithAssert());

    kernel.descriptor.kernelAttributes.flags.usesAssert = false;
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(commandList->hasKernelWithAssert());
}

HWTEST_F(CommandListImmediateWithAssert, GivenImmediateCmdListAndKernelWithAssertAppendedWhenLaunchKernelIsAppendedWithPreviousTaskNotCompletedInAsyncModeThenHasKernelWithAssertIsSetTrue) {
    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    ze_result_t result;
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = 0;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.callBaseExecute = true;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.isSyncModeQueue = false;
    auto commandQueue = CommandQueue::create(productFamily, device, &csr, &desc, cmdList.isCopyOnly(false), false, false, result);
    cmdList.cmdQImmediate = commandQueue;

    result = cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.getCmdContainer().setImmediateCmdListCsr(&csr);

    ze_group_count_t groupCount{1, 1, 1};

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    result = cmdList.appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(cmdList.hasKernelWithAssert());

    kernel.descriptor.kernelAttributes.flags.usesAssert = false;
    csr.testTaskCountReadyReturnValue = false;
    result = cmdList.appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(cmdList.hasKernelWithAssert());
}

HWTEST_F(CommandListImmediateWithAssert, GivenImmediateCmdListWhenCheckingAssertThenPrintMessageAndAbortOnAssertHandlerIsCalled) {
    ze_result_t result;
    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = 0;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(NEO::defaultHwInfo->platform.eProductFamily, device, &desc, false,
                                                                              NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);
    static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> *>(commandList.get())->kernelWithAssertAppended = true;
    static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> *>(commandList.get())->checkAssert();

    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
}

HWTEST_F(CommandListImmediateWithAssert, GivenImmediateCmdListAndNoAssertHandlerWhenCheckingAssertThenUnrecoverableIsCalled) {
    ze_result_t result;
    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = 0;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(NEO::defaultHwInfo->platform.eProductFamily, device, &desc, false,
                                                                              NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> *>(commandList.get())->kernelWithAssertAppended = true;
    EXPECT_THROW(static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> *>(commandList.get())->checkAssert(), std::exception);
}

HWTEST_F(CommandListImmediateWithAssert, givenKernelWithAssertWhenAppendedToAsynchronousImmCommandListThenAssertIsNotChecked) {
    ze_result_t result;

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = 0;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.callBaseExecute = true;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.isSyncModeQueue = false;
    auto commandQueue = CommandQueue::create(productFamily, device, &csr, &desc, cmdList.isCopyOnly(false), false, false, result);
    cmdList.cmdQImmediate = commandQueue;

    result = cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.getCmdContainer().setImmediateCmdListCsr(&csr);

    ze_group_count_t groupCount{1, 1, 1};
    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    result = cmdList.appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, cmdList.checkAssertCalled);
}

HWTEST_F(CommandListImmediateWithAssert, givenKernelWithAssertWhenAppendedToSynchronousImmCommandListThenAssertIsChecked) {
    ze_result_t result;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.callBaseExecute = true;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.isSyncModeQueue = true;

    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = 0;
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto commandQueue = CommandQueue::create(productFamily, device, &csr, &desc, cmdList.isCopyOnly(false), false, false, result);
    cmdList.cmdQImmediate = commandQueue;

    result = cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.getCmdContainer().setImmediateCmdListCsr(&csr);

    ze_group_count_t groupCount{1, 1, 1};
    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    result = cmdList.appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, cmdList.checkAssertCalled);
}

HWTEST_F(CommandListImmediateWithAssert, givenKernelWithAssertWhenAppendToSynchronousImmCommandListHangsThenAssertIsChecked) {
    ze_result_t result;

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    csr.callBaseWaitForCompletionWithTimeout = false;
    csr.returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.callBaseExecute = true;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.isSyncModeQueue = true;

    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = 0;
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto commandQueue = CommandQueue::create(productFamily, device, &csr, &desc, cmdList.isCopyOnly(false), false, false, result);
    cmdList.cmdQImmediate = commandQueue;

    result = cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.getCmdContainer().setImmediateCmdListCsr(&csr);

    ze_group_count_t groupCount{1, 1, 1};
    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    result = cmdList.appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);

    EXPECT_EQ(1u, cmdList.checkAssertCalled);
}

using CommandQueueWithAssert = Test<DeviceFixture>;

TEST_F(CommandQueueWithAssert, GivenCmdListWithAssertWhenExecutingThenCommandQueuesPropertyIsSet) {
    ze_command_queue_desc_t desc = {};

    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(NEO::defaultHwInfo->platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->close();

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_TRUE(commandQueue->cmdListWithAssertExecuted);

    commandQueue->destroy();
}

TEST_F(CommandQueueWithAssert, GivenAssertKernelExecutedAndNoAssertHandlerWhenCheckingAssertThenUnrecoverableIsCalled) {
    ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    commandQueue->cmdListWithAssertExecuted = true;

    EXPECT_THROW(commandQueue->checkAssert(), std::exception);
    EXPECT_FALSE(commandQueue->cmdListWithAssertExecuted);

    commandQueue->destroy();
}

TEST_F(CommandQueueWithAssert, GivenCmdListWithAssertExecutedWhenSynchronizeByPollingTaskCountCalledThenAssertIsChecked) {
    ze_command_queue_desc_t desc = {};

    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    commandQueue->cmdListWithAssertExecuted = true;

    returnValue = commandQueue->synchronizeByPollingForTaskCount(0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
    EXPECT_FALSE(commandQueue->cmdListWithAssertExecuted);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueWithAssert, GivenCmdListWithAssertExecutedAndDetectedHangWhenSynchronizingByPollingThenAssertIsChecked) {
    const ze_command_queue_desc_t desc{};

    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    Mock<KernelImp> kernel1;
    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.callBaseWaitForCompletionWithTimeout = false;
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    csr.returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;

    commandQueue->cmdListWithAssertExecuted = true;

    returnValue = commandQueue->synchronizeByPollingForTaskCount(0u);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, returnValue);

    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
    EXPECT_FALSE(commandQueue->cmdListWithAssertExecuted);

    commandQueue->destroy();
}

TEST_F(CommandQueueWithAssert, GivenAssertKernelExecutedWhenSynchronizingFenceThenAssertIsChecked) {
    ze_command_queue_desc_t desc = {};

    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    commandQueue->cmdListWithAssertExecuted = true;

    ze_fence_desc_t fenceDesc = {ZE_STRUCTURE_TYPE_FENCE_DESC,
                                 nullptr,
                                 0};
    auto fence = whiteboxCast(Fence::create(commandQueue, &fenceDesc));
    ASSERT_NE(fence, nullptr);
    fence->taskCount = 0;
    ze_result_t result = fence->hostSynchronize(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    delete fence;

    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
    EXPECT_FALSE(commandQueue->cmdListWithAssertExecuted);
    commandQueue->destroy();
}

TEST_F(CommandQueueWithAssert, GivenRegularCmdListWithAssertWhenExecutingAndSynchronizeOnImmediateThenPropertyIsSetAndAssertChecked) {
    ze_command_queue_desc_t desc = {};

    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandListImmediate(CommandList::whiteboxCast(CommandList::createImmediate(NEO::defaultHwInfo->platform.eProductFamily,
                                                                                                                      device,
                                                                                                                      &desc,
                                                                                                                      false,
                                                                                                                      NEO::EngineGroupType::compute,
                                                                                                                      returnValue)));
    ASSERT_NE(nullptr, commandListImmediate);
    auto whiteBoxImmediate = whiteboxCast(commandListImmediate.get());

    auto whiteBoxQueue = whiteboxCast(whiteBoxImmediate->cmdQImmediate);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(NEO::defaultHwInfo->platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->close();

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    returnValue = commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(whiteBoxQueue->cmdListWithAssertExecuted);

    returnValue = commandListImmediate->hostSynchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
}

using EventAssertTest = Test<EventFixture<1, 0>>;

TEST_F(EventAssertTest, GivenGpuHangWhenHostSynchronizeIsCalledThenAssertIsChecked) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = true;

    event->csrs[0] = csr.get();
    event->gpuHangCheckPeriod = std::chrono::microseconds::zero();
    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    neoDevice->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    constexpr uint64_t timeout = std::numeric_limits<std::uint64_t>::max();
    auto result = event->hostSynchronize(timeout);

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
}

TEST_F(EventAssertTest, GivenNoGpuHangAndOneNanosecondTimeoutWhenHostSynchronizeIsCalledThenAssertIsChecked) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->isGpuHangDetectedReturnValue = false;

    event->csrs[0] = csr.get();
    event->gpuHangCheckPeriod = std::chrono::microseconds::zero();
    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    neoDevice->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    constexpr uint64_t timeoutNanoseconds = 1;
    auto result = event->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
}

TEST_F(EventAssertTest, GivenEventSignalledWhenHostSynchronizeIsCalledThenAssertIsChecked) {
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    uint32_t *hostAddr = static_cast<uint32_t *>(event->getHostAddress());
    *hostAddr = Event::STATE_SIGNALED;

    event->setUsingContextEndOffset(false);
    event->csrs[0] = csr.get();

    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    neoDevice->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

    constexpr uint64_t timeoutNanoseconds = 1;
    auto result = event->hostSynchronize(timeoutNanoseconds);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, assertHandler->printAssertAndAbortCalled);
}

} // namespace ult
} // namespace L0
