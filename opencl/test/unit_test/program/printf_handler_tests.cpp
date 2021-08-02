/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/program/printf_handler.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

using namespace NEO;

using PrintfHandlerTests = ::testing::Test;

TEST_F(PrintfHandlerTests, givenNotPreparedPrintfHandlerWhenGetSurfaceIsCalledThenResultIsNullptr) {
    MockClDevice *device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    MockContext context;

    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->setPrintfSurface(sizeof(uintptr_t), 0);

    MockProgram *pProgram = new MockProgram(&context, false, toClDeviceVector(*device));
    MockKernel *pKernel = new MockKernel(pProgram, *pKernelInfo, *device);

    MockMultiDispatchInfo multiDispatchInfo(device, pKernel);
    PrintfHandler *printfHandler = PrintfHandler::create(multiDispatchInfo, *device);

    EXPECT_EQ(nullptr, printfHandler->getSurface());

    delete printfHandler;
    delete pKernel;

    delete pProgram;
    delete device;
}

TEST_F(PrintfHandlerTests, givenPreparedPrintfHandlerWithUndefinedSshOffsetWhenGetSurfaceIsCalledThenResultIsNullptr) {
    MockClDevice *device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    MockContext context;

    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->setPrintfSurface(sizeof(uintptr_t), 0);

    MockProgram *pProgram = new MockProgram(&context, false, toClDeviceVector(*device));

    uint64_t crossThread[10];
    MockKernel *pKernel = new MockKernel(pProgram, *pKernelInfo, *device);
    pKernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    MockMultiDispatchInfo multiDispatchInfo(device, pKernel);
    PrintfHandler *printfHandler = PrintfHandler::create(multiDispatchInfo, *device);
    printfHandler->prepareDispatch(multiDispatchInfo);
    EXPECT_NE(nullptr, printfHandler->getSurface());

    delete printfHandler;
    delete pKernel;

    delete pProgram;
    delete device;
}

HWTEST_F(PrintfHandlerTests, givenEnabledStatelessCompressionWhenPrintEnqueueOutputIsCalledThenBCSEngineIsUsedForAuxTranslation) {
    REQUIRE_BLITTER_OR_SKIP(defaultHwInfo.get());

    DebugManagerStateRestore restore;

    for (auto enable : {-1, 0, 1}) {
        DebugManager.flags.EnableStatelessCompression.set(enable);

        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        MockContext context;

        auto kernelInfo = std::make_unique<MockKernelInfo>();
        kernelInfo->setPrintfSurface(sizeof(uintptr_t), 0);

        auto program = std::make_unique<MockProgram>(&context, false, toClDeviceVector(*device));

        uint64_t crossThread[10];
        auto kernel = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
        kernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

        MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel.get());
        std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));
        printfHandler->prepareDispatch(multiDispatchInfo);
        EXPECT_NE(nullptr, printfHandler->getSurface());

        printfHandler->printEnqueueOutput();

        auto &bcsEngine = device->getEngine(EngineHelpers::getBcsEngineType(device->getHardwareInfo(), device->getSelectorCopyEngine()), EngineUsage::Regular);
        auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);

        if (enable > 0) {
            EXPECT_EQ(1u, bcsCsr->blitBufferCalled);
            EXPECT_EQ(AuxTranslationDirection::AuxToNonAux, bcsCsr->receivedBlitProperties[0].auxTranslationDirection);
        } else {
            EXPECT_EQ(0u, bcsCsr->blitBufferCalled);
        }
    }
}

