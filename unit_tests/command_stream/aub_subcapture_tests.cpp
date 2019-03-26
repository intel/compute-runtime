/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/aub_subcapture.h"
#include "runtime/helpers/dispatch_info.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_subcapture_manager.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

using namespace NEO;

struct AubSubCaptureTest : public DeviceFixture,
                           public ::testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();
        program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());
        kernelInfo.name = "kernel_name";
        dbgRestore = new DebugManagerStateRestore();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
        delete dbgRestore;
    }

    std::unique_ptr<MockProgram> program;
    KernelInfo kernelInfo;
    DebugManagerStateRestore *dbgRestore;
};

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureToggleCaptureOnOffIsUnspecifiedThenSubCaptureIsToggledOffByDefault) {
    struct AubSubCaptureManagerWithToggleActiveMock : public AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        using AubSubCaptureManager::isSubCaptureToggleActive;
    } aubSubCaptureManagerWithToggleActiveMock("");

    EXPECT_FALSE(aubSubCaptureManagerWithToggleActiveMock.isSubCaptureToggleActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureToggleFileNameIsUnspecifiedThenEmptyExternalFileNameIsReturnedByDefault) {
    struct AubSubCaptureManagerWithToggleActiveMock : public AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        using AubSubCaptureManager::getExternalFileName;
    } aubSubCaptureManagerWithToggleActiveMock("");

    EXPECT_STREQ("", aubSubCaptureManagerWithToggleActiveMock.getExternalFileName().c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenItIsCreatedThenItIsInitializedWithDefaults) {
    std::string initialFileName = "initial_file_name.aub";
    AubSubCaptureManagerMock aubSubCaptureManager(initialFileName);

    EXPECT_EQ(AubSubCaptureManager::SubCaptureMode::Off, aubSubCaptureManager.subCaptureMode);
    EXPECT_STREQ("", aubSubCaptureManager.subCaptureFilter.dumpKernelName.c_str());
    EXPECT_EQ(0u, aubSubCaptureManager.subCaptureFilter.dumpKernelStartIdx);
    EXPECT_EQ(static_cast<uint32_t>(-1), aubSubCaptureManager.subCaptureFilter.dumpKernelEndIdx);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureMode());
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
    EXPECT_FALSE(aubSubCaptureManager.wasSubCaptureActive());
    EXPECT_EQ(0u, aubSubCaptureManager.getKernelCurrentIndex());
    EXPECT_TRUE(aubSubCaptureManager.getUseExternalFileName());
    EXPECT_STREQ(initialFileName.c_str(), aubSubCaptureManager.getInitialFileName().c_str());
    EXPECT_STREQ("", aubSubCaptureManager.getCurrentFileName().c_str());
    EXPECT_NE(nullptr, aubSubCaptureManager.getSettingsReader());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenActivateSubCaptureIsCalledWithEmptyDispatchInfoThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    MultiDispatchInfo dispatchInfo;
    bool active = aubSubCaptureManager.activateSubCapture(dispatchInfo);
    EXPECT_FALSE(active);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenActivateSubCaptureIsCalledWithNonEmptyDispatchInfoThenKernelCurrentIndexIsIncremented) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    uint32_t kernelCurrentIndex = aubSubCaptureManager.getKernelCurrentIndex();
    ASSERT_EQ(0u, kernelCurrentIndex);

    aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_EQ(kernelCurrentIndex + 1, aubSubCaptureManager.getKernelCurrentIndex());

    aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_EQ(kernelCurrentIndex + 2, aubSubCaptureManager.getKernelCurrentIndex());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenActivateSubCaptureIsCalledButSubCaptureModeIsOffThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Off;
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(active);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenActivateSubCaptureIsCalledAndSubCaptureIsToggledOnThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(active);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenActivateSubCaptureIsCalledButSubCaptureIsToggledOffThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(false);
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(active);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterIsDefaultThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(active);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithValidKernelStartIndexIsSpecifiedThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelStartIdx = 0;
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(active);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelStartIndexIsSpecifiedThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelStartIdx = 1;
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(active);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelEndIndexIsSpecifiedThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelEndIdx = 0;
    aubSubCaptureManager.setKernelCurrentIndex(1);
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(active);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithValidKernelNameIsSpecifiedThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelName = "kernel_name";
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(active);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelNameIsSpecifiedThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelName = "invalid_kernel_name";
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(active);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenDeactivateSubCaptureIsCalledThenSubCaptureActiveStatesAreCleared) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.setSubCaptureWasActive(true);

    aubSubCaptureManager.disableSubCapture();
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
    EXPECT_FALSE(aubSubCaptureManager.wasSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureKeepsInactiveThenMakeEachEnqueueBlocking) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.setSubCaptureIsActive(false);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(false);

    aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(DebugManager.flags.MakeEachEnqueueBlocking.get());
    EXPECT_FALSE(DebugManager.flags.ForceCsrFlushing.get());
    EXPECT_FALSE(DebugManager.flags.ForceCsrReprogramming.get());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureGetsActiveThenDontMakeEachEnqueueBlockingAndForceCsrReprogramming) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.setSubCaptureIsActive(false);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);

    aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(DebugManager.flags.ForceCsrFlushing.get());
    EXPECT_TRUE(DebugManager.flags.ForceCsrReprogramming.get());
    EXPECT_FALSE(DebugManager.flags.MakeEachEnqueueBlocking.get());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureKeepsActiveThenDontMakeEachEnqueueBlocking) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);

    aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(DebugManager.flags.ForceCsrFlushing.get());
    EXPECT_FALSE(DebugManager.flags.ForceCsrReprogramming.get());
    EXPECT_FALSE(DebugManager.flags.MakeEachEnqueueBlocking.get());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureGetsInactiveThenMakeEachEnqueueBlockingAndForceCsrFlushing) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(false);

    aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(DebugManager.flags.ForceCsrFlushing.get());
    EXPECT_FALSE(DebugManager.flags.ForceCsrReprogramming.get());
    EXPECT_TRUE(DebugManager.flags.MakeEachEnqueueBlocking.get());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureActiveStatesAreDeterminedThenIsSubCaptureFunctionReturnCorrectValues) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

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

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInOffModeWhenGetSubCaptureFileNameIsCalledThenItReturnsEmptyFileName) {
    AubSubCaptureManagerMock aubSubCaptureManager("");
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    EXPECT_STREQ("", aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGetSubCaptureFileNameIsCalledAndExternalFileNameIsSpecifiedThenItReturnsItsName) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    std::string externalFileName = "external_file_name.aub";
    aubSubCaptureManager.setExternalFileName(externalFileName);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    EXPECT_STREQ(externalFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledAndExternalFileNameIsSpecifiedThenItReturnsItsName) {
    AubSubCaptureManagerMock aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    std::string externalFileName = "external_file_name.aub";
    aubSubCaptureManager.setExternalFileName(externalFileName);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    EXPECT_STREQ(externalFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGetSubCaptureFileNameIsCalledAndExternalFileNameIsNotSpecifiedThenItGeneratesFilterFileName) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub");

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    std::string externalFileName = "";
    aubSubCaptureManager.setExternalFileName(externalFileName);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    std::string filterFileName = aubSubCaptureManager.generateFilterFileName();
    EXPECT_STREQ(filterFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledAndExternalFileNameIsNotSpecifiedThenItGeneratesToggleFileName) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub");

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    std::string externalFileName = "";
    aubSubCaptureManager.setExternalFileName(externalFileName);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    std::string toggleFileName = aubSubCaptureManager.generateToggleFileName(multiDispatchInfo);
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGetSubCaptureFileNameIsCalledManyTimesAndExternalFileNameIsNotSpecifiedThenItGeneratesFilterFileNameOnceOnly) {
    struct AubSubCaptureManagerMockWithFilterFileNameGenerationCount : AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        std::string generateFilterFileName() const override {
            generateFilterFileNameCount++;
            return "aubfile_filter.aub";
        }
        mutable uint32_t generateFilterFileNameCount = 0;
    } aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    EXPECT_EQ(1u, aubSubCaptureManager.generateFilterFileNameCount);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledManyTimesAndExternalFileNameIsNotSpecifiedThenItGeneratesToggleFileNameOnceOnly) {
    struct AubSubCaptureManagerMockWithToggleFileNameGenerationCount : AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        std::string generateToggleFileName(const MultiDispatchInfo &dispatchInfo) const override {
            generateToggleFileNameCount++;
            return "aubfile_toggle.aub";
        }
        mutable uint32_t generateToggleFileNameCount = 0;
    } aubSubCaptureManager("");

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    EXPECT_EQ(1u, aubSubCaptureManager.generateToggleFileNameCount);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGenerateFilterFileNameIsCalledThenItGeneratesFileNameWithStartAndEndIndexes) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub");
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelStartIdx = 123;
    aubSubCaptureManager.subCaptureFilter.dumpKernelEndIdx = 456;
    std::string filterFileName = aubSubCaptureManager.generateFilterFileName();
    EXPECT_NE(std::string::npos, filterFileName.find("from_123_to_456"));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGenerateFilterFileNameIsCalledAndKernelNameIsSpecifiedInFilterThenItGeneratesFileNameWithNameOfKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub");
    std::string kernelName = "kernel_name";
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelName = kernelName;
    std::string filterFileName = aubSubCaptureManager.generateFilterFileName();
    EXPECT_NE(std::string::npos, filterFileName.find(kernelName));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGenerateFilterFileNameIsCalledAndKernelNameIsSpecifiedInFilterThenItGeneratesFileNameWithStartAndEndIndexesOfKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub");
    std::string kernelName = "kernel_name";
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpKernelName = kernelName;
    aubSubCaptureManager.subCaptureFilter.dumpNamedKernelStartIdx = 12;
    aubSubCaptureManager.subCaptureFilter.dumpNamedKernelEndIdx = 17;
    std::string filterFileName = aubSubCaptureManager.generateFilterFileName();
    EXPECT_NE(std::string::npos, filterFileName.find("from_12_to_17"));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGenerateToggleFileNameIsCalledThenItGeneratesFileNameWithKernelCurrentIndex) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub");
    std::string kernelCurrentIndex = "from_" + std::to_string(aubSubCaptureManager.getKernelCurrentIndex() - 1);
    MultiDispatchInfo dispatchInfo;
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    std::string filterFileName = aubSubCaptureManager.generateToggleFileName(dispatchInfo);
    EXPECT_NE(std::string::npos, filterFileName.find(kernelCurrentIndex));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGenerateToggleFileNameIsCalledAndDispatchInfoIsEmptyThenItGeneratesFileNameWithoutNameOfKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub");
    std::string kernelName = "kernel_name";
    MultiDispatchInfo dispatchInfo;
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.subCaptureFilter.dumpKernelName = kernelName;
    std::string toggleFileName = aubSubCaptureManager.generateToggleFileName(dispatchInfo);
    EXPECT_EQ(std::string::npos, toggleFileName.find(kernelName));
}

