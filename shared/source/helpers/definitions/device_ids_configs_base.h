/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifdef SUPPORT_XE_HPG_CORE
#ifdef SUPPORT_DG2
#include "device_ids_configs_dg2.h"
#endif
#endif

#if SUPPORT_XE_HPC_CORE
#ifdef SUPPORT_PVC
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#endif
#endif

#ifdef SUPPORT_XE_HP_CORE
#ifdef SUPPORT_XE_HP_SDV
#include "shared/source/xe_hp_core/xe_hp_sdv/device_ids_configs_xe_hp_sdv.h"
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

#ifdef SUPPORT_GEN11
#ifdef SUPPORT_ICLLP
#include "shared/source/gen11/icllp/device_ids_configs_icllp.h"
#endif
#ifdef SUPPORT_EHL
#include "shared/source/gen11/ehl/device_ids_configs_ehl.h"
#endif
#ifdef SUPPORT_LKF
#include "shared/source/gen11/lkf/device_ids_configs_lkf.h"
#endif
#endif

#ifdef SUPPORT_GEN9
#ifdef SUPPORT_SKL
#include "shared/source/gen9/skl/device_ids_configs_skl.h"
#endif
#ifdef SUPPORT_KBL
#include "shared/source/gen9/kbl/device_ids_configs_kbl.h"
#endif
#ifdef SUPPORT_CFL
#include "shared/source/gen9/cfl/device_ids_configs_cfl.h"
#endif
#ifdef SUPPORT_GLK
#include "shared/source/gen9/glk/device_ids_configs_glk.h"
#endif
#ifdef SUPPORT_BXT
#include "shared/source/gen9/bxt/device_ids_configs_bxt.h"
#endif
#endif

#ifdef SUPPORT_GEN8
#include "shared/source/gen8/bdw/device_ids_configs_bdw.h"
#endif
