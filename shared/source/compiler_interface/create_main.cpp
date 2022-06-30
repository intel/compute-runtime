/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cif/common/cif_main.h"
#include "cif/common/library_api.h"

namespace NEO {
#if defined(__clang__)
__attribute__((no_sanitize("undefined")))
#endif
CIF::CIFMain *
createMainNoSanitize(CIF::CreateCIFMainFunc_t createFunc) {
    return createFunc();
}
} // namespace NEO
