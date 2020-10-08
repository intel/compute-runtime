/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_plus.inl"

#ifdef SUPPORT_TGLLP
#include "hw_info_config_tgllp.inl"
#endif
#ifdef SUPPORT_DG1
#include "hw_info_config_dg1.inl"
#endif
#ifdef SUPPORT_RKL
#include "hw_info_config_rkl.inl"
#endif
#ifdef SUPPORT_ADLS
#include "hw_info_config_adls.inl"
#endif