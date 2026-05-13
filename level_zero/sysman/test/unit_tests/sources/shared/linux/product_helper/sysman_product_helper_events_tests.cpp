/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/events/linux/sysman_os_events_imp.h"
#include "level_zero/sysman/source/api/events/linux/sysman_os_events_netlink.h"
#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_ras_types.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_nl_api.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_sysman_drm_nl_api.h"

namespace L0 {
namespace Sysman {
namespace ult {

class MockDrmNlApiForProductHelper : public MockDrmNlApi {
  public:
    MockDrmNlApiForProductHelper(std::string devId) : MockDrmNlApi(devId) {
        pNlApi = std::make_unique<MockNlApi>();
        mockNlApi = static_cast<MockNlApi *>(pNlApi.get());
        mockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
        mockNlApi->mockNlSocketAllocReturnValue.push_back(reinterpret_cast<struct nl_sock *>(0x1234));
        mockNlApi->mockNlSocketAllocReturnValue.push_back(reinterpret_cast<struct nl_sock *>(0x5678));
        mockNlApi->mockGenlConnectReturnValue.push_back(0);
        mockNlApi->mockGenCtrlResolveReturnValue.push_back(42);
        mockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
        mockNlApi->mockGenlOpsResolveReturnValue.push_back(0);
        mockNlApi->mockGenlCtrlResolveGrpReturnValue.push_back(100);
        mockNlApi->mockNlSocketAddMembershipReturnValue.push_back(0);
        mockNlApi->mockNlSocketSetNonblockingReturnValue.push_back(0);
        mockNlApi->mockNlSocketGetFdReturnValue.push_back(42);
        isInitDone = true;
    }

    ~MockDrmNlApiForProductHelper() override = default;

    ze_result_t subscribeReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t listNodesReturnStatus = ZE_RESULT_SUCCESS;
    std::vector<DrmRasNode> mockListNodesReturnData;
    MockNlApi *mockNlApi = nullptr;

    ze_result_t subscribeToEvents() override {
        return subscribeReturnStatus;
    }

    ze_result_t listNodes(std::vector<DrmRasNode> &nodes) override {
        if (listNodesReturnStatus == ZE_RESULT_SUCCESS) {
            nodes = mockListNodesReturnData;
        }
        return listNodesReturnStatus;
    }
};

class PublicLinuxSysmanDriverImpForProductHelper : public L0::Sysman::LinuxSysmanDriverImp {
  public:
    using LinuxSysmanDriverImp::pDrmNl;
};

class MockNetlinkRasUtilForProductHelper : public NetlinkRasUtil {
  public:
    using NetlinkRasUtil::rasNodes;
    MockNetlinkRasUtilForProductHelper(zes_ras_error_type_t type, L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId)
        : NetlinkRasUtil(type, pLinuxSysmanImp, subdeviceId) {}
    ~MockNetlinkRasUtilForProductHelper() override = default;
};

class SysmanProductHelperEventsTest : public SysmanDeviceFixture {
  protected:
    std::vector<DrmRasNode> originalRasNodes;

  public:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        originalRasNodes = MockNetlinkRasUtilForProductHelper::rasNodes;
        MockNetlinkRasUtilForProductHelper::rasNodes.clear();
    }

    void TearDown() override {
        MockNetlinkRasUtilForProductHelper::rasNodes = std::move(originalRasNodes);
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(SysmanProductHelperEventsTest, GivenNetlinkInitializeSucceedsWhenInitIsCalledThenPDrmNlIsNotNullOnCRI, IsCRI) {
    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForProductHelper>();
    auto pMockDrmNl = std::make_unique<MockDrmNlApiForProductHelper>("testCard");

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1},
        {1, pLinuxSysmanImp->getDevicePciBdf(), "uncorrectable-errors", 1}};

    pMockDrmNl->mockListNodesReturnData = rasNodes;
    pMockDrmNl->listNodesReturnStatus = ZE_RESULT_SUCCESS;
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pPublicLinuxSysmanDriverImp->pDrmNl = pMockDrmNl.release();

    L0::Sysman::OsSysmanDriver *pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = pPublicLinuxSysmanDriverImp.get();

    DrmNlApi *pDrmNl = nullptr;
    ze_result_t result = netlinkInitialize(pPublicLinuxSysmanDriverImp.get(), pDrmNl, pSysmanDeviceImp);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(pDrmNl, nullptr);
    EXPECT_EQ(pDrmNl, pPublicLinuxSysmanDriverImp->getDrmNlApiHandle());
    EXPECT_FALSE(MockNetlinkRasUtilForProductHelper::rasNodes.empty());
    EXPECT_EQ(MockNetlinkRasUtilForProductHelper::rasNodes.size(), 2u);

    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp->pDrmNl;
    pPublicLinuxSysmanDriverImp->pDrmNl = nullptr;
}

HWTEST2_F(SysmanProductHelperEventsTest, GivenNetlinkInitializeFailsWhenListNodesFailsThenErrorIsReturnedOnCRI, IsCRI) {

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForProductHelper>();
    auto pMockDrmNl = std::make_unique<MockDrmNlApiForProductHelper>("testCard");
    pMockDrmNl->listNodesReturnStatus = ZE_RESULT_ERROR_UNKNOWN;
    pPublicLinuxSysmanDriverImp->pDrmNl = pMockDrmNl.release();

    L0::Sysman::OsSysmanDriver *pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = pPublicLinuxSysmanDriverImp.get();

    DrmNlApi *pDrmNl = nullptr;
    ze_result_t result = netlinkInitialize(pPublicLinuxSysmanDriverImp.get(), pDrmNl, pSysmanDeviceImp);

    EXPECT_NE(result, ZE_RESULT_SUCCESS);

    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp->pDrmNl;
    pPublicLinuxSysmanDriverImp->pDrmNl = nullptr;
}

HWTEST2_F(SysmanProductHelperEventsTest, GivenNetlinkInitializeFailsWhenSubscribeFailsThenErrorIsReturnedOnCRI, IsCRI) {
    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForProductHelper>();
    auto pMockDrmNl = std::make_unique<MockDrmNlApiForProductHelper>("testCard");

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};

    pMockDrmNl->mockListNodesReturnData = rasNodes;
    pMockDrmNl->listNodesReturnStatus = ZE_RESULT_SUCCESS;
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_ERROR_UNKNOWN;
    pPublicLinuxSysmanDriverImp->pDrmNl = pMockDrmNl.release();

    L0::Sysman::OsSysmanDriver *pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = pPublicLinuxSysmanDriverImp.get();

    DrmNlApi *pDrmNl = nullptr;
    ze_result_t result = netlinkInitialize(pPublicLinuxSysmanDriverImp.get(), pDrmNl, pSysmanDeviceImp);

    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(pDrmNl, nullptr);

    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;

    delete pPublicLinuxSysmanDriverImp->pDrmNl;
    pPublicLinuxSysmanDriverImp->pDrmNl = nullptr;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
