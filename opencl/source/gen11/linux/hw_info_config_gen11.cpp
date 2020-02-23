/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_plus.inl"

#ifdef SUPPORT_ICLLP
#include "hw_info_config_icllp.inl"
#endif
#ifdef SUPPORT_LKF
#include "hw_info_config_lkf.inl"
#endif
#ifdef SUPPORT_EHL
#include "hw_info_config_ehl.inl"
#endif
