/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
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
