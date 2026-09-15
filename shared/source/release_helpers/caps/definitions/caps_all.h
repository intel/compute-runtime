/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifdef SUPPORT_GEN12LP
#include "shared/source/release_helpers/caps/caps_gen12lp.h"
#endif

#ifdef SUPPORT_XE_HPG_CORE
#include "shared/source/release_helpers/caps/caps_xe_hpg.h"
#include "shared/source/release_helpers/caps/caps_xe_lpg.h"
#endif

#ifdef SUPPORT_XE_HPC_CORE
#include "shared/source/release_helpers/caps/caps_xe_hpc.h"
#endif

#ifdef SUPPORT_XE2_HPG_CORE
#include "shared/source/release_helpers/caps/caps_xe2_hpg.h"
#endif

#ifdef SUPPORT_XE3_CORE
#include "shared/source/release_helpers/caps/caps_xe3.h"
#endif

#ifdef SUPPORT_XE3P_CORE
#include "shared/source/release_helpers/caps/caps_xe3p.h"
#endif
