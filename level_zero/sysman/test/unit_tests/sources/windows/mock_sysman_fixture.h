/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/default_hw_info.h"

#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware_util/mock_fw_util_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_wddm.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace L0 {
namespace Sysman {
namespace ult {

class PublicWddmSysmanImp : public L0::Sysman::WddmSysmanImp {
  public:
    using WddmSysmanImp::pFwUtilInterface;
    using WddmSysmanImp::pKmdSysManager;
    using WddmSysmanImp::pPmt;
    using WddmSysmanImp::pSysmanProductHelper;
};

class SysmanDeviceFixture : public ::testing::Test {
  public:
    void SetUp() override {

        execEnv = new NEO::ExecutionEnvironment();
        execEnv->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < execEnv->rootDeviceEnvironments.size(); i++) {
            execEnv->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            execEnv->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            execEnv->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::make_unique<SysmanMockWddm>(*execEnv->rootDeviceEnvironments[i]));
        }

        driverHandle = std::make_unique<L0::Sysman::SysmanDriverHandleImp>();
        driverHandle->initialize(*execEnv);
        pSysmanDevice = driverHandle->sysmanDevices[0];
        L0::Sysman::globalSysmanDriver = driverHandle.get();

        L0::Sysman::sysmanOnlyInit = true;

        pSysmanDeviceImp = static_cast<L0::Sysman::SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pWddmSysmanImp = static_cast<PublicWddmSysmanImp *>(pOsSysman);
        pWddmSysmanImp->pFwUtilInterface = new MockFwUtilInterface();
    }

    void TearDown() override {
        L0::Sysman::sysmanOnlyInit = false;
        L0::Sysman::globalSysmanDriver = nullptr;
    }

    L0::Sysman::SysmanDevice *pSysmanDevice = nullptr;
    L0::Sysman::SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    L0::Sysman::OsSysman *pOsSysman = nullptr;
    PublicWddmSysmanImp *pWddmSysmanImp = nullptr;
    NEO::ExecutionEnvironment *execEnv = nullptr;
    std::unique_ptr<L0::Sysman::SysmanDriverHandleImp> driverHandle;
    const uint32_t numRootDevices = 1u;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
