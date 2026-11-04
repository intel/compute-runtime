/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/shared_section.h"

namespace NEO {

bool SharedSection::isSection(HANDLE handle) {
    return false;
}

bool SharedSection::initialize() {
    return false;
}

} // namespace NEO
