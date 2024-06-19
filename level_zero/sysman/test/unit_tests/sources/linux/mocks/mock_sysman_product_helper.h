/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockSysmanProductHelper : public L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
    MockSysmanProductHelper() = default;
    ADDMETHOD_NOBASE(isFrequencySetRangeSupported, bool, false, ());
    ADDMETHOD_NOBASE(isPowerSetLimitSupported, bool, false, ());
};

} // namespace ult
} // namespace Sysman
} // namespace L0
