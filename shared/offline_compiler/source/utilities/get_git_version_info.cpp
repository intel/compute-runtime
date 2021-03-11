/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "driver_version.h"

#include <string>

#ifdef QTR
#undef QTR
#endif

#ifdef TOSTR
#undef TOSTR
#endif

#define QTR(a) #a
#define TOSTR(b) QTR(b)

namespace NEO {

std::string getRevision() {
#ifdef NEO_REVISION
    return NEO_REVISION;
#else
    return "";
#endif
}

std::string getOclDriverVersion() {
#ifdef NEO_OCL_DRIVER_VERSION
    return TOSTR(NEO_OCL_DRIVER_VERSION);
#else
    return "";
#endif
}

} // namespace NEO
