/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/performance/linux/sysman_os_performance_imp.h"
#include "level_zero/sysman/source/api/performance/sysman_performance.h"
#include "level_zero/sysman/source/api/performance/sysman_performance_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/performance/linux/mock_sysfs_performance.h"

#include <cmath>

namespace L0 {
namespace Sysman {
namespace ult {

static constexpr std::string_view i915MediaFreqFactorFileName0("gt/gt0/media_freq_factor");
static constexpr std::string_view i915MediaFreqFactorScaleFileName0("gt/gt0/media_freq_factor.scale");
static constexpr std::string_view i915BaseFreqFactorFileName0("gt/gt0/base_freq_factor");
static constexpr std::string_view i915BaseFreqFactorScaleFileName0("gt/gt0/base_freq_factor.scale");
static constexpr std::string_view i915MediaFreqFactorFileName1("gt/gt1/media_freq_factor");
static constexpr std::string_view i915MediaFreqFactorScaleFileName1("gt/gt1/media_freq_factor.scale");
static constexpr std::string_view i915BaseFreqFactorFileName1("gt/gt1/base_freq_factor");
static constexpr std::string_view i915BaseFreqFactorScaleFileName1("gt/gt1/base_freq_factor.scale");
static constexpr std::string_view sysPwrBalanceFileName("sys_pwr_balance");

static constexpr std::string_view xeMediaFreqFactorFileName0("device/tile0/gt0/media_freq_factor");
static constexpr std::string_view xeMediaFreqFactorScaleFileName0("device/tile0/gt0/media_freq_factor.scale");
static constexpr std::string_view xeBaseFreqFactorFileName0("device/tile0/gt0/base_freq_factor");
static constexpr std::string_view xeBaseFreqFactorScaleFileName0("device/tile0/gt0/base_freq_factor.scale");
static constexpr std::string_view xeMediaFreqFactorFileName1("device/tile1/gt1/media_freq_factor");
static constexpr std::string_view xeMediaFreqFactorScaleFileName1("device/tile1/gt1/media_freq_factor.scale");
static constexpr std::string_view xeBaseFreqFactorFileName1("device/tile1/gt1/base_freq_factor");
static constexpr std::string_view xeBaseFreqFactorScaleFileName1("device/tile1/gt1/base_freq_factor.scale");

static double mockBaseFreq = 128.0;
static double mockMediaFreq = 256.0;
static double mockScale = 0.00390625;
static double mockSysPwrBalance = 0.0;
static double setFactor = 0;
static double getFactor = 0;

static constexpr uint32_t mockI915HandleCount = 2;

static int mockOpenSuccess(const char *pathname, int flags) {
    int returnValue;
    std::string strPathName(pathname);
    if (strPathName == i915MediaFreqFactorFileName0 || strPathName == i915MediaFreqFactorFileName1 || strPathName == xeMediaFreqFactorFileName0 || strPathName == xeMediaFreqFactorFileName1) {
        returnValue = 1;
    } else if (strPathName == i915MediaFreqFactorScaleFileName0 || strPathName == i915MediaFreqFactorScaleFileName1 || strPathName == xeMediaFreqFactorScaleFileName0 || strPathName == xeMediaFreqFactorScaleFileName1) {
        returnValue = 2;
    } else if (strPathName == i915BaseFreqFactorFileName0 || strPathName == i915BaseFreqFactorFileName1 || strPathName == xeBaseFreqFactorFileName0 || strPathName == xeBaseFreqFactorFileName1) {
        returnValue = 3;
    } else if (strPathName == i915BaseFreqFactorScaleFileName0 || strPathName == i915BaseFreqFactorScaleFileName1 || strPathName == xeBaseFreqFactorScaleFileName0 || strPathName == xeBaseFreqFactorScaleFileName1) {
        returnValue = 4;
    } else if (strPathName == sysPwrBalanceFileName) {
        returnValue = 5;
    } else {
        returnValue = 0;
    }
    return returnValue;
}

static ssize_t mockReadSuccess(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    if (fd == 1) {
        oStream << mockMediaFreq;
    } else if (fd == 2 || fd == 4) {
        oStream << mockScale;
    } else if (fd == 3) {
        oStream << mockBaseFreq;
    } else if (fd == 5) {
        oStream << mockSysPwrBalance;
    } else {
        oStream << "0";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadFailure(int fd, void *buf, size_t count, off_t offset) {
    errno = ENOENT;
    return -1;
}

static ssize_t mockWriteSuccess(int fd, const void *buf, size_t count, off_t offset) {
    double multiplier = 0;
    if (fd == 1) {
        if (setFactor > halfOfMaxPerformanceFactor) {
            multiplier = 1;
        } else {
            multiplier = 0.5;
        }
    }
    multiplier = multiplier / mockScale;
    multiplier = std::round(multiplier);
    mockMediaFreq = multiplier;
    return count;
}

static int mockStatSuccess(const std::string &filePath, struct stat *statbuf) {
    statbuf->st_mode = S_IRUSR | S_IWUSR;
    return 0;
}

class ZesPerformanceFixture : public SysmanMultiDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
};

class ZesPerformanceFixtureI915 : public ZesPerformanceFixture {
  protected:
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();
    }
    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_perf_handle_t> getPerfHandlesForI915Version(uint32_t count) {
        std::vector<zes_perf_handle_t> handles(count, nullptr);

        VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
        VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);
        VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> mockPwrite(&NEO::SysCalls::sysCallsPwrite, &mockWriteSuccess);

        EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesPerformanceFixtureI915, GivenKmdInterfaceWhenGettingSysFsFilenamesForPerformanceForI915VersionThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915Upstream>(pLinuxSysmanImp->getSysmanProductHelper());
    EXPECT_STREQ(std::string(i915MediaFreqFactorFileName0).c_str(), pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceMediaFrequencyFactor, 0, true).c_str());
    EXPECT_STREQ(std::string(i915MediaFreqFactorScaleFileName0).c_str(), pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceMediaFrequencyFactorScale, 0, true).c_str());
}

HWTEST2_F(ZesPerformanceFixtureI915, GivenValidSysmanHandleWhenRetrievingPerfThenValidHandlesAreReturned, IsXeCore) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);

    uint32_t count = 0;
    getPerfHandlesForI915Version(count);

    ze_result_t result = zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockI915HandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockI915HandleCount);

    count = 0;
    std::vector<zes_perf_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockI915HandleCount);
}

