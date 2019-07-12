/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/hw_info_config.inl"
#include "runtime/os_interface/hw_info_config_bdw_plus.inl"

#ifdef SUPPORT_ICLLP
#include "hw_info_config_icllp.inl"
#endif
#ifdef SUPPORT_LKF
#include "hw_info_config_lkf.inl"
#endif
