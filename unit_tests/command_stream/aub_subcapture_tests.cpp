/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_stream/aub_subcapture.h"
#include "runtime/helpers/dispatch_info.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
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
    AubSubCaptureCommon subCaptureCommon;
};

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureToggleCaptureOnOffIsUnspecifiedThenSubCaptureIsToggledOffByDefault) {
    struct AubSubCaptureManagerWithToggleActiveMock : public AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        using AubSubCaptureManager::isSubCaptureToggleActive;
    } aubSubCaptureManagerWithToggleActiveMock("", subCaptureCommon);

    EXPECT_FALSE(aubSubCaptureManagerWithToggleActiveMock.isSubCaptureToggleActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureToggleFileNameIsUnspecifiedThenEmptyToggleFileNameIsReturnedByDefault) {
    struct AubSubCaptureManagerWithToggleFileNameMock : public AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        using AubSubCaptureManager::getToggleFileName;
    } aubSubCaptureManagerWithToggleFileNameMock("", subCaptureCommon);

    EXPECT_STREQ("", aubSubCaptureManagerWithToggleFileNameMock.getToggleFileName().c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenAubCaptureFileNameIsUnspecifiedThenEmptyFileNameIsReturnedByDefault) {
    AubSubCaptureManagerMock aubSubCaptureManager("file_name.aub", subCaptureCommon);

    EXPECT_STREQ("", aubSubCaptureManager.getAubCaptureFileName().c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenItIsCreatedThenItIsInitializedWithDefaults) {
    std::string initialFileName = "initial_file_name.aub";
    AubSubCaptureManagerMock aubSubCaptureManager(initialFileName, subCaptureCommon);

    EXPECT_EQ(AubSubCaptureCommon::SubCaptureMode::Off, subCaptureCommon.subCaptureMode);
    EXPECT_STREQ("", subCaptureCommon.subCaptureFilter.dumpKernelName.c_str());
    EXPECT_EQ(0u, subCaptureCommon.subCaptureFilter.dumpKernelStartIdx);
    EXPECT_EQ(static_cast<uint32_t>(-1), subCaptureCommon.subCaptureFilter.dumpKernelEndIdx);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureMode());
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
    EXPECT_FALSE(aubSubCaptureManager.wasSubCaptureActiveInPreviousEnqueue());
    EXPECT_EQ(0u, aubSubCaptureManager.getKernelCurrentIndex());
    EXPECT_TRUE(aubSubCaptureManager.getUseToggleFileName());
    EXPECT_STREQ(initialFileName.c_str(), aubSubCaptureManager.getInitialFileName().c_str());
    EXPECT_STREQ("", aubSubCaptureManager.getCurrentFileName().c_str());
    EXPECT_NE(nullptr, aubSubCaptureManager.getSettingsReader());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenCheckAndActivateSubCaptureIsCalledWithEmptyDispatchInfoThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    MultiDispatchInfo dispatchInfo;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(dispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenCheckAndActivateSubCaptureIsCalledWithNonEmptyDispatchInfoThenKernelCurrentIndexIsIncremented) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    uint32_t kernelCurrentIndex = aubSubCaptureManager.getKernelCurrentIndex();
    ASSERT_EQ(0u, kernelCurrentIndex);

    aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_EQ(kernelCurrentIndex + 0, aubSubCaptureManager.getKernelCurrentIndex());

    aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_EQ(kernelCurrentIndex + 1, aubSubCaptureManager.getKernelCurrentIndex());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenCheckAndActivateSubCaptureIsCalledButSubCaptureModeIsOffThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Off;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureIsToggledOnThenSubCaptureGetsAndRemainsActivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(status.isActive);
    EXPECT_TRUE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenCheckAndActivateSubCaptureIsCalledButSubCaptureIsToggledOffThenSubCaptureRemainsDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(false);
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenCheckAndActivateSubCaptureIsCalledButSubCaptureIsToggledOnAndOffThenSubCaptureGetsActivatedAndDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    aubSubCaptureManager.setSubCaptureToggleActive(false);
    status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_TRUE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterIsDefaultThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(status.isActive);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithValidKernelStartIndexIsSpecifiedThenSubCaptureGetsActivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelStartIdx = 0;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelStartIndexIsSpecifiedThenSubCaptureRemainsDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelStartIdx = 1;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelEndIndexIsSpecifiedThenSubCaptureRemainsDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelEndIdx = 0;
    subCaptureCommon.getKernelCurrentIndexAndIncrement();
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithValidKernelNameIsSpecifiedThenSubCaptureGetsActivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "kernel_name";
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelNameIsSpecifiedThenSubCaptureRemainsDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "invalid_kernel_name";
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenDeactivateSubCaptureIsCalledThenSubCaptureActiveStatesAreCleared) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.setSubCaptureWasActiveInPreviousEnqueue(true);

    aubSubCaptureManager.disableSubCapture();
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
    EXPECT_FALSE(aubSubCaptureManager.wasSubCaptureActiveInPreviousEnqueue());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureActiveStatesAreDeterminedThenIsSubCaptureFunctionReturnCorrectValues) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    aubSubCaptureManager.setSubCaptureWasActiveInPreviousEnqueue(false);
    aubSubCaptureManager.setSubCaptureIsActive(false);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureEnabled());

    aubSubCaptureManager.setSubCaptureWasActiveInPreviousEnqueue(false);
    aubSubCaptureManager.setSubCaptureIsActive(true);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureEnabled());

    aubSubCaptureManager.setSubCaptureWasActiveInPreviousEnqueue(true);
    aubSubCaptureManager.setSubCaptureIsActive(false);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureEnabled());

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.setSubCaptureWasActiveInPreviousEnqueue(true);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureEnabled());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureActiveStatesAreDeterminedThenGetSubCaptureStatusReturnsCorrectValues) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);
    AubSubCaptureStatus aubSubCaptureStatus{};

    aubSubCaptureManager.setSubCaptureWasActiveInPreviousEnqueue(false);
    aubSubCaptureManager.setSubCaptureIsActive(false);
    aubSubCaptureStatus = aubSubCaptureManager.getSubCaptureStatus();
    EXPECT_FALSE(aubSubCaptureStatus.wasActiveInPreviousEnqueue);
    EXPECT_FALSE(aubSubCaptureStatus.isActive);

    aubSubCaptureManager.setSubCaptureWasActiveInPreviousEnqueue(false);
    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureStatus = aubSubCaptureManager.getSubCaptureStatus();
    EXPECT_FALSE(aubSubCaptureStatus.wasActiveInPreviousEnqueue);
    EXPECT_TRUE(aubSubCaptureStatus.isActive);

    aubSubCaptureManager.setSubCaptureWasActiveInPreviousEnqueue(true);
    aubSubCaptureManager.setSubCaptureIsActive(false);
    aubSubCaptureStatus = aubSubCaptureManager.getSubCaptureStatus();
    EXPECT_TRUE(aubSubCaptureStatus.wasActiveInPreviousEnqueue);
    EXPECT_FALSE(aubSubCaptureStatus.isActive);

    aubSubCaptureManager.setSubCaptureIsActive(true);
    aubSubCaptureManager.setSubCaptureWasActiveInPreviousEnqueue(true);
    aubSubCaptureStatus = aubSubCaptureManager.getSubCaptureStatus();
    EXPECT_TRUE(aubSubCaptureStatus.wasActiveInPreviousEnqueue);
    EXPECT_TRUE(aubSubCaptureStatus.isActive);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenGetSubCaptureFileNameIsCalledAndAubCaptureFileNameIsSpecifiedThenItReturnsTheSpecifiedFileName) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpCaptureFileName.set("aubcapture_file_name.aub");

    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);
    MultiDispatchInfo multiDispatchInfo;
    EXPECT_STREQ("aubcapture_file_name.aub", aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInOffModeWhenGetSubCaptureFileNameIsCalledThenItReturnsEmptyFileName) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    EXPECT_STREQ("", aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGetSubCaptureFileNameIsCalledAndToggleFileNameIsSpecifiedThenItReturnsItsName) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    std::string toggleFileName = "toggle_file_name.aub";
    aubSubCaptureManager.setToggleFileName(toggleFileName);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledAndToggleFileNameIsSpecifiedThenItReturnsItsName) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    std::string toggleFileName = "toggle_file_name.aub";
    aubSubCaptureManager.setToggleFileName(toggleFileName);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledAndBothToggleAndAubCaptureFileNamesAreSpecifiedThenToggleNameTakesPrecedence) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpCaptureFileName.set("aubcapture_file_name.aub");

    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    MultiDispatchInfo multiDispatchInfo;
    std::string toggleFileName = "toggle_file_name.aub";
    aubSubCaptureManager.setToggleFileName(toggleFileName);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGetSubCaptureFileNameIsCalledAndToggleFileNameIsNotSpecifiedThenItGeneratesFilterFileName) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    aubSubCaptureManager.setToggleFileName("");

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    std::string filterFileName = aubSubCaptureManager.generateFilterFileName();
    EXPECT_STREQ(filterFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledAndToggleFileNameIsNotSpecifiedThenItGeneratesToggleFileName) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    aubSubCaptureManager.setToggleFileName("");

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    std::string toggleFileName = aubSubCaptureManager.generateToggleFileName(multiDispatchInfo);
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledForEmptyDispatchInfoThenGenerateToggleFileNameWithoutKernelName) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);

    MultiDispatchInfo dispatchInfo;
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    auto toggleFileName = aubSubCaptureManager.generateToggleFileName(dispatchInfo);
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(dispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGetSubCaptureFileNameIsCalledManyTimesAndToggleFileNameIsNotSpecifiedThenItGeneratesFilterFileNameOnceOnly) {
    struct AubSubCaptureManagerMockWithFilterFileNameGenerationCount : AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        std::string generateFilterFileName() const override {
            generateFilterFileNameCount++;
            return "aubfile_filter.aub";
        }
        mutable uint32_t generateFilterFileNameCount = 0;
    } aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    EXPECT_EQ(1u, aubSubCaptureManager.generateFilterFileNameCount);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledManyTimesAndToggleFileNameIsNotSpecifiedThenItGeneratesToggleFileNameOnceOnly) {
    struct AubSubCaptureManagerMockWithToggleFileNameGenerationCount : AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        std::string generateToggleFileName(const MultiDispatchInfo &dispatchInfo) const override {
            generateToggleFileNameCount++;
            return "aubfile_toggle.aub";
        }
        mutable uint32_t generateToggleFileNameCount = 0;
    } aubSubCaptureManager("", subCaptureCommon);

    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    EXPECT_EQ(1u, aubSubCaptureManager.generateToggleFileNameCount);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGenerateFilterFileNameIsCalledThenItGeneratesFileNameWithStartAndEndIndexes) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelStartIdx = 123;
    subCaptureCommon.subCaptureFilter.dumpKernelEndIdx = 456;
    std::string filterFileName = aubSubCaptureManager.generateFilterFileName();
    EXPECT_NE(std::string::npos, filterFileName.find("from_123_to_456"));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGenerateFilterFileNameIsCalledAndKernelNameIsSpecifiedInFilterThenItGeneratesFileNameWithNameOfKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);
    std::string kernelName = "kernel_name";
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = kernelName;
    std::string filterFileName = aubSubCaptureManager.generateFilterFileName();
    EXPECT_NE(std::string::npos, filterFileName.find(kernelName));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGenerateFilterFileNameIsCalledAndKernelNameIsSpecifiedInFilterThenItGeneratesFileNameWithStartAndEndIndexesOfKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);
    std::string kernelName = "kernel_name";
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = kernelName;
    subCaptureCommon.subCaptureFilter.dumpNamedKernelStartIdx = 12;
    subCaptureCommon.subCaptureFilter.dumpNamedKernelEndIdx = 17;
    std::string filterFileName = aubSubCaptureManager.generateFilterFileName();
    EXPECT_NE(std::string::npos, filterFileName.find("from_12_to_17"));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGenerateToggleFileNameIsCalledThenItGeneratesFileNameWithKernelCurrentIndex) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);
    std::string kernelCurrentIndex = "from_" + std::to_string(aubSubCaptureManager.getKernelCurrentIndex());
    MultiDispatchInfo dispatchInfo;
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    std::string filterFileName = aubSubCaptureManager.generateToggleFileName(dispatchInfo);
    EXPECT_NE(std::string::npos, filterFileName.find(kernelCurrentIndex));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGenerateToggleFileNameIsCalledAndDispatchInfoIsEmptyThenItGeneratesFileNameWithoutNameOfKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);
    std::string kernelName = "kernel_name";
    MultiDispatchInfo dispatchInfo;
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureCommon.subCaptureFilter.dumpKernelName = kernelName;
    std::string toggleFileName = aubSubCaptureManager.generateToggleFileName(dispatchInfo);
    EXPECT_EQ(std::string::npos, toggleFileName.find(kernelName));
}

