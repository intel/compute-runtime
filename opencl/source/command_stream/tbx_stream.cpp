/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "opencl/source/aub/aub_helper.h"
#include "opencl/source/command_stream/tbx_command_stream_receiver.h"
#include "opencl/source/tbx/tbx_sockets.h"

namespace NEO {

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
    uint32_t type = AubHelper::getMemType(addressSpace);
    socket->writeMemory(addr, memory, size, type);
}

void TbxStream::writeGTT(uint32_t gttOffset, uint64_t entry) {
    socket->writeGTT(gttOffset, entry);
}

void TbxStream::writePTE(uint64_t physAddress, uint64_t entry, uint32_t addressSpace) {
    uint32_t type = AubHelper::getMemType(addressSpace);
    socket->writeMemory(physAddress, &entry, sizeof(entry), type);
}

void TbxStream::writeMemoryWriteHeader(uint64_t physAddress, size_t size, uint32_t addressSpace, uint32_t hint) {
}

void TbxStream::writeMMIOImpl(uint32_t offset, uint32_t value) {
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

} // namespace NEO
