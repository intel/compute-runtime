/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace L0 {

class ContextExt;
struct DriverHandle;

ContextExt *createContextExt(DriverHandle *driverHandle) {
    return nullptr;
}

void destroyContextExt(ContextExt *ctxExt) {
    return;
}

} // namespace L0
