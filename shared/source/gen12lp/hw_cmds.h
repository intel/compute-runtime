/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gen12lp/hw_cmds_base.h"
#ifdef SUPPORT_TGLLP
#include "hw_cmds_tgllp.h"
#include "hw_info_tgllp.h"
#endif
#ifdef SUPPORT_DG1
#include "hw_cmds_dg1.h"
#include "hw_info_dg1.h"
#endif
#ifdef SUPPORT_RKL
#include "hw_cmds_rkl.h"
#include "hw_info_rkl.h"
#endif
#ifdef SUPPORT_ADLS
#include "hw_cmds_adls.h"
#include "hw_info_adls.h"
#endif
#ifdef SUPPORT_ADLP
#include "hw_cmds_adlp.h"
#include "hw_info_adlp.h"
#endif
#ifdef SUPPORT_ADLN
#include "hw_cmds_adln.h"
#include "hw_info_adln.h"
#endif
