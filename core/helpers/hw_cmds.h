/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_GEN8
#include "core/gen8/hw_cmds.h"
#endif
#ifdef SUPPORT_GEN9
#include "core/gen9/hw_cmds.h"
#endif
#ifdef SUPPORT_GEN11
#include "core/gen11/hw_cmds.h"
#endif
#ifdef SUPPORT_GEN12LP
#include "core/gen12lp/hw_cmds.h"
#endif
