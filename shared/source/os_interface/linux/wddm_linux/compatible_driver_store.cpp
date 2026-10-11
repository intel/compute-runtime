/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string>

typedef unsigned int D3DKMT_HANDLE;

namespace NEO {

class Gdi;

bool isCompatibleDriverStore(const Gdi &gdi, D3DKMT_HANDLE adapter) {
    return false;
}

} // namespace NEO
