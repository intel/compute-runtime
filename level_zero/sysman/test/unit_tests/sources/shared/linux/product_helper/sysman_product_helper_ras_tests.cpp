/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/ras/linux/mock_sysman_ras.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperRasTest = SysmanDeviceFixture;

HWTEST2_F(SysmanProductHelperRasTest, GivenSysmanProductHelperInstanceWhenQueryingRasInterfaceThenVerifyProperInterfacesAreReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(RasInterfaceType::pmu, pSysmanProductHelper->getGtRasUtilInterface());
    EXPECT_EQ(RasInterfaceType::gsc, pSysmanProductHelper->getHbmRasUtilInterface());
}

HWTEST2_F(SysmanProductHelperRasTest, GivenSysmanProductHelperInstanceWhenQueryingRasInterfaceThenVerifyProperInterfacesAreReturned, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(RasInterfaceType::pmu, pSysmanProductHelper->getGtRasUtilInterface());
    EXPECT_EQ(RasInterfaceType::none, pSysmanProductHelper->getHbmRasUtilInterface());
}

HWTEST2_F(SysmanProductHelperRasTest, GivenValidRasHandleWhenRequestingCountWithRasGetStateExpThenCountIsNotZero, IsPVC) {
    bool isSubDevice = true;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    pLinuxRasImp->rasSources.clear();
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceGt>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceHbm>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetStateExp(&count, nullptr));
    EXPECT_EQ(4u, count);
}

HWTEST2_F(SysmanProductHelperRasTest, GivenValidRasHandleWhenRequestingCountWithRasGetStateExpThenCountIsNotZero, IsDG1) {
    bool isSubDevice = true;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    pLinuxRasImp->rasSources.clear();
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceGt>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceHbm>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetStateExp(&count, nullptr));
    EXPECT_EQ(3u, count);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
