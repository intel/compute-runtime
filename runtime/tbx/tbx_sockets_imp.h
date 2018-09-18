/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/tbx/tbx_sockets.h"
#include "os_socket.h"
#include <iostream>

namespace OCLRT {

class TbxSocketsImp : public TbxSockets {
  public:
    TbxSocketsImp(std::ostream &err = std::cerr);
    ~TbxSocketsImp() override = default;

    bool init(const std::string &hostNameOrIp, uint16_t port) override;
    void close() override;

    bool writeGTT(uint32_t gttOffset, uint64_t entry) override;

    bool readMemory(uint64_t offset, void *data, size_t size) override;
    bool writeMemory(uint64_t offset, const void *data, size_t size) override;

    bool readMMIO(uint32_t offset, uint32_t *data) override;
    bool writeMMIO(uint32_t offset, uint32_t data) override;

  protected:
    std::ostream &cerrStream;
    SOCKET m_socket = 0;

    bool connectToServer(const std::string &hostNameOrIp, uint16_t port);
    bool sendWriteData(const void *buffer, size_t sizeInBytes);
    bool getResponseData(void *buffer, size_t sizeInBytes);

    inline uint32_t getNextTransID() { return transID++; }

    void logErrorInfo(const char *tag);

    uint32_t transID = 0;
};
} // namespace OCLRT
