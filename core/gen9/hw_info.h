/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_SKL
#include "hw_info_skl.h"
#endif
#ifdef SUPPORT_KBL
#include "hw_info_kbl.h"
#endif
#ifdef SUPPORT_BXT
#include "hw_info_bxt.h"
#endif
#ifdef SUPPORT_GLK
#include "hw_info_glk.h"
#endif
#ifdef SUPPORT_CFL
#include "hw_info_cfl.h"
#endif
