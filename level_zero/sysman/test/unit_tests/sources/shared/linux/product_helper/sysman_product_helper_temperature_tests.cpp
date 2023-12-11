/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt_xml_offsets.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/temperature/linux/mock_sysfs_temperature.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperTemperatureTest = ::testing::Test;

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleAndReadSocTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG1) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = false;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    pPmt->mockReadSocTempResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleAndReadCoreTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG1) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = false;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    pPmt->mockReadCoreTempResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleAndReadComputeTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG1) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = false;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    pPmt->mockReadComputeTempResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingGlobalTemperatureThenSuccessIsReturned, IsDG1) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = false;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleAndReadComputeTemperatureFailsWhenGettingGpuTemperatureThenFailureIsReturned, IsDG1) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = false;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    pPmt->mockReadComputeTempResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingGpuTemperatureThenSuccessIsReturned, IsDG1) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = false;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingMemoryTemperatureThenUnsupportedIsReturned, IsDG1) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = false;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleAndReadTileMaxTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsPVC) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    pPmt->mockReadValueResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingGlobalTemperatureThenSuccessIsReturned, IsPVC) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleAndReadGTMaxTemperatureFailsWhenGettingGpuTemperatureThenFailureIsReturned, IsPVC) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    pPmt->mockReadValueResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingGpuMaxTemperatureThenSuccessIsReturned, IsPVC) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingMemoryMaxTemperatureThenSuccessIsReturned, IsPVC) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidHandleWhenQueryingMemoryTemperatureSupportThenTrueIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    bool result = pSysmanProductHelper->isMemoryMaxTemperatureSupported();
    EXPECT_EQ(true, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleAndReadSocTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG2) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    pPmt->mockReadSocTempResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingGlobalTemperatureThenSuccessIsReturned, IsDG2) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleAndReadSocTemperatureFailsWhenGettingGpuTemperatureThenFailureIsReturned, IsDG2) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    pPmt->mockReadSocTempResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingGpuMaxTemperatureThenSuccessIsReturned, IsDG2) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingMemoryMaxTemperatureThenUnSupportedIsReturned, IsDG2) {
    uint32_t subdeviceId = 0;
    bool onSubdevice = true;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<MockTemperatureFsAccess> pFsAccess = std::make_unique<MockTemperatureFsAccess>();
    auto pPmt = new MockTemperaturePmt(pFsAccess.get(), onSubdevice, subdeviceId);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pPmt, &temperature);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    delete pPmt;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidHandleWhenQueryingMemoryTemperatureSupportThenFalseIsReturned, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    bool result = pSysmanProductHelper->isMemoryMaxTemperatureSupported();
    EXPECT_EQ(false, result);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
