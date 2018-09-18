/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cif/common/cif_main.h"
#include "cif/common/library_api.h"

namespace OCLRT {
#if defined(__clang__)
__attribute__((no_sanitize("undefined")))
#endif
CIF::CIFMain *
createMainNoSanitize(CIF::CreateCIFMainFunc_t createFunc) {
    return createFunc();
}
} // namespace OCLRT
