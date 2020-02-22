/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_GEN8
#include "gen8/aub_mapper.h"
#endif
#ifdef SUPPORT_GEN9
#include "gen9/aub_mapper.h"
#endif
#ifdef SUPPORT_GEN11
#include "gen11/aub_mapper.h"
#endif
#ifdef SUPPORT_GEN12LP
#include "gen12lp/aub_mapper.h"
#endif
