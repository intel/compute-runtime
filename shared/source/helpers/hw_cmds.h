/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_GEN8
#include "gen8/hw_cmds.h"
#endif
#ifdef SUPPORT_GEN9
#include "gen9/hw_cmds.h"
#endif
#ifdef SUPPORT_GEN11
#include "gen11/hw_cmds.h"
#endif
#ifdef SUPPORT_GEN12LP
#include "gen12lp/hw_cmds.h"
#endif
