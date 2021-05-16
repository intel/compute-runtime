/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_plus.inl"

namespace NEO {

#ifdef SUPPORT_ICLLP
template class HwInfoConfigHw<IGFX_ICELAKE_LP>;
#endif

#ifdef SUPPORT_LKF
template class HwInfoConfigHw<IGFX_LAKEFIELD>;
#endif

#ifdef SUPPORT_EHL
template class HwInfoConfigHw<IGFX_ELKHARTLAKE>;
#endif
} // namespace NEO
