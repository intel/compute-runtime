/*
 * Copyright (C) 2024-2025 Intel Corporation
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

const std::wstring deviceInterface = L"TEST_INTC_PMT\0";

class PublicPlatformMonitoringTech : public L0::Sysman::PlatformMonitoringTech {
  public:
    PublicPlatformMonitoringTech(SysmanProductHelper *pSysmanProductHelper, uint32_t bus, uint32_t device, uint32_t function) : PlatformMonitoringTech(pSysmanProductHelper, bus, device, function) {}
    using PlatformMonitoringTech::deviceInterface;
    using PlatformMonitoringTech::keyOffsetMap;
    using PlatformMonitoringTech::pcreateFile;
    using PlatformMonitoringTech::pdeviceIoControl;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
