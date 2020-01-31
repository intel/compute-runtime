/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/hw_info_config.inl"
#include "core/os_interface/hw_info_config_bdw_plus.inl"

#ifdef SUPPORT_BXT
#include "hw_info_config_bxt.inl"
#endif
#ifdef SUPPORT_CFL
#include "hw_info_config_cfl.inl"
#endif
#ifdef SUPPORT_GLK
#include "hw_info_config_glk.inl"
#endif
#ifdef SUPPORT_KBL
#include "hw_info_config_kbl.inl"
#endif
#ifdef SUPPORT_SKL
#include "hw_info_config_skl.inl"
#endif