HWTEST_F(PrintfHandlerTests, givenPrintfHandlerWhenEnqueueIsBlockedThenDontUsePrintfObjectAfterMove) {
    DebugManagerStateRestore restore;
    DebugManager.flags.MakeEachEnqueueBlocking.set(true);

    class MyMockCommandQueueHw : public CommandQueueHw<FamilyType> {
      public:
        using CommandQueueHw<FamilyType>::CommandQueueHw;
        using CommandQueueHw<FamilyType>::enqueueKernel;

        void waitUntilComplete(bool blockedQueue, PrintfHandler *printfHandler) override {
            waitCalled = true;
            printfHandlerUsedForWait = printfHandler;
        }

        bool waitCalled = false;
        PrintfHandler *printfHandlerUsedForWait = nullptr;
    };

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;

    MyMockCommandQueueHw cmdQ(&context, device.get(), nullptr, false);

    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->setPrintfSurface(sizeof(uintptr_t), 0);

    uint64_t crossThread[10];
    auto program = std::make_unique<MockProgram>(&context, false, toClDeviceVector(*device));
    auto kernel = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
    kernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);
    kernel->incRefInternal();

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    size_t gws[] = {1, 1, 1};
    cmdQ.enqueueKernel(kernel.get(), 1, nullptr, gws, nullptr, 1, waitlist, nullptr);

    EXPECT_TRUE(cmdQ.waitCalled);
    EXPECT_EQ(nullptr, cmdQ.printfHandlerUsedForWait);

    userEvent.setStatus(CL_COMPLETE);
    EXPECT_FALSE(cmdQ.isQueueBlocked());
}

TEST_F(PrintfHandlerTests, givenParentKernelWihoutPrintfAndBlockKernelWithPrintfWhenPrintfHandlerCreateCalledThenResaultIsAnObject) {

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    std::unique_ptr<MockParentKernel> parentKernelWithoutPrintf(MockParentKernel::create(context, false, false, false, false));

    MockMultiDispatchInfo multiDispatchInfo(device.get(), parentKernelWithoutPrintf.get());

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));

    ASSERT_NE(nullptr, printfHandler.get());
}

TEST_F(PrintfHandlerTests, givenParentKernelAndBlockKernelWithoutPrintfWhenPrintfHandlerCreateCalledThenResaultIsNullptr) {

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    std::unique_ptr<MockParentKernel> blockKernelWithoutPrintf(MockParentKernel::create(context, false, false, false, false, false));

    MockMultiDispatchInfo multiDispatchInfo(device.get(), blockKernelWithoutPrintf.get());

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));

    ASSERT_EQ(nullptr, printfHandler.get());
}
TEST_F(PrintfHandlerTests, givenParentKernelWithPrintfAndBlockKernelWithoutPrintfWhenPrintfHandlerCreateCalledThenResaultIsAnObject) {

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    std::unique_ptr<MockParentKernel> parentKernelWithPrintfBlockKernelWithoutPrintf(MockParentKernel::create(context, false, false, false, true, false));

    MockMultiDispatchInfo multiDispatchInfo(device.get(), parentKernelWithPrintfBlockKernelWithoutPrintf.get());

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));

    ASSERT_NE(nullptr, printfHandler);
}

TEST_F(PrintfHandlerTests, givenMultiDispatchInfoWithMultipleKernelsWhenCreatingAndDispatchingPrintfHandlerThenPickMainKernel) {
    MockContext context;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto program = std::make_unique<MockProgram>(&context, false, toClDeviceVector(*device));

    auto pMainKernelInfo = std::make_unique<MockKernelInfo>();
    pMainKernelInfo->setPrintfSurface(sizeof(uintptr_t), 0);

    auto pKernelInfo = std::make_unique<MockKernelInfo>();

    auto mainKernel = std::make_unique<MockKernel>(program.get(), *pMainKernelInfo, *device);
    auto kernel1 = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *device);
    auto kernel2 = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *device);

    uint64_t crossThread[8];
    mainKernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    DispatchInfo mainDispatchInfo(device.get(), mainKernel.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
    DispatchInfo dispatchInfo1(device.get(), kernel1.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
    DispatchInfo dispatchInfo2(device.get(), kernel2.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});

    MultiDispatchInfo multiDispatchInfo(mainKernel.get());
    multiDispatchInfo.push(dispatchInfo1);
    multiDispatchInfo.push(mainDispatchInfo);
    multiDispatchInfo.push(dispatchInfo2);

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));
    ASSERT_NE(nullptr, printfHandler.get());

    printfHandler->prepareDispatch(multiDispatchInfo);
    EXPECT_NE(nullptr, printfHandler->getSurface());
}

