/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/aub_subcapture.h"
#include "runtime/helpers/dispatch_info.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_subcapture_manager.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "test.h"

using namespace OCLRT;

struct AubSubCaptureTest : public DeviceFixture,
                           public ::testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();
        kernelInfo.name = "kernel_name";
        dbgRestore = new DebugManagerStateRestore();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
        delete dbgRestore;
    }

    MockProgram program;
    KernelInfo kernelInfo;
    DebugManagerStateRestore *dbgRestore;
};

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureToggleRegKeyIsUnspecifiedThenSubCaptureIsToggledOffByDefault) {
    struct AubSubCaptureManagerWithToggleActiveMock : public AubSubCaptureManager {
        using AubSubCaptureManager::isSubCaptureToggleActive;
    } aubSubCaptureManagerWithToggleActiveMock;

    EXPECT_FALSE(aubSubCaptureManagerWithToggleActiveMock.isSubCaptureToggleActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenItIsCreatedThenItIsInitializedWithDefaults) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    EXPECT_EQ(AubSubCaptureManager::SubCaptureMode::Off, aubSubCaptureManager.subCaptureMode);
    EXPECT_STREQ("", aubSubCaptureManager.subCaptureFilter.dumpKernelName.c_str());
    EXPECT_EQ(0u, aubSubCaptureManager.subCaptureFilter.dumpKernelStartIdx);
    EXPECT_EQ(static_cast<uint32_t>(-1), aubSubCaptureManager.subCaptureFilter.dumpKernelEndIdx);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureMode());
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
    EXPECT_FALSE(aubSubCaptureManager.wasSubCaptureActive());
    EXPECT_EQ(0u, aubSubCaptureManager.getKernelCurrentIndex());
    EXPECT_NE(nullptr, aubSubCaptureManager.getSettingsReader());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenActivateSubCaptureIsCalledWithEmptyDispatchInfoThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    MultiDispatchInfo dispatchInfo;
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenActivateSubCaptureIsCalledButSubCaptureModeIsOffThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Off;
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenActivateSubCaptureIsCalledAndSubCaptureIsToggledOnThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenActivateSubCaptureIsCalledButSubCaptureIsToggledOffThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(false);
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterIsDefaultThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithValidKernelStartIndexIsSpecifiedThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelStartIdx = 0;
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelStartIndexIsSpecifiedThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelStartIdx = 1;
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelEndIndexIsSpecifiedThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelEndIdx = 0;
    aubSubCaptureManager.setKernelCurrentIndex(1);
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithValidKernelNameIsSpecifiedThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelName = "kernel_name";
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelNameIsSpecifiedThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelName = "invalid_kernel_name";
    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenDeactivateSubCaptureIsCalledThenSubCaptureActiveStatesAreCleared) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.setSubCaptureWasActive(true);

    aubSubCaptureManager.deactivateSubCapture();
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
    EXPECT_FALSE(aubSubCaptureManager.wasSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureKeepsInactiveThenMakeEachEnqueueBlocking) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.setSubCaptureIsActive(false);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(false);

    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_TRUE(DebugManager.flags.MakeEachEnqueueBlocking.get());
    EXPECT_FALSE(DebugManager.flags.ForceCsrFlushing.get());
    EXPECT_FALSE(DebugManager.flags.ForceCsrReprogramming.get());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureGetsActiveThenDontMakeEachEnqueueBlockingAndForceCsrReprogramming) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.setSubCaptureIsActive(false);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);

    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_FALSE(DebugManager.flags.ForceCsrFlushing.get());
    EXPECT_TRUE(DebugManager.flags.ForceCsrReprogramming.get());
    EXPECT_FALSE(DebugManager.flags.MakeEachEnqueueBlocking.get());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureKeepsActiveThenDontMakeEachEnqueueBlocking) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);

    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_FALSE(DebugManager.flags.ForceCsrFlushing.get());
    EXPECT_FALSE(DebugManager.flags.ForceCsrReprogramming.get());
    EXPECT_FALSE(DebugManager.flags.MakeEachEnqueueBlocking.get());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureGetsInactiveThenMakeEachEnqueueBlockingAndForceCsrFlushing) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    DispatchInfo dispatchInfo;
    MockKernel kernel(&program, kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(false);

    aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_TRUE(DebugManager.flags.ForceCsrFlushing.get());
    EXPECT_FALSE(DebugManager.flags.ForceCsrReprogramming.get());
    EXPECT_TRUE(DebugManager.flags.MakeEachEnqueueBlocking.get());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureActiveStatesAreDeterminedThenIsSubCaptureFunctionReturnCorrectValues) {
    AubSubCaptureManagerMock aubSubCaptureManager;

    aubSubCaptureManager.setSubCaptureWasActive(false);
    aubSubCaptureManager.setSubCaptureIsActive(false);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureEnabled());

    aubSubCaptureManager.setSubCaptureWasActive(false);
    aubSubCaptureManager.setSubCaptureIsActive(true);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureEnabled());

    aubSubCaptureManager.setSubCaptureWasActive(true);
    aubSubCaptureManager.setSubCaptureIsActive(false);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureEnabled());

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.setSubCaptureWasActive(true);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureEnabled());
}
