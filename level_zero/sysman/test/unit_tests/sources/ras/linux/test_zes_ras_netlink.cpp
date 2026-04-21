/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/api/ras/linux/sysman_os_ras_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_nl_api.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_sysman_drm_nl_api.h"

#include <map>

namespace L0 {
namespace Sysman {
namespace ult {

class MockRasNetlinkUtil : public NetlinkRasUtil {
  public:
    using NetlinkRasUtil::drmNl;
    using NetlinkRasUtil::pLinuxSysmanImp;
    using NetlinkRasUtil::rasErrorList;
    using NetlinkRasUtil::rasNodeId;
    using NetlinkRasUtil::rasNodes;
    MockRasNetlinkUtil(zes_ras_error_type_t type, L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) : NetlinkRasUtil(type, pLinuxSysmanImp, subdeviceId) {}
    ~MockRasNetlinkUtil() override = default;
};

class SysmanRasNetlinkFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<MockRasNetlinkUtil> pRasNetlinkUtil = nullptr;
    std::unique_ptr<MockDrmNlApi> drmNlApi = nullptr;
    std::unique_ptr<DrmNlApi> pDrmNlApiOriginal = nullptr;
    zes_ras_error_type_t errorType;
    std::vector<DrmRasNode> originalRasNodes = MockRasNetlinkUtil::rasNodes;
    std::map<uint32_t, std::vector<DrmErrorCounter>> originalRasErrorList = MockRasNetlinkUtil::rasErrorList;

  public:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        errorType = ZES_RAS_ERROR_TYPE_CORRECTABLE;
        drmNlApi = std::make_unique<MockDrmNlApi>("testCard");
        drmNlApi->getMockErrorList = true;
        pRasNetlinkUtil = std::make_unique<MockRasNetlinkUtil>(errorType, pLinuxSysmanImp, 0u);
        pDrmNlApiOriginal = std::move(pRasNetlinkUtil->drmNl);
        pRasNetlinkUtil->drmNl = std::move(drmNlApi);
    }

    void TearDown() override {
        pRasNetlinkUtil->drmNl = std::move(pDrmNlApiOriginal);
        MockRasNetlinkUtil::rasNodes = std::move(originalRasNodes);
        MockRasNetlinkUtil::rasErrorList = std::move(originalRasErrorList);
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanRasNetlinkFixture, GivenNetlinkRasUtilInterfaceWhenCallingRasGetStateThenUnsupportedFeatureIsReturned) {
    zes_ras_state_t state = {};
    auto result = pRasNetlinkUtil->rasGetState(state, true);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanRasNetlinkFixture, GivenNetlinkRasUtilInterfaceAndLoadEntryPointsFailsWhenCallingRasGetStateExpThenErrorIsReturned) {
    auto pDrmNlApi = std::make_unique<MockDrmNlApi>("testCard");
    pDrmNlApi->getErrorsListReturnStatus = ZE_RESULT_ERROR_UNKNOWN;
    pRasNetlinkUtil->drmNl = std::move(pDrmNlApi);
    uint32_t numCategories = 1u;
    std::vector<zes_ras_state_exp_t> state(numCategories);
    auto result = pRasNetlinkUtil->rasGetStateExp(numCategories, state.data());
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanRasNetlinkFixture, GivenNetlinkRasUtilInterfaceAndValidErrorsAreFetchedWhenCallingRasGetStateExpThenSuccessAndValidErrorCountIsReturned) {
    uint32_t numCategories = 2u;
    std::vector<zes_ras_state_exp_t> state(numCategories);
    auto result = pRasNetlinkUtil->rasGetStateExp(numCategories, state.data());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    // categoryToListOfErrors is sorted by key: COMPUTE(3) first, MEMORY(7) second
    EXPECT_EQ(state[0].category, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS);
    EXPECT_EQ(state[0].errorCounter, 10u);
    EXPECT_EQ(state[1].category, ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS);
    EXPECT_EQ(state[1].errorCounter, 0u); // device_memory not in mock data
}

TEST_F(SysmanRasNetlinkFixture, GivenNetlinkRasUtilInterfaceAndValidUncorrectableErrorsAreFetchedWhenCallingRasGetStateExpThenSuccessAndValidErrorCountIsReturned) {

    auto rasNetlinkUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_UNCORRECTABLE, pLinuxSysmanImp, 0u);
    auto localDrmNlApi = std::make_unique<MockDrmNlApi>("testCard");
    localDrmNlApi->getMockErrorList = true;
    auto drmNlApiOriginal = std::move(rasNetlinkUtil->drmNl);
    rasNetlinkUtil->drmNl = std::move(localDrmNlApi);

