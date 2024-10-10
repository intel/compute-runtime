/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#if SUPPORT_XE2_HPG_CORE
#ifdef SUPPORT_BMG
#include "device_ids_configs_bmg.h"
#endif
#ifdef SUPPORT_LNL
#include "shared/source/xe2_hpg_core/lnl/device_ids_configs_lnl.h"
#endif
#endif

#ifdef SUPPORT_XE_HPG_CORE
#ifdef SUPPORT_MTL
#include "shared/source/xe_hpg_core/xe_lpg/device_ids_configs_xe_lpg.h"
#endif
#endif

#if SUPPORT_XE_HPC_CORE
#ifdef SUPPORT_PVC
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#endif
#endif

#ifdef SUPPORT_GEN12LP
#ifdef SUPPORT_TGLLP
#include "shared/source/gen12lp/tgllp/device_ids_configs_tgllp.h"
#endif
#ifdef SUPPORT_DG1
#include "shared/source/gen12lp/dg1/device_ids_configs_dg1.h"
#endif
#ifdef SUPPORT_RKL
#include "shared/source/gen12lp/rkl/device_ids_configs_rkl.h"
#endif
#ifdef SUPPORT_ADLS
#include "shared/source/gen12lp/adls/device_ids_configs_adls.h"
#endif
#ifdef SUPPORT_ADLP
#include "shared/source/gen12lp/adlp/device_ids_configs_adlp.h"
#endif
#ifdef SUPPORT_ADLN
#include "shared/source/gen12lp/adln/device_ids_configs_adln.h"
#endif
#endif
