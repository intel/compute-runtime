/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_GEN12LP
#include "shared/source/gen12lp/aub_mapper.h"
#endif
#ifdef SUPPORT_XE_HPG_CORE
#include "shared/source/xe_hpg_core/aub_mapper.h"
#endif
#ifdef SUPPORT_XE_HPC_CORE
#include "shared/source/xe_hpc_core/aub_mapper.h"
#endif
#ifdef SUPPORT_XE2_HPG_CORE
#include "shared/source/xe2_hpg_core/aub_mapper.h"
#endif
