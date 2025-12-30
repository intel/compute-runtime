/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/ipc_socket_server.h"

#include <string>

namespace NEO {

class IpcSocketClient {
  public:
    IpcSocketClient();
    ~IpcSocketClient();

    bool connectToServer(const std::string &socketPath);
    void disconnect();
    int requestHandle(uint64_t handleId);

    bool isConnected() const { return clientSocket != -1; }

  protected:
    int receiveFileDescriptor(void *data, size_t dataSize);

    bool sendMessage(const IpcSocketMessage &msg, const void *payload = nullptr);
    bool receiveMessage(IpcSocketMessage &msg, void *payload = nullptr, size_t maxPayloadSize = 0);

  private:
    int clientSocket = -1;
    std::string serverSocketPath;

    static constexpr int socketTimeout = 5000; // 5 seconds
};

} // namespace NEO