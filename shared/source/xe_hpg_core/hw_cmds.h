/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#ifdef SUPPORT_DG2
#include "hw_cmds_dg2.h"
#endif

#ifdef SUPPORT_MTL
#include "hw_cmds_mtl.h"
#endif

#ifdef SUPPORT_ARL
#include "hw_cmds_arl.h"
#endif
