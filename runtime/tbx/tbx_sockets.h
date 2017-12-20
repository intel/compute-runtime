/*
* Copyright (c) 2017, Intel Corporation
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
#include <string>

namespace OCLRT {

class TbxSockets {
  protected:
    TbxSockets() = default;

  public:
    virtual ~TbxSockets() = default;
    virtual bool init(const std::string &hostNameOrIp, uint16_t port) = 0;
    virtual void close() = 0;

    virtual bool writeGTT(uint32_t offset, uint64_t entry) = 0;

    virtual bool readMemory(uint64_t addr, void *memory, size_t size) = 0;
    virtual bool writeMemory(uint64_t addr, const void *memory, size_t size) = 0;

    virtual bool readMMIO(uint32_t offset, uint32_t *value) = 0;
    virtual bool writeMMIO(uint32_t offset, uint32_t value) = 0;

    static TbxSockets *create();
};
} // namespace OCLRT
