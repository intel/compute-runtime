/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/global_operations/linux/mock_global_operations.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

#include "driver_version.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperGlobalOperationsTest = SysmanDeviceFixture;
HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleWhenQueryingRepairStatusSupportThenTrueIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(true, pSysmanProductHelper->isRepairStatusSupported());
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleWhenQueryingRepairStatusSupportThenFalseIsReturned, IsNotPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(false, pSysmanProductHelper->isRepairStatusSupported());
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsReadFailsWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockReadError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileIsEmptyWhenCallingMemoryGetPageOfflineStateExpThenZeroOfflinePagesAndSuccessIsReturned, IsCRI) {
    uint32_t expectedCount = 0u;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithEmptyLines = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, nullptr));
    EXPECT_EQ(expectedCount, count);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasRepeatedEntriesWhenCallingMemoryGetPageOfflineStateExpThenSingleEntryOfOfflinePagesAndSuccessIsReturned, IsCRI) {

    uint32_t expectedCount = 1u;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithRepeatedEntries = true;
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(expectedCount, count);

    std::vector<zes_intel_mem_page_info_exp_t> pageOfflineInfo(count, {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP});
    result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, pageOfflineInfo.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    for (const auto &pageInfo : pageOfflineInfo) {
        EXPECT_EQ(static_cast<uint64_t>(0x00001234), pageInfo.pageAddress);
        EXPECT_EQ(static_cast<uint32_t>(0x00001000), pageInfo.pageSize);
    }
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasNoColonEntryWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithNoColonEntry = true;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, &pageOfflineInfo);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasOneColonEntryWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithOneColonEntry = true;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, &pageOfflineInfo);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasNoFieldsEntryWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithNoFieldsEntry = true;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, &pageOfflineInfo);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasNoSizeFieldWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithNoSizeField = true;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, &pageOfflineInfo);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasInvalidAddressWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithInvalidAddress = true;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, &pageOfflineInfo);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasInvalidSizeWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithInvalidSize = true;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, &pageOfflineInfo);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasNoStatusWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithNoStatus = true;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, &pageOfflineInfo);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasInvalidStatusWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithInvalidStatus = true;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    ze_result_t result = pSysmanProductHelper->memoryGetPageOfflineStateExp(pSysfsAccess.get(), pageStatus, &count, memPageInfoList, &pageOfflineInfo);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsReadFailsWhenCallingGetMaxMemoryOfflinePagesThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockReadError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    uint32_t maxOfflinePages = 0;
    ze_result_t result = pSysmanProductHelper->getMaxMemoryOfflinePages(pSysfsAccess.get(), &maxOfflinePages);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleWhenCallingGetMaxMemoryOfflinePagesThenProperValuesAndSuccessIsReturned, IsCRI) {
    uint32_t expectedMaxOfflinePages = 10000u;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    uint32_t maxOfflinePages = 0;
    ze_result_t result = pSysmanProductHelper->getMaxMemoryOfflinePages(pSysfsAccess.get(), &maxOfflinePages);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(expectedMaxOfflinePages, maxOfflinePages);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasNoColonEntryWhenCallingGetMaxMemoryOfflinePagesThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithNoColonEntry = true;
    uint32_t maxOfflinePages = 0;
    ze_result_t result = pSysmanProductHelper->getMaxMemoryOfflinePages(pSysfsAccess.get(), &maxOfflinePages);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasNoMaxPagesLineWhenCallingGetMaxMemoryOfflinePagesThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithEmptyLines = true;
    uint32_t maxOfflinePages = 0;
    ze_result_t result = pSysmanProductHelper->getMaxMemoryOfflinePages(pSysfsAccess.get(), &maxOfflinePages);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasInvalidMaxPageEntryWhenCallingGetMaxMemoryOfflinePagesThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithInvalidMaxPageEntry = true;
    uint32_t maxOfflinePages = 0;
    ze_result_t result = pSysmanProductHelper->getMaxMemoryOfflinePages(pSysfsAccess.get(), &maxOfflinePages);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleAndSysfsFileHasMaxPagesWithNoValueWhenCallingGetMaxMemoryOfflinePagesThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pSysfsAccess->mockFileWithMaxPagesNoValue = true;
    uint32_t maxOfflinePages = 0;
    ze_result_t result = pSysmanProductHelper->getMaxMemoryOfflinePages(pSysfsAccess.get(), &maxOfflinePages);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleWhenCallingMemoryGetPageOfflineStateExpThenErrorIsReturned, IsNotCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    uint32_t count = 0;
    std::vector<MemPageInfo> memPageInfoList = {};
    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanProductHelper->memoryGetPageOfflineStateExp(&pLinuxSysmanImp->getSysfsAccess(), pageStatus, &count, memPageInfoList, &pageOfflineInfo));
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleWhenCallingGetMaxMemoryOfflinePagesThenErrorIsReturned, IsNotCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t maxOfflinePages = 0;
    ze_result_t result = pSysmanProductHelper->getMaxMemoryOfflinePages(&pLinuxSysmanImp->getSysfsAccess(), &maxOfflinePages);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenCallingMemoryGetPageOfflineStateExpThenSuccessIsReturned, IsCRI) {
    uint32_t expectedNumberMemoryPageOffline = 1u;
    uint32_t expectedNumberMemoryPagePendingOffline = 2u; // Both 'P' and 'F' status map to pending offline
    auto pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    uint32_t count = 0;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    ze_result_t result = zesIntelDeviceMemoryGetPageOfflineStateExp(pSysmanDevice->toHandle(), pageStatus, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(expectedNumberMemoryPageOffline, count);

    std::vector<zes_intel_mem_page_info_exp_t> pageOfflineInfo(count, {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP});
    result = zesIntelDeviceMemoryGetPageOfflineStateExp(pSysmanDevice->toHandle(), pageStatus, &count, pageOfflineInfo.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    for (const auto &pageInfo : pageOfflineInfo) {
        EXPECT_EQ(static_cast<uint64_t>(0x00001234), pageInfo.pageAddress);
        EXPECT_EQ(static_cast<uint32_t>(0x00001000), pageInfo.pageSize);
    }

    count = 0;
    pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_PENDING_OFFLINE;
    result = zesIntelDeviceMemoryGetPageOfflineStateExp(pSysmanDevice->toHandle(), pageStatus, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(expectedNumberMemoryPagePendingOffline, count);

    pageOfflineInfo.clear();
    pageOfflineInfo.resize(count, {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP});
    result = zesIntelDeviceMemoryGetPageOfflineStateExp(pSysmanDevice->toHandle(), pageStatus, &count, pageOfflineInfo.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<uint64_t>(0x00005678), pageOfflineInfo[0].pageAddress);
    EXPECT_EQ(static_cast<uint32_t>(0x00001000), pageOfflineInfo[0].pageSize);
    EXPECT_EQ(static_cast<uint64_t>(0x00007890), pageOfflineInfo[1].pageAddress);
    EXPECT_EQ(static_cast<uint32_t>(0x00001000), pageOfflineInfo[1].pageSize);
}

HWTEST2_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenCallingZesIntelDeviceMemoryGetPageOfflineStateExpThenErrorIsReturned, IsNotCRI) {

    zes_intel_mem_page_info_exp_t pageOfflineInfo = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_INFO_EXP};
    uint32_t count = 0;
    zes_intel_mem_page_status_exp_t pageStatus = ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE;
    ze_result_t result = zesIntelDeviceMemoryGetPageOfflineStateExp(pSysmanDevice->toHandle(), pageStatus, &count, &pageOfflineInfo);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenOverrideVersionBuildWhenCallingGetDriverVersionThenForcedVersionBuildIsUsed, IsCRI) {
    DebugManagerStateRestore restorer;
    constexpr uint32_t versionBuild = 3900u;
    NEO::debugManager.flags.OverrideVersionBuild.set(versionBuild);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    char driverVersion[ZES_STRING_PROPERTY_SIZE] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getDriverVersion(driverVersion));

    auto expectedDriverVersion = static_cast<uint32_t>(L0::DriverHandle::initialDriverVersionValue) + versionBuild;
    EXPECT_STREQ(std::to_string(expectedDriverVersion).c_str(), driverVersion);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenOverrideDriverVersionWhenCallingGetDriverVersionThenOverriddenValueTakesPrecedence, IsCRI) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.OverrideVersionBuild.set(20);
    NEO::debugManager.flags.OverrideDriverVersion.set(1234);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    char driverVersion[ZES_STRING_PROPERTY_SIZE] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getDriverVersion(driverVersion));
    EXPECT_STREQ("1234", driverVersion);
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleWhenCallingGetDriverVersionThenUnsupportedIsReturnedAndBufferIsUntouched, IsNotCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    char driverVersion[ZES_STRING_PROPERTY_SIZE] = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanProductHelper->getDriverVersion(driverVersion));
    EXPECT_STREQ("", driverVersion);
}