HWTEST2_F(ZesPerformanceFixtureI915, GivenValidPerfHandleWhenGettingPerformancePropertiesThenValidPropertiesReturned, IsXeCore) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);

    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;

    auto handle = getPerfHandlesForI915Version(mockI915HandleCount);
    zes_perf_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetProperties(handle[0], &properties));
    EXPECT_TRUE(onSubdevice);
    EXPECT_EQ(properties.engines, ZES_ENGINE_TYPE_FLAG_MEDIA);
    EXPECT_EQ(properties.subdeviceId, subdeviceId);
}

TEST_F(ZesPerformanceFixtureI915, GivenBaseAndOtherDomainTypeWhenGettingPerfHandlesThenZeroHandlesAreRetrieved) {

    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_COMPUTE);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;

    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_OTHER);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;
}

HWTEST2_F(ZesPerformanceFixtureI915, GivenMediaDomainTypeWhenGettingPerfHandlesThenValidHandleIsRetrieved, IsXeCore) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915Upstream>(pLinuxSysmanImp->getSysmanProductHelper());
    auto pSysFsAccess = std::make_unique<MockSysFsAccessInterface>();

    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_MEDIA);
    pLinuxPerformanceImp->pSysmanKmdInterface = pSysmanKmdInterface.get();
    pLinuxPerformanceImp->pSysFsAccess = pSysFsAccess.get();
    EXPECT_TRUE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;
}

TEST_F(ZesPerformanceFixtureI915, GivenComputeDomainTypeWhenGettingPerfHandlesThenNotSupportedIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);

    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915Upstream>(pLinuxSysmanImp->getSysmanProductHelper());
    auto pSysFsAccess = std::make_unique<MockSysFsAccessInterface>();

    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_COMPUTE);
    pLinuxPerformanceImp->pSysmanKmdInterface = pSysmanKmdInterface.get();
    pLinuxPerformanceImp->pSysFsAccess = pSysFsAccess.get();
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;

    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_OTHER);
    pLinuxPerformanceImp->pSysmanKmdInterface = pSysmanKmdInterface.get();
    pLinuxPerformanceImp->pSysFsAccess = pSysFsAccess.get();
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;
}

