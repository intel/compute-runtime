/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_GEN8
#include "shared/source/gen8/aub_mapper.h"
#endif
#ifdef SUPPORT_GEN9
#include "shared/source/gen9/aub_mapper.h"
#endif
#ifdef SUPPORT_GEN11
#include "shared/source/gen11/aub_mapper.h"
#endif
#ifdef SUPPORT_GEN12LP
#include "shared/source/gen12lp/aub_mapper.h"
#endif
#ifdef SUPPORT_XE_HP_CORE
#include "shared/source/xe_hp_core/aub_mapper.h"
#endif
#ifdef SUPPORT_XE_HPG_CORE
#include "shared/source/xe_hpg_core/aub_mapper.h"
#endif
#ifdef SUPPORT_XE_HPC_CORE
#include "shared/source/xe_hpc_core/aub_mapper.h"
#endif
