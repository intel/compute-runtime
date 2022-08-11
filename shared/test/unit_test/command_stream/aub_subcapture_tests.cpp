/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_subcapture.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_aub_subcapture_manager.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct AubSubCaptureTest : public DeviceFixture,
                           public ::testing::Test {
    void SetUp() override {
        DeviceFixture::setUp();
        dbgRestore = new DebugManagerStateRestore();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
        delete dbgRestore;
    }
    static constexpr const char *kernelName = "kernel_name";
    DebugManagerStateRestore *dbgRestore;
    AubSubCaptureCommon subCaptureCommon;
};

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureToggleCaptureOnOffIsUnspecifiedThenSubCaptureIsToggledOffByDefault) {
    struct AubSubCaptureManagerWithToggleActiveMock : public AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        using AubSubCaptureManager::isSubCaptureToggleActive;
    } aubSubCaptureManagerWithToggleActiveMock("", subCaptureCommon, "");

    EXPECT_FALSE(aubSubCaptureManagerWithToggleActiveMock.isSubCaptureToggleActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenSubCaptureToggleFileNameIsUnspecifiedThenEmptyToggleFileNameIsReturnedByDefault) {
    struct AubSubCaptureManagerWithToggleFileNameMock : public AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        using AubSubCaptureManager::getToggleFileName;
    } aubSubCaptureManagerWithToggleFileNameMock("", subCaptureCommon, "");

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

    auto status = aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenCheckAndActivateSubCaptureIsCalledWithNonEmptyDispatchInfoThenKernelCurrentIndexIsIncremented) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    uint32_t kernelCurrentIndex = aubSubCaptureManager.getKernelCurrentIndex();
    ASSERT_EQ(0u, kernelCurrentIndex);

    aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_EQ(kernelCurrentIndex + 0, aubSubCaptureManager.getKernelCurrentIndex());

    aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_EQ(kernelCurrentIndex + 1, aubSubCaptureManager.getKernelCurrentIndex());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenCheckAndActivateSubCaptureIsCalledButSubCaptureModeIsOffThenSubCaptureIsInactive) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Off;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureIsToggledOnThenSubCaptureGetsAndRemainsActivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);
    auto status = aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    status = aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_TRUE(status.isActive);
    EXPECT_TRUE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenCheckAndActivateSubCaptureIsCalledButSubCaptureIsToggledOffThenSubCaptureRemainsDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(false);
    auto status = aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    status = aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenCheckAndActivateSubCaptureIsCalledButSubCaptureIsToggledOnAndOffThenSubCaptureGetsActivatedAndDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.setSubCaptureToggleActive(true);
    auto status = aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    aubSubCaptureManager.setSubCaptureToggleActive(false);
    status = aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_FALSE(status.isActive);
    EXPECT_TRUE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterIsDefaultThenSubCaptureIsActive) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(kernelName);
    EXPECT_TRUE(status.isActive);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithValidKernelStartIndexIsSpecifiedThenSubCaptureGetsActivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelStartIdx = 0;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(kernelName);
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelStartIndexIsSpecifiedThenSubCaptureRemainsDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelStartIdx = 1;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(kernelName);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelEndIndexIsSpecifiedThenSubCaptureRemainsDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelEndIdx = 0;
    subCaptureCommon.getKernelCurrentIndexAndIncrement();
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(kernelName);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithValidKernelNameIsSpecifiedThenSubCaptureGetsActivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "kernel_name";
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(kernelName);
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenCheckAndActivateSubCaptureIsCalledAndSubCaptureFilterWithInvalidKernelNameIsSpecifiedThenSubCaptureRemainsDeactivated) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "invalid_kernel_name";
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(kernelName);
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
    EXPECT_STREQ("aubcapture_file_name.aub", aubSubCaptureManager.getSubCaptureFileName("kernelName").c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInOffModeWhenGetSubCaptureFileNameIsCalledThenItReturnsEmptyFileName) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);
    EXPECT_STREQ("", aubSubCaptureManager.getSubCaptureFileName("kernelName").c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGetSubCaptureFileNameIsCalledAndToggleFileNameIsSpecifiedThenItReturnsItsName) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    std::string toggleFileName = "toggle_file_name.aub";
    aubSubCaptureManager.setToggleFileName(toggleFileName);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName("kernelName").c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledAndToggleFileNameIsSpecifiedThenItReturnsItsName) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    std::string toggleFileName = "toggle_file_name.aub";
    aubSubCaptureManager.setToggleFileName(toggleFileName);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName("kernelName").c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledAndBothToggleAndAubCaptureFileNamesAreSpecifiedThenToggleNameTakesPrecedence) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpCaptureFileName.set("aubcapture_file_name.aub");

    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    std::string toggleFileName = "toggle_file_name.aub";
    aubSubCaptureManager.setToggleFileName(toggleFileName);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName("kernelName").c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGetSubCaptureFileNameIsCalledAndToggleFileNameIsNotSpecifiedThenItGeneratesFilterFileName) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);

    aubSubCaptureManager.setToggleFileName("");

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    std::string filterFileName = aubSubCaptureManager.generateFilterFileName();
    EXPECT_STREQ(filterFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName("kernelName").c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledAndToggleFileNameIsNotSpecifiedThenItGeneratesToggleFileName) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);

    aubSubCaptureManager.setToggleFileName("");

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    std::string toggleFileName = aubSubCaptureManager.generateToggleFileName(kernelName);
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName(kernelName).c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledForEmptyDispatchInfoThenGenerateToggleFileNameWithoutKernelName) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    auto toggleFileName = aubSubCaptureManager.generateToggleFileName("kernelName");
    EXPECT_STREQ(toggleFileName.c_str(), aubSubCaptureManager.getSubCaptureFileName("kernelName").c_str());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenGetSubCaptureFileNameIsCalledManyTimesAndToggleFileNameIsNotSpecifiedThenItGeneratesFilterFileNameOnceOnly) {
    struct AubSubCaptureManagerMockWithFilterFileNameGenerationCount : AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        std::string generateFilterFileName() const override {
            generateFilterFileNameCount++;
            return "aubfile_filter.aub";
        }
        mutable uint32_t generateFilterFileNameCount = 0;
    } aubSubCaptureManager("", subCaptureCommon, "");

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    aubSubCaptureManager.getSubCaptureFileName("kernelName");
    aubSubCaptureManager.getSubCaptureFileName("kernelName");
    aubSubCaptureManager.getSubCaptureFileName("kernelName");
    EXPECT_EQ(1u, aubSubCaptureManager.generateFilterFileNameCount);
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGetSubCaptureFileNameIsCalledManyTimesAndToggleFileNameIsNotSpecifiedThenItGeneratesToggleFileNameOnceOnly) {
    struct AubSubCaptureManagerMockWithToggleFileNameGenerationCount : AubSubCaptureManager {
        using AubSubCaptureManager::AubSubCaptureManager;
        std::string generateToggleFileName(const std::string &kernelName) const override {
            generateToggleFileNameCount++;
            return "aubfile_toggle.aub";
        }
        mutable uint32_t generateToggleFileNameCount = 0;
    } aubSubCaptureManager("", subCaptureCommon, "");

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManager.getSubCaptureFileName("kernelName");
    aubSubCaptureManager.getSubCaptureFileName("kernelName");
    aubSubCaptureManager.getSubCaptureFileName("kernelName");
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
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    std::string filterFileName = aubSubCaptureManager.generateToggleFileName("kernelName");
    EXPECT_NE(std::string::npos, filterFileName.find(kernelCurrentIndex));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInToggleModeWhenGenerateToggleFileNameIsCalledAndDispatchInfoIsEmptyThenItGeneratesFileNameWithoutNameOfKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);
    std::string kernelName = "kernel_name";
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureCommon.subCaptureFilter.dumpKernelName = kernelName;
    std::string toggleFileName = aubSubCaptureManager.generateToggleFileName("kernelName");
    EXPECT_EQ(std::string::npos, toggleFileName.find(kernelName));
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerInFilterModeWhenKernelNameIsSpecifiedThenNamedKernelIndexesShouldApplyToTheSpecifiedKernel) {
    AubSubCaptureManagerMock aubSubCaptureManager("aubfile.aub", subCaptureCommon);
    std::string kernelName = "kernel_name";
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpNamedKernelStartIdx = 1;
    subCaptureCommon.subCaptureFilter.dumpNamedKernelEndIdx = 1;
    subCaptureCommon.subCaptureFilter.dumpKernelName = kernelName;

    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    auto status = aubSubCaptureManager.checkAndActivateSubCapture(kernelName);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());

    status = aubSubCaptureManager.checkAndActivateSubCapture(kernelName);
    EXPECT_TRUE(status.isActive);
    EXPECT_TRUE(aubSubCaptureManager.isSubCaptureActive());

    status = aubSubCaptureManager.checkAndActivateSubCapture(kernelName);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(aubSubCaptureManager.isSubCaptureActive());
}

TEST_F(AubSubCaptureTest, givenSubCaptureManagerWhenPublicInterfaceIsCalledThenLockShouldBeAcquired) {
    AubSubCaptureManagerMock aubSubCaptureManager("", subCaptureCommon);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.isSubCaptureEnabled();
    EXPECT_TRUE(aubSubCaptureManager.isLocked);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.disableSubCapture();
    EXPECT_TRUE(aubSubCaptureManager.isLocked);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.checkAndActivateSubCapture("kernelName");
    EXPECT_TRUE(aubSubCaptureManager.isLocked);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.getSubCaptureStatus();
    EXPECT_TRUE(aubSubCaptureManager.isLocked);

    aubSubCaptureManager.isLocked = false;
    aubSubCaptureManager.getSubCaptureFileName("kernelName");
    EXPECT_TRUE(aubSubCaptureManager.isLocked);
}