    uint32_t numCategories = 2u;
    std::vector<zes_ras_state_exp_t> state(numCategories);
    auto result = rasNetlinkUtil->rasGetStateExp(numCategories, state.data());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    // categoryToListOfErrors is sorted by key: COMPUTE(3) first, MEMORY(7) second
    EXPECT_EQ(state[0].category, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS);
    EXPECT_EQ(state[0].errorCounter, 10u);
    EXPECT_EQ(state[1].category, ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS);
    EXPECT_EQ(state[1].errorCounter, 0u); // device_memory not in mock data
    rasNetlinkUtil->drmNl = std::move(drmNlApiOriginal);
}

TEST_F(SysmanRasNetlinkFixture, givenRasClearStateExpCallToNetlinkThenReturnsUnsupported) {
    std::unique_ptr<NetlinkRasUtil> rasNetlinkUtil(new NetlinkRasUtil(errorType, pLinuxSysmanImp, 0u));
    zes_ras_error_category_exp_t errorCat = ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS;
    auto result = rasNetlinkUtil->rasClearStateExp(errorCat);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNetlinkUtilInterfaceWhenCallingRasGetCategoryCountCategoryForCorrectableErrorsThenValidCountIsReturned) {
    std::unique_ptr<NetlinkRasUtil> rasNetlinkUtil(new NetlinkRasUtil(errorType, pLinuxSysmanImp, 0u));
    auto errorCategoryCount = rasNetlinkUtil->rasGetCategoryCount();
    EXPECT_EQ(6u, errorCategoryCount);
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNetlinkUtilInterfaceWhenCallingRasGetCategoryCountCategoryForUncorrectableErrorsThenValidCountIsReturned) {
    errorType = ZES_RAS_ERROR_TYPE_UNCORRECTABLE;
    std::unique_ptr<NetlinkRasUtil> rasNetlinkUtil(new NetlinkRasUtil(errorType, pLinuxSysmanImp, 0u));
    auto errorCategoryCount = rasNetlinkUtil->rasGetCategoryCount();
    EXPECT_EQ(6u, errorCategoryCount);
}

TEST_F(SysmanRasNetlinkFixture, GivenNetlinkRasUtilInterfaceWhenRasNodesIsEmptyAndListNodesFailsThenGetSupportedRasErrorTypesReturnsEmptySet) {
    std::set<zes_ras_error_type_t> errorType = {};
    bool isSubDevice = false;
    uint32_t subDeviceId = 0;

    MockRasNetlinkUtil::rasNodes.clear();

    MockRasNetlinkUtil::getSupportedRasErrorTypes(errorType, pLinuxSysmanImp, isSubDevice, subDeviceId);
    EXPECT_EQ(errorType.size(), 0u);
}

TEST_F(SysmanRasNetlinkFixture, GivenNetlinkRasUtilInterfaceWhenCallingGetSupportedRasErrorTypesThenSupportedErrorTypesAreReturned) {
    std::set<zes_ras_error_type_t> errorType = {};
    bool isSubDevice = false;
    uint32_t subDeviceId = 0;

    MockRasNetlinkUtil::rasNodes = {{0, "0000:00:02.0", "correctable-errors", 1},
                                    {1, "0000:00:02.0", "uncorrectable-errors", 1}};

    MockRasNetlinkUtil::getSupportedRasErrorTypes(errorType, pLinuxSysmanImp, isSubDevice, subDeviceId);
    EXPECT_EQ(errorType.size(), 2u);
    EXPECT_TRUE(errorType.contains(ZES_RAS_ERROR_TYPE_CORRECTABLE));
    EXPECT_TRUE(errorType.contains(ZES_RAS_ERROR_TYPE_UNCORRECTABLE));
}

TEST_F(SysmanRasNetlinkFixture, GivenNetlinkRasUtilInterfaceWhenRasNodesIsEmptyAndListNodesSucceedsThenGetSupportedRasErrorTypesReturnsValidTypes) {
    std::set<zes_ras_error_type_t> errorType = {};
    bool isSubDevice = false;
    uint32_t subDeviceId = 0;

    MockRasNetlinkUtil::rasNodes.clear();

    VariableBackup<decltype(NetlinkRasUtil::createDrmNlApi)> backup(&NetlinkRasUtil::createDrmNlApi);

    NetlinkRasUtil::createDrmNlApi = []() -> std::unique_ptr<DrmNlApi> {
        auto mockApi = std::make_unique<MockDrmNlApi>("testCard");
        mockApi->mockListNodesReturnData = {{0, "0000:00:02.0", "correctable-errors", 1},
                                            {1, "0000:00:02.0", "uncorrectable-errors", 1}};

        return mockApi;
    };

    MockRasNetlinkUtil::getSupportedRasErrorTypes(errorType, pLinuxSysmanImp, isSubDevice, subDeviceId);
    EXPECT_EQ(errorType.size(), 2u);
    EXPECT_TRUE(errorType.contains(ZES_RAS_ERROR_TYPE_CORRECTABLE));
    EXPECT_TRUE(errorType.contains(ZES_RAS_ERROR_TYPE_UNCORRECTABLE));
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNetlinkUtilInterfaceWhenCreatingRasUtilPtrThenValidPtrIsReturned) {
    auto rasInterfaceType = RasInterfaceType::netlink;
    bool isSubDevice = false;
    uint32_t subDeviceId = 0;
    auto pRasUtil = RasUtil::create(rasInterfaceType, pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    EXPECT_NE(pRasUtil.get(), nullptr);
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNodesWithMatchingNodeNameAndDeviceNameWhenConstructingNetlinkRasUtilThenCorrectRasNodeIdIsSet) {
    MockRasNetlinkUtil::rasNodes = {{0, "0000:00:02.0", "correctable-errors", 1},
                                    {1, "0000:00:02.0", "uncorrectable-errors", 1}};

    auto rasUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, 0u);
    EXPECT_EQ(rasUtil->rasNodeId, 0u);

    rasUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_UNCORRECTABLE, pLinuxSysmanImp, 0u);
    EXPECT_EQ(rasUtil->rasNodeId, 1u);
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNodesWithMatchingNodeNameButNonMatchingDeviceNameWhenConstructingNetlinkRasUtilThenRasNodeIdIsZero) {
    MockRasNetlinkUtil::rasNodes = {{5, "0000:00:03.0", "correctable-errors", 1},
                                    {10, "0000:00:04.0", "uncorrectable-errors", 1}};

    auto rasUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, 0u);
    EXPECT_EQ(rasUtil->rasNodeId, 0u);
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNodesWithMatchingDeviceNameButNonMatchingNodeNameWhenConstructingNetlinkRasUtilThenRasNodeIdIsZero) {
    MockRasNetlinkUtil::rasNodes = {{7, "0000:00:02.0", "nonfatal-errors", 1},
                                    {8, "0000:00:02.0", "some-other-errors", 1}};

    auto rasUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, 0u);
    EXPECT_EQ(rasUtil->rasNodeId, 0u);
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNodesWithNonMatchingNodeNameAndDeviceNameWhenConstructingNetlinkRasUtilThenRasNodeIdIsZero) {
    MockRasNetlinkUtil::rasNodes = {{11, "0000:00:05.0", "nonfatal-errors", 1},
                                    {12, "0000:00:06.0", "some-other-errors", 1}};

    auto rasUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, 0u);
    EXPECT_EQ(rasUtil->rasNodeId, 0u);
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNodesWithMatchingDeviceNameButUnknownNodeNameWhenCallingGetSupportedRasErrorTypesThenNoTypesAreAdded) {
    std::set<zes_ras_error_type_t> errorType = {};
    bool isSubDevice = false;
    uint32_t subDeviceId = 0;

    MockRasNetlinkUtil::rasNodes = {{0, "0000:00:02.0", "unknown-errors", 1}};

    MockRasNetlinkUtil::getSupportedRasErrorTypes(errorType, pLinuxSysmanImp, isSubDevice, subDeviceId);
    EXPECT_EQ(errorType.size(), 0u);
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNodesWithNonMatchingDeviceNameWhenCallingGetSupportedRasErrorTypesThenNoTypesAreAdded) {
    std::set<zes_ras_error_type_t> errorType = {};
    bool isSubDevice = false;
    uint32_t subDeviceId = 0;

    MockRasNetlinkUtil::rasNodes = {{0, "0000:00:99.0", "correctable-errors", 1},
                                    {1, "0000:00:99.0", "uncorrectable-errors", 1}};

    MockRasNetlinkUtil::getSupportedRasErrorTypes(errorType, pLinuxSysmanImp, isSubDevice, subDeviceId);
    EXPECT_EQ(errorType.size(), 0u);
}

