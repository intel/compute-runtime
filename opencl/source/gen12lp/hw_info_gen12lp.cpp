/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef SUPPORT_TGLLP
#include "hw_info_tgllp.inl"
#endif
#ifdef SUPPORT_DG1
#include "hw_info_dg1.inl"
#endif
#ifdef SUPPORT_RKL
#include "hw_info_rkl.inl"
#endif
#ifdef SUPPORT_ADLS
#include "hw_info_adls.inl"
#endif

#include "shared/source/gen12lp/hw_info_gen12lp.h"

namespace NEO {
const char *GfxFamilyMapper<IGFX_GEN12LP_CORE>::name = "Gen12LP";
} // namespace NEO
