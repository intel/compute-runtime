/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include <cstdint>

namespace OCLRT {

class CommandStreamReceiver;

class AUBCommandStreamFixture : public CommandStreamFixture {
  public:
    virtual void SetUp(CommandQueue *pCommandQueue);
    virtual void TearDown();

    template <typename FamilyType>
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue) {
        using AubMemDump::CmdServicesMemTraceRegisterCompare;
        CmdServicesMemTraceRegisterCompare header;
        memset(&header, 0, sizeof(header));
        header.setHeader();

        header.data[0] = expectedValue;
        header.registerOffset = mmioRegister;
        header.noReadExpect = CmdServicesMemTraceRegisterCompare::NoReadExpectValues::ReadExpect;
        header.registerSize = CmdServicesMemTraceRegisterCompare::RegisterSizeValues::Dword;
        header.registerSpace = CmdServicesMemTraceRegisterCompare::RegisterSpaceValues::Mmio;
        header.readMaskLow = 0xffffffff;
        header.readMaskHigh = 0xffffffff;
        header.dwordCount = (sizeof(header) / sizeof(uint32_t)) - 1;

        // Write our pseudo-op to the AUB file
        auto aubCsr = reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(pCommandStreamReceiver);
        aubCsr->stream->fileHandle.write(reinterpret_cast<char *>(&header), sizeof(header));
    }

    template <typename FamilyType>
    void expectMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        auto aubCsr = reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(pCommandStreamReceiver);
        PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset) {
            if (offset > length)
                abort();

            aubCsr->stream->expectMemory(physAddress,
                                         reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(srcAddress) + offset),
                                         size);
        };

        aubCsr->ppgtt->pageWalk(reinterpret_cast<uintptr_t>(gfxAddress), length, 0, walker);
    }

    CommandStreamReceiver *pCommandStreamReceiver = nullptr;
    volatile uint32_t *pTagMemory = nullptr;

  private:
    CommandQueue *commandQueue = nullptr;
};
} // namespace OCLRT
