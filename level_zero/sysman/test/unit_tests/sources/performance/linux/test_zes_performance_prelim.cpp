/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/performance/linux/mock_sysfs_performance_prelim.h"

#include "gtest/gtest.h"

#include <cmath>

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCount = 5;
constexpr double maxPerformanceFactor = 100;
constexpr double halfOfMaxPerformanceFactor = 50;
constexpr double minPerformanceFactor = 0;
class ZesPerformanceFixtureI915Prelim : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<MockPerformanceSysfsAccess> pSysfsAccess;
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();
        std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915Prelim>(pLinuxSysmanImp->getSysmanProductHelper());
        std::swap(pLinuxSysmanImp->pSysmanKmdInterface, pSysmanKmdInterface);
        pSysfsAccess = std::make_unique<MockPerformanceSysfsAccess>();
    }
    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_perf_handle_t> getPerfHandles(uint32_t count) {
        std::vector<zes_perf_handle_t> handles(count, nullptr);

        EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidSysmanHandleWhenRetrievingPerfThenValidHandlesReturned, IsXeCore) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumPerformanceFactorDomains(pSysmanDevice->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumPerformanceFactorDomains(pSysmanDevice->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);

    count = 0;
    std::vector<zes_perf_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPerformanceFactorDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(ZesPerformanceFixtureI915Prelim, GivenInAnyDomainTypeIfcanReadFailsWhenGettingPerfHandlesThenZeroHandlesAreRetrieved) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    pSysfsAccess->mockCanReadResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    for (auto &handle : pSysmanDeviceImp->pPerformanceHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();

    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_MEDIA);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;

    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_OTHER);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;

    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 0, 0u, ZES_ENGINE_TYPE_FLAG_COMPUTE);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;
}

TEST_F(ZesPerformanceFixtureI915Prelim, GivenInAnyDomainTypeIfSysfsReadForMediaAndComputeScaleFailsWhileGettingPerfHandlesThenZeroHandlesAreRetrieved) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    pSysfsAccess->mockReadResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    for (auto &handle : pSysmanDeviceImp->pPerformanceHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();

    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_MEDIA);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());

    delete pLinuxPerformanceImp;
    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_COMPUTE);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());

    delete pLinuxPerformanceImp;
}

TEST_F(ZesPerformanceFixtureI915Prelim, GivenInAnyDomainTypeIfMediaAndBaseFreqFactorSysfsNodesAreAbsentWhenGettingPerfHandlesThenZeroHandlesAreRetrieved) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    for (auto &handle : pSysmanDeviceImp->pPerformanceHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();
    pSysfsAccess->mockReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_MEDIA);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());

    delete pLinuxPerformanceImp;
    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_COMPUTE);

    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());

    delete pLinuxPerformanceImp;
}

