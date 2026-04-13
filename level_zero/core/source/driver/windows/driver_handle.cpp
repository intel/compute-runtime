/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle.h"

#include "shared/source/helpers/debug_helpers.h"

namespace NEO {

void IpcSocketServerDeleter::operator()(IpcSocketServer *ptr) const {
    (void)ptr;
}

} // namespace NEO

namespace L0 {

bool DriverHandle::initializeIpcSocketServer() {
    return false;
}

void DriverHandle::shutdownIpcSocketServer() {
}

bool DriverHandle::registerIpcHandleWithServer(uint64_t handleId, int fd) {
    return false;
}

bool DriverHandle::unregisterIpcHandleWithServer(uint64_t handleId) {
    return false;
}

std::string DriverHandle::getIpcSocketServerPath() {
    return "";
}

int DriverHandle::duplicateFd(int fd) {
    DEBUG_BREAK_IF(true);
    return fd;
}

void DriverHandle::closeFd(int fd) {
    DEBUG_BREAK_IF(true);
}

} // namespace L0
