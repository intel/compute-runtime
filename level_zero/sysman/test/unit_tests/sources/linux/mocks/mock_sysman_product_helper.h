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

static std::map<std::string, std::map<std::string, uint64_t>> mockGuidToKeyOffsetMap = {
    {"0x490e01",
     {{"PACKAGE_ENERGY", 0x420},
      {"COMPUTE_TEMPERATURES", 0x68},
      {"SOC_TEMPERATURES", 0x60},
      {"CORE_TEMPERATURES", 0x6c}}}};

struct MockSysmanProductHelper : public L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
    MockSysmanProductHelper() = default;
    std::map<std::string, std::map<std::string, uint64_t>> *getGuidToKeyOffsetMap() override { return &mockGuidToKeyOffsetMap; }
    ADDMETHOD_NOBASE(isFrequencySetRangeSupported, bool, false, ());
    ADDMETHOD_NOBASE(isPowerSetLimitSupported, bool, false, ());
    ADDMETHOD_NOBASE(isUpstreamPortConnected, bool, true, ());

    ze_result_t mockGetPciStatsResult = ZE_RESULT_SUCCESS;
    ze_result_t getPciStats(zes_pci_stats_t *pStats, LinuxSysmanImp *pLinuxSysmanImp) override {
        return mockGetPciStatsResult;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
