/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_plus.inl"

namespace NEO {

#ifdef SUPPORT_BXT
template class HwInfoConfigHw<IGFX_BROXTON>;
#endif

#ifdef SUPPORT_CFL
template class HwInfoConfigHw<IGFX_COFFEELAKE>;
#endif

#ifdef SUPPORT_GLK
template class HwInfoConfigHw<IGFX_GEMINILAKE>;
#endif

#ifdef SUPPORT_KBL
template class HwInfoConfigHw<IGFX_KABYLAKE>;
#endif

#ifdef SUPPORT_SKL
template class HwInfoConfigHw<IGFX_SKYLAKE>;
#endif

} // namespace NEO
