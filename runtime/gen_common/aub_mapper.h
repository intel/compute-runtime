/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_GEN8
#include "runtime/gen8/aub_mapper.h"
#endif
#ifdef SUPPORT_GEN9
#include "runtime/gen9/aub_mapper.h"
#endif
#ifdef SUPPORT_GEN11
#include "runtime/gen11/aub_mapper.h"
#endif
#ifdef SUPPORT_GEN12LP
#include "runtime/gen12lp/aub_mapper.h"
#endif