TEST_F(AubSubCaptureTest, givenMultiDispatchInfoWithMultipleKernelsWhenGenerateToggleFileNameThenPickMainKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);
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

    aubSubCaptureManager.setToggleFileName("");

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    std::string toggleFileName = aubSubCaptureManager.generateToggleFileName(multiDispatchInfo);
    EXPECT_NE(std::string::npos, toggleFileName.find(mainKernelInfo.name));
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenKernelNameIsSpecifiedThenNamedKernelIndexesShouldApplyToTheSpecifiedKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);
    std::string kernelName = "kernel_name";
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpNamedKernelStartIdx = 1;
    subCaptureCommon.subCaptureFilter.dumpNamedKernelEndIdx = 1;
    subCaptureCommon.subCaptureFilter.dumpKernelName = kernelName;

    DispatchInfo dispatchInfo;
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    dispatchInfo.setKernel(&kernel);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());

    status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(status.isActive);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());

    status = aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenPublicInterfaceIsCalledThenLockShouldBeAcquired) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.isSubCaptureEnabled();
    EXPECT_TRUE(aubSubCaptureManager.isLocked);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.disableSubCapture();
    EXPECT_TRUE(aubSubCaptureManager.isLocked);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.checkAndActivateSubCapture(multiDispatchInfo);
    EXPECT_TRUE(aubSubCaptureManager.isLocked);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.getSubCaptureStatus();
    EXPECT_TRUE(aubSubCaptureManager.isLocked);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.getSubCaptureFileName(multiDispatchInfo);
    EXPECT_TRUE(aubSubCaptureManager.isLocked);
}
