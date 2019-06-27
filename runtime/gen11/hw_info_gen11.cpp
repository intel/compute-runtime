/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef SUPPORT_ICLLP
#include "hw_info_icllp.inl"
#endif
#ifdef SUPPORT_LKF
#include "hw_info_lkf.inl"
#endif
#ifdef SUPPORT_EHL
#include "hw_info_ehl.inl"
#endif

namespace NEO {
const char *GfxFamilyMapper<IGFX_GEN11_CORE>::name = "Gen11";
} // namespace NEO
