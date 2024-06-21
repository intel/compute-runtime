/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {

class MockSysmanKmdInterfacePrelim : public L0::Sysman::SysmanKmdInterfaceI915Prelim {

  public:
    using L0::Sysman::SysmanKmdInterface::pFsAccess;
    using L0::Sysman::SysmanKmdInterface::pProcfsAccess;
    using L0::Sysman::SysmanKmdInterface::pSysfsAccess;
    MockSysmanKmdInterfacePrelim(const PRODUCT_FAMILY productFamily) : SysmanKmdInterfaceI915Prelim(productFamily) {}
    ~MockSysmanKmdInterfacePrelim() override = default;
};

class MockSysmanKmdInterfaceUpstream : public L0::Sysman::SysmanKmdInterfaceI915Upstream {

  public:
    using L0::Sysman::SysmanKmdInterface::pFsAccess;
    using L0::Sysman::SysmanKmdInterface::pProcfsAccess;
    using L0::Sysman::SysmanKmdInterface::pSysfsAccess;
    MockSysmanKmdInterfaceUpstream(const PRODUCT_FAMILY productFamily) : SysmanKmdInterfaceI915Upstream(productFamily) {}
    ~MockSysmanKmdInterfaceUpstream() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
