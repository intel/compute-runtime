/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "driver_version.h"

#include <string>

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
    return NEO_OCL_DRIVER_VERSION;
#else
    return "";
#endif
}

} // namespace NEO
