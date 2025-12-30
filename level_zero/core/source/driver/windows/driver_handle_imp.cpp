/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace NEO {

void IpcSocketServerDeleter::operator()(IpcSocketServer *ptr) const {
    (void)ptr;
}

} // namespace NEO

namespace L0 {

bool DriverHandleImp::initializeIpcSocketServer() {
    return false;
}

void DriverHandleImp::shutdownIpcSocketServer() {
}

bool DriverHandleImp::registerIpcHandleWithServer(uint64_t handleId, int fd) {
    return false;
}

bool DriverHandleImp::unregisterIpcHandleWithServer(uint64_t handleId) {
    return false;
}

std::string DriverHandleImp::getIpcSocketServerPath() {
    return "";
}

} // namespace L0