TEST_F(ZesPerformanceFixtureI915Prelim, GivenIncorrectConditionsWhenGettingPerfHandlesThenZeroHandlesAreRetrieved) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    // Check perf handle with incorrect domain type
    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_DMA);
    EXPECT_FALSE(pLinuxPerformanceImp->isPerformanceSupported());
    delete pLinuxPerformanceImp;
    pLinuxPerformanceImp = nullptr;
}

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandleWhenGettingPerformancePropertiesThenValidPropertiesReturned, IsXeCore) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;

    auto handle = getPerfHandles(1u);
    ASSERT_NE(nullptr, handle[0]);
    zes_perf_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetProperties(handle[0], &properties));
    EXPECT_TRUE(onSubdevice);
    EXPECT_EQ(properties.engines, ZES_ENGINE_TYPE_FLAG_MEDIA);
    EXPECT_EQ(properties.subdeviceId, subdeviceId);
}

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandleWhenGettingConfigThenSuccessIsReturned, IsXeCore) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        double factor = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &factor));
        EXPECT_EQ(factor, 100);
    }
}

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandlesWhenInvalidMultiplierValuesAreReturnedBySysfsInterfaceThenUnknownErrorIsReturned, IsXeCore) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    for (auto &handle : pSysmanDeviceImp->pPerformanceHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();
    pSysfsAccess->isReturnUnknownFailure = true;
    pSysmanDeviceImp->pPerformanceHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        double factor = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesPerformanceFactorGetConfig(handle, &factor));
    }
}

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandlesWhenBaseAndMediaFreqFactorNodesAreAbsentThenUnsupportedFeatureIsReturned, IsXeCore) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    for (auto &handle : pSysmanDeviceImp->pPerformanceHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPerformanceHandleContext->handleList.clear();
    pSysfsAccess->isMediaBaseFailure = true;
    pSysmanDeviceImp->pPerformanceHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        double factor = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPerformanceFactorGetConfig(handle, &factor));
    }
}

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandleWhenSettingConfigThenSuccessIsReturned, IsXeCore) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        double setFactor = 0;
        double getFactor;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));

        setFactor = 100;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
        getFactor = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
        EXPECT_DOUBLE_EQ(std::round(getFactor), setFactor);
    }
}

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandleWhenSettingMediaConfigAndGettingMediaConfigForProductOtherThanPVCThenValidConfigIsReturned, IsAtMostDg2) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_perf_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetProperties(handle, &properties));
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            double setFactor = 49;
            double getFactor = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorSetConfig(handle, setFactor));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetConfig(handle, &getFactor));
            EXPECT_DOUBLE_EQ(std::round(getFactor), halfOfMaxPerformanceFactor);

            setFactor = 50;
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
            EXPECT_DOUBLE_EQ(std::round(getFactor), minPerformanceFactor);
        }
    }
}

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandleWhenSettingMediaConfigAndGettingMediaConfigWhenProductFamilyIsPVCThenValidConfigIsReturned, IsPVC) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_perf_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPerformanceFactorGetProperties(handle, &properties));
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            double setFactor = 49;
            double getFactor = 0;
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

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandlesButSysfsReadsFailAtDifferentBrancesWhenSettingPerfConfigThenPerfGetConfigFails, IsXeCore) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    auto handles = getPerfHandles(mockHandleCount);
    double factor = 0;

    pSysfsAccess->isComputeInvalid = true;

    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_COMPUTE);
    EXPECT_TRUE(pLinuxPerformanceImp->isPerformanceSupported());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxPerformanceImp->osPerformanceGetConfig(&factor));
    delete pLinuxPerformanceImp;
    pLinuxPerformanceImp = nullptr;
    pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_DMA);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPerformanceImp->osPerformanceGetConfig(&factor));
    delete pLinuxPerformanceImp;
}

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandleWhenGettingPerfConfigForOtherDomainAndSysfsReadReturnsInvalidValueThenPerfGetConfigFails, IsXeCore) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    auto handles = getPerfHandles(mockHandleCount);
    double factor = 0;

    pSysfsAccess->returnNegativeFactor = true;
    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 0, 0u, ZES_ENGINE_TYPE_FLAG_OTHER);
    EXPECT_TRUE(pLinuxPerformanceImp->isPerformanceSupported());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxPerformanceImp->osPerformanceGetConfig(&factor));
    delete pLinuxPerformanceImp;
}

HWTEST2_F(ZesPerformanceFixtureI915Prelim, GivenValidPerfHandlesButSysfsReadsFailAtDifferentBrancesWhenSettingPerfConfigThenPerfSetConfigFails, IsXeCore) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    auto handles = getPerfHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        double setFactor = 110;
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesPerformanceFactorSetConfig(handle, setFactor));

        setFactor = -10;
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesPerformanceFactorSetConfig(handle, setFactor));
    }

    double factor = 50;
    // Set perf Config with incorrect domain type
    PublicLinuxPerformanceImp *pLinuxPerformanceImp = new PublicLinuxPerformanceImp(pOsSysman, 1, 0u, ZES_ENGINE_TYPE_FLAG_DMA);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPerformanceImp->osPerformanceSetConfig(factor));
    delete pLinuxPerformanceImp;
}

TEST_F(ZesPerformanceFixtureI915Prelim, GivenValidOfjectsOfClassPerformanceImpAndPerformanceHandleContextThenDuringObjectReleaseCheckDestructorBranches) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    for (auto &handle : pSysmanDeviceImp->pPerformanceHandleContext->handleList) {
        auto pPerformanceImp = static_cast<L0::Sysman::PerformanceImp *>(handle);
        delete pPerformanceImp->pOsPerformance;
        pPerformanceImp->pOsPerformance = nullptr;
        delete handle;
        handle = nullptr;
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
