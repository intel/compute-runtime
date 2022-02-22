/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifdef SUPPORT_XE_HPG_CORE
#ifdef SUPPORT_DG2
#include "shared/source/xe_hpg_core/definitions/device_ids_configs_dg2.h"
#endif
#endif

#if SUPPORT_XE_HPC_CORE
#ifdef SUPPORT_PVC
#include "shared/source/xe_hpc_core/definitions/device_ids_configs_pvc.h"
#endif
#endif
