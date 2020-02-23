/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/program/printf_handler.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(PrintfHandlerTest, givenNotPreparedPrintfHandlerWhenGetSurfaceIsCalledThenResultIsNullptr) {
    MockClDevice *device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    MockContext context;
    SPatchAllocateStatelessPrintfSurface *pPrintfSurface = new SPatchAllocateStatelessPrintfSurface();
    pPrintfSurface->DataParamOffset = 0;
    pPrintfSurface->DataParamSize = 8;

    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface = pPrintfSurface;

    MockProgram *pProgram = new MockProgram(*device->getExecutionEnvironment(), &context, false, &device->getDevice());
    MockKernel *pKernel = new MockKernel(pProgram, *pKernelInfo, *device);

    MockMultiDispatchInfo multiDispatchInfo(pKernel);
    PrintfHandler *printfHandler = PrintfHandler::create(multiDispatchInfo, *device);

    EXPECT_EQ(nullptr, printfHandler->getSurface());

    delete printfHandler;
    delete pPrintfSurface;
    delete pKernel;

    delete pProgram;
    delete device;
}

TEST(PrintfHandlerTest, givenPreparedPrintfHandlerWhenGetSurfaceIsCalledThenResultIsNullptr) {
    MockClDevice *device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    MockContext context;
    SPatchAllocateStatelessPrintfSurface *pPrintfSurface = new SPatchAllocateStatelessPrintfSurface();
    pPrintfSurface->DataParamOffset = 0;
    pPrintfSurface->DataParamSize = 8;

    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface = pPrintfSurface;

    MockProgram *pProgram = new MockProgram(*device->getExecutionEnvironment(), &context, false, &device->getDevice());

    uint64_t crossThread[10];
    MockKernel *pKernel = new MockKernel(pProgram, *pKernelInfo, *device);
    pKernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    MockMultiDispatchInfo multiDispatchInfo(pKernel);
    PrintfHandler *printfHandler = PrintfHandler::create(multiDispatchInfo, *device);
    printfHandler->prepareDispatch(multiDispatchInfo);
    EXPECT_NE(nullptr, printfHandler->getSurface());

    delete printfHandler;
    delete pPrintfSurface;
    delete pKernel;

    delete pProgram;
    delete device;
}

TEST(PrintfHandlerTest, givenParentKernelWihoutPrintfAndBlockKernelWithPrintfWhenPrintfHandlerCreateCalledThenResaultIsAnObject) {

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    std::unique_ptr<MockParentKernel> parentKernelWithoutPrintf(MockParentKernel::create(context, false, false, false, false));

    MockMultiDispatchInfo multiDispatchInfo(parentKernelWithoutPrintf.get());

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));

    ASSERT_NE(nullptr, printfHandler.get());
}

TEST(PrintfHandlerTest, givenParentKernelAndBlockKernelWithoutPrintfWhenPrintfHandlerCreateCalledThenResaultIsNullptr) {

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    std::unique_ptr<MockParentKernel> blockKernelWithoutPrintf(MockParentKernel::create(context, false, false, false, false, false));

    MockMultiDispatchInfo multiDispatchInfo(blockKernelWithoutPrintf.get());

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));

    ASSERT_EQ(nullptr, printfHandler.get());
}
TEST(PrintfHandlerTest, givenParentKernelWithPrintfAndBlockKernelWithoutPrintfWhenPrintfHandlerCreateCalledThenResaultIsAnObject) {

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    std::unique_ptr<MockParentKernel> parentKernelWithPrintfBlockKernelWithoutPrintf(MockParentKernel::create(context, false, false, false, true, false));

    MockMultiDispatchInfo multiDispatchInfo(parentKernelWithPrintfBlockKernelWithoutPrintf.get());

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));

    ASSERT_NE(nullptr, printfHandler);
}

TEST(PrintfHandlerTest, givenMultiDispatchInfoWithMultipleKernelsWhenCreatingAndDispatchingPrintfHandlerThenPickMainKernel) {
    MockContext context;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto program = std::make_unique<MockProgram>(*device->getExecutionEnvironment(), &context, false, &device->getDevice());
    auto mainKernelInfo = std::make_unique<KernelInfo>();
    auto kernelInfo = std::make_unique<KernelInfo>();

    auto printfSurface = std::make_unique<SPatchAllocateStatelessPrintfSurface>();
    printfSurface->DataParamOffset = 0;
    printfSurface->DataParamSize = 8;

    mainKernelInfo->patchInfo.pAllocateStatelessPrintfSurface = printfSurface.get();

    uint64_t crossThread[8];
    auto mainKernel = std::make_unique<MockKernel>(program.get(), *mainKernelInfo, *device);
    auto kernel1 = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
    auto kernel2 = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
    mainKernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    DispatchInfo mainDispatchInfo(mainKernel.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
    DispatchInfo dispatchInfo1(kernel1.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
    DispatchInfo dispatchInfo2(kernel2.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});

    MultiDispatchInfo multiDispatchInfo(mainKernel.get());
    multiDispatchInfo.push(dispatchInfo1);
    multiDispatchInfo.push(mainDispatchInfo);
    multiDispatchInfo.push(dispatchInfo2);

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));
    ASSERT_NE(nullptr, printfHandler.get());

    printfHandler->prepareDispatch(multiDispatchInfo);
    EXPECT_NE(nullptr, printfHandler->getSurface());
}

TEST(PrintfHandlerTest, GivenEmptyMultiDispatchInfoWhenCreatingPrintfHandlerThenPrintfHandlerIsNotCreated) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals mockKernelWithInternals{device};
    MockMultiDispatchInfo multiDispatchInfo{mockKernelWithInternals.mockKernel};
    multiDispatchInfo.dispatchInfos.resize(0);
    EXPECT_EQ(nullptr, multiDispatchInfo.peekMainKernel());

    auto printfHandler = PrintfHandler::create(multiDispatchInfo, device);
    EXPECT_EQ(nullptr, printfHandler);
}

using PrintfHandlerMultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(PrintfHandlerMultiRootDeviceTests, printfSurfaceHasCorrectRootDeviceIndex) {
    auto printfSurface = std::make_unique<SPatchAllocateStatelessPrintfSurface>();
    printfSurface->DataParamOffset = 0;
    printfSurface->DataParamSize = 8;

    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->patchInfo.pAllocateStatelessPrintfSurface = printfSurface.get();

    auto program = std::make_unique<MockProgram>(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice());

    uint64_t crossThread[10];
    auto kernel = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
    kernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    MockMultiDispatchInfo multiDispatchInfo(kernel.get());
    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));
    printfHandler->prepareDispatch(multiDispatchInfo);
    auto surface = printfHandler->getSurface();

    ASSERT_NE(nullptr, surface);
    EXPECT_EQ(expectedRootDeviceIndex, surface->getRootDeviceIndex());
}