TEST_F(PrintfHandlerTests, GivenEmptyMultiDispatchInfoWhenCreatingPrintfHandlerThenPrintfHandlerIsNotCreated) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals mockKernelWithInternals{device};
    MockMultiDispatchInfo multiDispatchInfo{&device, mockKernelWithInternals.mockKernel};
    multiDispatchInfo.dispatchInfos.resize(0);
    EXPECT_EQ(nullptr, multiDispatchInfo.peekMainKernel());

    auto printfHandler = PrintfHandler::create(multiDispatchInfo, device);
    EXPECT_EQ(nullptr, printfHandler);
}

TEST_F(PrintfHandlerTests, GivenAllocationInLocalMemoryWhichRequiresBlitterWhenPreparingPrintfSurfaceDispatchThenBlitterIsUsed) {
    REQUIRE_BLITTER_OR_SKIP(defaultHwInfo.get());

    DebugManagerStateRestore restorer;

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    uint32_t blitsCounter = 0;
    uint32_t expectedBlitsCount = 0;
    auto mockBlitMemoryToAllocation = [&blitsCounter](const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                                      Vec3<size_t> size) -> BlitOperationResult {
        blitsCounter++;
        return BlitOperationResult::Success;
    };
    VariableBackup<BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup{
        &BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation};

    LocalMemoryAccessMode localMemoryAccessModes[] = {
        LocalMemoryAccessMode::Default,
        LocalMemoryAccessMode::CpuAccessAllowed,
        LocalMemoryAccessMode::CpuAccessDisallowed};

    for (auto localMemoryAccessMode : localMemoryAccessModes) {
        DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(localMemoryAccessMode));
        for (auto isLocalMemorySupported : ::testing::Bool()) {
            DebugManager.flags.EnableLocalMemory.set(isLocalMemorySupported);
            auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
            MockContext context{pClDevice.get()};

            auto pKernelInfo = std::make_unique<MockKernelInfo>();
            pKernelInfo->setPrintfSurface(sizeof(uintptr_t), 0);

            auto program = std::make_unique<MockProgram>(&context, false, toClDeviceVector(*pClDevice));
            uint64_t crossThread[10];
            auto kernel = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *pClDevice);
            kernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

            MockMultiDispatchInfo multiDispatchInfo(pClDevice.get(), kernel.get());
            std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *pClDevice));
            printfHandler->prepareDispatch(multiDispatchInfo);

            if (printfHandler->getSurface()->isAllocatedInLocalMemoryPool() &&
                (localMemoryAccessMode == LocalMemoryAccessMode::CpuAccessDisallowed)) {
                expectedBlitsCount++;
            }
            EXPECT_EQ(expectedBlitsCount, blitsCounter);
        }
    }
}

using PrintfHandlerMultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(PrintfHandlerMultiRootDeviceTests, GivenPrintfSurfaceThenItHasCorrectRootDeviceIndex) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->setPrintfSurface(sizeof(uintptr_t), 0);

    auto program = std::make_unique<MockProgram>(context.get(), false, toClDeviceVector(*device1));

    auto kernel = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *device1);
    uint64_t crossThread[10];
    kernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    MockMultiDispatchInfo multiDispatchInfo(device1, kernel.get());
    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device1));
    printfHandler->prepareDispatch(multiDispatchInfo);
    auto surface = printfHandler->getSurface();

    ASSERT_NE(nullptr, surface);
    EXPECT_EQ(expectedRootDeviceIndex, surface->getRootDeviceIndex());
}
