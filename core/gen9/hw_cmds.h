/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_SKL
#include "hw_cmds_skl.h"
#endif
#ifdef SUPPORT_KBL
#include "hw_cmds_kbl.h"
#endif
#ifdef SUPPORT_BXT
#include "hw_cmds_bxt.h"
#endif
#ifdef SUPPORT_GLK
#include "hw_cmds_glk.h"
#endif
#ifdef SUPPORT_CFL
#include "hw_cmds_cfl.h"
#endif
