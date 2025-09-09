/*
 * Copyright (C) 2024-2025 Intel Corporation
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
      {"CORE_TEMPERATURES", 0x6c}}},
    {"0x12345",
     {{"FATAL_ARRAY_BIST", 8},
      {"FATAL_IDI_PARITY", 16},
      {"ENGINE_RESET", 24},
      {"EU_ATTENTION", 32},
      {"SOC_FATAL_PSF", 40},
      {"SOC_FATAL_PUNIT", 48},
      {"FATAL_FPU", 56},
      {"FATAL_EU_GRF", 64},
      {"DRIVER_OBJECT_MIGRATION", 72},
      {"DRIVER_ENGINE_OTHER", 80},
      {"SOC_FATAL_MDFI", 88},
      {"FATAL_L3_FABRIC", 96},
      {"CORRECTABLE_L3_SNG", 104},
      {"SGUNIT_CORRECTABLE", 112},
      {"CORRECTABLE_EU", 120},
      {"CORRECTABLE_GUC", 128}}}};

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
