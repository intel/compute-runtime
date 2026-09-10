/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {
class MockSysmanKmdInterfaceXe : public L0::Sysman::SysmanKmdInterfaceXe {

  public:
    using L0::Sysman::SysmanKmdInterface::pFsAccess;
    using L0::Sysman::SysmanKmdInterface::pProcfsAccess;
    using L0::Sysman::SysmanKmdInterface::pSysfsAccess;
    using L0::Sysman::SysmanKmdInterfaceXe::nodeNameToFileMap;
    MockSysmanKmdInterfaceXe(SysmanProductHelper *pSysmanProductHelper) : SysmanKmdInterfaceXe(pSysmanProductHelper) {}
    ~MockSysmanKmdInterfaceXe() override = default;

    bool mockNodeFileNameUnavailable = false;
    std::string getNodeFileName(NodeName nodeName) override {
        if (mockNodeFileNameUnavailable) {
            return {};
        }
        return SysmanKmdInterfaceXe::getNodeFileName(nodeName);
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
