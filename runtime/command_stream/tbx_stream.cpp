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

#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/tbx/tbx_sockets.h"

namespace OCLRT {

TbxStream::TbxStream() {
}

TbxStream::~TbxStream() {
    delete socket;
}

void TbxStream::open(const char *options) {
}

void TbxStream::close() {
    DEBUG_BREAK_IF(!socket);
    socket->close();
}

bool TbxStream::init(uint32_t stepping, uint32_t device) {
    socket = TbxSockets::create();
    DEBUG_BREAK_IF(!socket);
    auto tbxServer = DebugManager.flags.TbxServer.get();
    auto tbxPort = DebugManager.flags.TbxPort.get();
    return socket->init(tbxServer, tbxPort);
}

void TbxStream::writeMemory(uint64_t addr, const void *memory, size_t size, uint32_t addressSpace, uint32_t hint) {
    socket->writeMemory(addr, memory, size);
}

void TbxStream::writeGTT(uint32_t gttOffset, uint64_t entry) {
    socket->writeGTT(gttOffset, entry);
}

void TbxStream::writePTE(uint64_t physAddress, uint64_t entry) {
    socket->writeMemory(physAddress, &entry, sizeof(entry));
}

void TbxStream::writeMemoryWriteHeader(uint64_t physAddress, size_t size, uint32_t addressSpace, uint32_t hint) {
}

void TbxStream::writeMMIO(uint32_t offset, uint32_t value) {
    socket->writeMMIO(offset, value);
}

void TbxStream::registerPoll(uint32_t registerOffset, uint32_t mask, uint32_t desiredValue, bool pollNotEqual, uint32_t timeoutAction) {
    bool matches = false;
    bool asyncMMIO = false;
    do {
        uint32_t value;
        socket->readMMIO(registerOffset, &value);

        matches = ((value & mask) == desiredValue);
    } while (matches == pollNotEqual && asyncMMIO);
}

void TbxStream::readMemory(uint64_t physAddress, void *memory, size_t size) {
    socket->readMemory(physAddress, memory, size);
}

} // namespace OCLRT