HWTEST2_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenCallingZesDeviceGetPropertiesThenUmdDriverVersionIsReportedInsteadOfKmdVersion, IsCRI) {
    pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime = MockOSTime::create();
    pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime->setDeviceTimerResolution();

    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties));

    auto expectedDriverVersion = static_cast<uint32_t>(L0::DriverHandle::initialDriverVersionValue);
    expectedDriverVersion += static_cast<uint32_t>(NEO_VERSION_BUILD);
    EXPECT_STREQ(std::to_string(expectedDriverVersion).c_str(), properties.driverVersion);
}

HWTEST2_F(SysmanDeviceFixture, GivenValidExtensionStructureWhenCallingZesDeviceGetPropertiesThenProperValuesAndSuccessIsReturned, IsNotCRI) {

    pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime = MockOSTime::create();
    pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime->setDeviceTimerResolution();

    uint32_t expectedMaxOfflinePages = 0;
    zes_intel_mem_page_offline_properties_exp_t memPageOfflineProperties = {ZES_INTEL_STRUCTURE_TYPE_MEMORY_PAGE_OFFLINE_PROPERTIES_EXP, nullptr, 10};
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES, &memPageOfflineProperties};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(expectedMaxOfflinePages, memPageOfflineProperties.maxOfflinePages);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
