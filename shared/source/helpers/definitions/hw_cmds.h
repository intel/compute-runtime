/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_GEN8
#include "shared/source/gen8/hw_cmds.h"
#endif
#ifdef SUPPORT_GEN9
#include "shared/source/gen9/hw_cmds.h"
#endif
#ifdef SUPPORT_GEN11
#include "shared/source/gen11/hw_cmds.h"
#endif
#ifdef SUPPORT_GEN12LP
#include "shared/source/gen12lp/hw_cmds.h"
#endif
#ifdef SUPPORT_XE_HP_CORE
#include "shared/source/xe_hp_core/hw_cmds.h"
#endif
#ifdef SUPPORT_XE_HPG_CORE
#include "shared/source/xe_hpg_core/hw_cmds.h"
#endif
#ifdef SUPPORT_XE_HPC_CORE
#include "shared/source/xe_hpc_core/hw_cmds.h"
#endif
