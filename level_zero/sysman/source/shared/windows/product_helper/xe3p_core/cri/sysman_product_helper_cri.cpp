/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.inl"

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_CRI;

#include "level_zero/sysman/source/shared/product_helper/sysman_os_agnostic_product_helper_xe2_and_later.inl"

static std::map<unsigned long, std::map<std::string, uint32_t>> guidToKeyOffsetMap = {
    {0x1e2fa030, {}}, // CRI PUNIT rev 0
    {0x5e2fa230, {}}, // CRI OOBMSM Rev 0
    {0x5e2fa270, {}}  // CRI GFSP Rev 0
};

template <>
std::map<unsigned long, std::map<std::string, uint32_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

template <>
uint32_t SysmanProductHelperHw<gfxProduct>::getPmtMaxInterfaces() {
    return 3u; // PUNIT, OOBMSM, GFSP
}

template class SysmanProductHelperHw<gfxProduct>;

static EnableSysmanProductHelper<gfxProduct> enableCri;

} // namespace Sysman
} // namespace L0
