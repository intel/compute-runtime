/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/program/printf_handler.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_program.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(PrintfHandlerTest, givenNotPreparedPrintfHandlerWhenGetSurfaceIsCalledThenResultIsNullptr) {
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    MockContext context;
    SPatchAllocateStatelessPrintfSurface *pPrintfSurface = new SPatchAllocateStatelessPrintfSurface();
    pPrintfSurface->DataParamOffset = 0;
    pPrintfSurface->DataParamSize = 8;

    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface = pPrintfSurface;

    MockProgram *pProgram = new MockProgram(*device->getExecutionEnvironment(), &context, false);
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
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    MockContext context;
    SPatchAllocateStatelessPrintfSurface *pPrintfSurface = new SPatchAllocateStatelessPrintfSurface();
    pPrintfSurface->DataParamOffset = 0;
    pPrintfSurface->DataParamSize = 8;

    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface = pPrintfSurface;

    MockProgram *pProgram = new MockProgram(*device->getExecutionEnvironment(), &context, false);

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

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    std::unique_ptr<MockParentKernel> parentKernelWithoutPrintf(MockParentKernel::create(context, false, false, false, false));

    MockMultiDispatchInfo multiDispatchInfo(parentKernelWithoutPrintf.get());

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));

    ASSERT_NE(nullptr, printfHandler.get());
}

TEST(PrintfHandlerTest, givenParentKernelAndBlockKernelWithoutPrintfWhenPrintfHandlerCreateCalledThenResaultIsNullptr) {

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    std::unique_ptr<MockParentKernel> blockKernelWithoutPrintf(MockParentKernel::create(context, false, false, false, false, false));

    MockMultiDispatchInfo multiDispatchInfo(blockKernelWithoutPrintf.get());

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));

    ASSERT_EQ(nullptr, printfHandler.get());
}
TEST(PrintfHandlerTest, givenParentKernelWithPrintfAndBlockKernelWithoutPrintfWhenPrintfHandlerCreateCalledThenResaultIsAnObject) {

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(device.get());
    std::unique_ptr<MockParentKernel> parentKernelWithPrintfBlockKernelWithoutPrintf(MockParentKernel::create(context, false, false, false, true, false));

    MockMultiDispatchInfo multiDispatchInfo(parentKernelWithPrintfBlockKernelWithoutPrintf.get());

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device));

    ASSERT_NE(nullptr, printfHandler);
}

TEST(PrintfHandlerTest, givenMultiDispatchInfoWithMultipleKernelsWhenCreatingAndDispatchingPrintfHandlerThenPickMainKernel) {
    MockContext context;
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto program = std::make_unique<MockProgram>(*device->getExecutionEnvironment(), &context, false);
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
