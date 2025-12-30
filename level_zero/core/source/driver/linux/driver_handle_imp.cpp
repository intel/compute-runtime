/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/ipc_socket_server.h"

namespace NEO {

void IpcSocketServerDeleter::operator()(IpcSocketServer *ptr) const {
    delete ptr;
}

} // namespace NEO

namespace L0 {

bool DriverHandleImp::initializeIpcSocketServer() {
    std::lock_guard<std::mutex> lock(ipcSocketServerMutex);

    if (ipcSocketServer && ipcSocketServer->isRunning()) {
        return true;
    }

    ipcSocketServer.reset(new NEO::IpcSocketServer());
    if (!ipcSocketServer->initialize()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "DriverHandleImp: Failed to initialize IPC socket server\n");
        ipcSocketServer.reset();
        return false;
    }

    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                 "DriverHandleImp: IPC socket server initialized at %s\n",
                 ipcSocketServer->getSocketPath().c_str());
    return true;
}

void DriverHandleImp::shutdownIpcSocketServer() {
    std::lock_guard<std::mutex> lock(ipcSocketServerMutex);
    if (ipcSocketServer) {
        ipcSocketServer->shutdown();
        ipcSocketServer.reset();
    }
}

bool DriverHandleImp::registerIpcHandleWithServer(uint64_t handleId, int fd) {
    std::lock_guard<std::mutex> lock(ipcSocketServerMutex);

    if (!ipcSocketServer || !ipcSocketServer->isRunning()) {
        return false;
    }

    return ipcSocketServer->registerHandle(handleId, fd);
}

bool DriverHandleImp::unregisterIpcHandleWithServer(uint64_t handleId) {
    std::lock_guard<std::mutex> lock(ipcSocketServerMutex);

    if (!ipcSocketServer || !ipcSocketServer->isRunning()) {
        return false;
    }

    return ipcSocketServer->unregisterHandle(handleId);
}

std::string DriverHandleImp::getIpcSocketServerPath() {
    std::lock_guard<std::mutex> lock(ipcSocketServerMutex);
    if (ipcSocketServer && ipcSocketServer->isRunning()) {
        return ipcSocketServer->getSocketPath();
    }
    return "";
}

} // namespace L0
