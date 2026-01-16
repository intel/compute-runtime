/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace L0 {

class ContextExt;
class DriverHandle;

ContextExt *createContextExt(DriverHandle *driverHandle) {
    return nullptr;
}

void destroyContextExt(ContextExt *ctxExt) {
    return;
}

} // namespace L0