TEST_F(AubSubCaptureTest, givenMultiDispatchInfoWithMultipleKernelsWhenGenerateToggleFileNameThenPickMainKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub");
    KernelInfo mainKernelInfo = {};
    mainKernelInfo.name = "main_kernel";

    MockKernel mainKernel(program.get(), mainKernelInfo, *pDevice);
    MockKernel kernel1(program.get(), kernelInfo, *pDevice);
    MockKernel kernel2(program.get(), kernelInfo, *pDevice);

    DispatchInfo mainDispatchInfo(&mainKernel, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
    DispatchInfo dispatchInfo1(&kernel1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
    DispatchInfo dispatchInfo2(&kernel2, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});

    MultiDispatchInfo multiDispatchInfo(&mainKernel);
    multiDispatchInfo.push(dispatchInfo1);
    multiDispatchInfo.push(mainDispatchInfo);
    multiDispatchInfo.push(dispatchInfo2);

    std::string externalFileName = "";
    aubSubCaptureManager.setExternalFileName(externalFileName);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    std::string toggleFileName = aubSubCaptureManager.generateToggleFileName(multiDispatchInfo);
    EXPECT_NE(std::string::npos, toggleFileName.find(mainKernelInfo.name));
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenKernelNameIsSpecifiedThenNamedKernelIndexesShouldApplyToTheSpecifiedKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub");
    std::string kernelName = "kernel_name";
    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.subCaptureFilter.dumpNamedKernelStartIdx = 1;
    aubSubCaptureManager.subCaptureFilter.dumpNamedKernelEndIdx = 1;
    aubSubCaptureManager.subCaptureFilter.dumpKernelName = kernelName;

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    bool active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(active);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());

    active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(active);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());

    active = aubSubCaptureManager.activateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(active);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}
