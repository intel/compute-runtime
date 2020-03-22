/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "driver_version.h"

#ifdef QTR
#undef QTR
#endif

#ifdef TOSTR
#undef TOSTR
#endif

#define QTR(a) #a
#define TOSTR(b) QTR(b)

namespace NEO {
constexpr const char *driverVersion = TOSTR(NEO_OCL_DRIVER_VERSION);
}

#undef QTR
#undef TOSTR