TEST_F(ZesPerformanceFixtureI915, GivenValidPerfHandleAndPreadFailsWhenGettingPerformanceHandlesThenNoHandlesAreReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadFailure);

    uint32_t mockCount = 0;
    uint32_t count = 0;
    std::vector<zes_perf_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockCount);
}

HWTEST2_F(ZesPerformanceFixtureI915, GivenValidPerfHandleAndInvalidArgumentWhenSettingMediaConfigForProductFamilyPVCThenErrorIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> mockPwrite(&NEO::SysCalls::sysCallsPwrite, &mockWriteSuccess);

    uint32_t count = mockI915HandleCount;
    setFactor = 500;
    std::vector<zes_perf_handle_t> handles(count, nullptr);
    zes_perf_properties_t properties = {};
    EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetProperties(handles[0], &properties));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesPerformanceFactorSetConfig(handles[0], setFactor));
}

HWTEST2_F(ZesPerformanceFixtureI915, GivenValidPerfHandleWhenSettingMediaConfigAndGettingMediaConfigWhenProductFamilyIsPVCThenValidConfigIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> mockPwrite(&NEO::SysCalls::sysCallsPwrite, &mockWriteSuccess);

    uint32_t count = mockI915HandleCount;
    std::vector<zes_perf_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

    for (const auto &handle : handles) {
        zes_perf_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetProperties(handle, &properties));
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            setFactor = 49;
            getFactor = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), halfOfMaxPerformanceFactor);

            setFactor = 60;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), maxPerformanceFactor);

            setFactor = 100;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), maxPerformanceFactor);

            setFactor = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), halfOfMaxPerformanceFactor);
        }
    }
}

class ZesPerformanceFixtureXe : public ZesPerformanceFixture {
  protected:
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();
    }
    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }
};

TEST_F(ZesPerformanceFixtureXe, GivenKmdInterfaceWhenGettingSysFsFilenamesForPerformanceForXeVersionThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
    EXPECT_STREQ(std::string(xeMediaFreqFactorFileName0).c_str(), pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceMediaFrequencyFactor, 0, true).c_str());
    EXPECT_STREQ(std::string(xeMediaFreqFactorScaleFileName0).c_str(), pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceMediaFrequencyFactorScale, 0, true).c_str());
    EXPECT_STREQ(std::string(xeBaseFreqFactorFileName0).c_str(), pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceBaseFrequencyFactor, 0, true).c_str());
    EXPECT_STREQ(std::string(xeBaseFreqFactorScaleFileName0).c_str(), pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceBaseFrequencyFactorScale, 0, true).c_str());
    EXPECT_STREQ(std::string(sysPwrBalanceFileName).c_str(), pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceSystemPowerBalance, 0, false).c_str());
}

HWTEST2_F(ZesPerformanceFixtureXe, GivenPerfFactorDomainsWhenGettingPerfHandlesThenVerifyPerfFactorIsNotSupported, IsXeCore) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
    auto pSysFsAccess = std::make_unique<MockSysFsAccessInterface>();

    std::swap(pLinuxSysmanImp->pSysmanKmdInterface, pSysmanKmdInterface);
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysFsAccess.get());

    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_COMPUTE);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;

    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_MEDIA);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;

    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_OTHER);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;
}

TEST_F(ZesPerformanceFixtureXe, GivenPerfFactorDomainsWhenGettingPerfHandlesThenVerifyPerfFactorIsNotSupported) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    struct MockSysmanProductHelperPerf : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperPerf() = default;
        bool isPerfFactorSupported() override {
            return false;
        }
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperPerf>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
    auto pSysFsAccess = std::make_unique<MockSysFsAccessInterface>();

    std::swap(pLinuxSysmanImp->pSysmanKmdInterface, pSysmanKmdInterface);
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysFsAccess.get());

    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_COMPUTE);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;

    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_MEDIA);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;

    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_OTHER);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;
}

} // namespace ult
} // namespace Sysman
} // namespace L0