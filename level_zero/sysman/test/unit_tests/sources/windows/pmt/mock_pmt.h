/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/os_interface/windows/mock_sys_calls.h"

#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"

namespace L0 {
namespace Sysman {
namespace ult {

class PublicPlatformMonitoringTech : public L0::Sysman::PlatformMonitoringTech {
  public:
    PublicPlatformMonitoringTech(std::vector<wchar_t> deviceInterfaceList, SysmanProductHelper *pSysmanProductHelper) : PlatformMonitoringTech(deviceInterfaceList, pSysmanProductHelper) {}
    using PlatformMonitoringTech::keyOffsetMap;
    using PlatformMonitoringTech::pcreateFile;
    using PlatformMonitoringTech::pdeviceIoControl;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