TEST_F(SysmanRasNetlinkFixture, GivenNumCategoriesRequestedLessThanAvailableWhenCallingRasGetStateExpThenOnlyRequestedCategoriesArePopulated) {
    uint32_t numCategories = 1u;
    std::vector<zes_ras_state_exp_t> pState(numCategories);
    auto result = pRasNetlinkUtil->rasGetStateExp(numCategories, pState.data());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(pState[0].errorCounter, 10u);
}

TEST_F(SysmanRasNetlinkFixture, GivenNetlinkRasUtilInterfaceAndEmptyErrorListWhenCallingRasGetStateExpThenSuccessIsReturnedWithZeroCounters) {
    auto pDrmNlApi = std::make_unique<MockDrmNlApi>("testCard");
    pDrmNlApi->returnEmptyErrorList = true;
    pRasNetlinkUtil->drmNl = std::move(pDrmNlApi);

    uint32_t numCategories = 2u;
    std::vector<zes_ras_state_exp_t> pState(numCategories);
    auto result = pRasNetlinkUtil->rasGetStateExp(numCategories, pState.data());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(pState[0].category, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS);
    EXPECT_EQ(pState[0].errorCounter, 0u);
    EXPECT_EQ(pState[1].category, ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS);
    EXPECT_EQ(pState[1].errorCounter, 0u);
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNetlinkUtilInterfaceWhenRasErrorListCacheIsEmptyWhenCallingGetSupportedErrorCategoriesExpThenEmptyVectorIsReturned) {
    auto rasNetlinkUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, 0u);
    MockRasNetlinkUtil::rasErrorList.erase(rasNetlinkUtil->rasNodeId);
    auto categories = rasNetlinkUtil->getSupportedErrorCategoriesExp();
    EXPECT_EQ(0u, categories.size());
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNetlinkUtilInterfaceWhenCallingGetSupportedErrorCategoriesExpForCorrectableErrorsThenAllSixCategoriesAreReturned) {
    auto rasNetlinkUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, 0u);
    MockRasNetlinkUtil::rasErrorList[rasNetlinkUtil->rasNodeId] = {
        {0, "core-compute", 10, 0}, {0, "device-memory", 20, 0}, {0, "fabric", 30, 0}, {0, "scale", 40, 0}, {0, "pcie", 50, 0}, {0, "soc-internal", 60, 0}};
    auto categories = rasNetlinkUtil->getSupportedErrorCategoriesExp();
    EXPECT_EQ(6u, categories.size());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_SCALE_ERRORS) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_PCIE_ERRORS)) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_FABRIC_ERRORS)) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_SOC_INTERNAL_ERRORS)) != categories.end());
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNetlinkUtilInterfaceWhenCallingGetSupportedErrorCategoriesExpForUncorrectableErrorsThenAllSixCategoriesAreReturned) {
    auto rasNetlinkUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_UNCORRECTABLE, pLinuxSysmanImp, 0u);
    MockRasNetlinkUtil::rasErrorList[rasNetlinkUtil->rasNodeId] = {
        {0, "core-compute", 10, 0}, {0, "device-memory", 20, 0}, {0, "fabric", 30, 0}, {0, "scale", 40, 0}, {0, "pcie", 50, 0}, {0, "soc-internal", 60, 0}};
    auto categories = rasNetlinkUtil->getSupportedErrorCategoriesExp();
    EXPECT_EQ(6u, categories.size());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_SCALE_ERRORS) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_PCIE_ERRORS)) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_FABRIC_ERRORS)) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_SOC_INTERNAL_ERRORS)) != categories.end());
}

TEST_F(SysmanRasNetlinkFixture, GivenRasNetlinkUtilInterfaceWhenErrorListContainsSubsetOfCategoriesThenOnlyMatchingCategoriesAreReturned) {
    auto rasNetlinkUtil = std::make_unique<MockRasNetlinkUtil>(ZES_RAS_ERROR_TYPE_CORRECTABLE, pLinuxSysmanImp, 0u);
    MockRasNetlinkUtil::rasErrorList[rasNetlinkUtil->rasNodeId] = {
        {0, "core-compute", 10, 0},
        {0, "soc-internal", 20, 0}};
    auto categories = rasNetlinkUtil->getSupportedErrorCategoriesExp();
    EXPECT_EQ(2u, categories.size());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_SOC_INTERNAL_ERRORS)) != categories.end());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
