/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/aub_mem_dump/aub_mem_dump.h"

namespace NEO {
class CommandStreamReceiver;
class TbxSockets;
class ExecutionEnvironment;

class TbxStream : public AubMemDump::AubStream {
  protected:
    TbxSockets *socket = nullptr;

  public:
    TbxStream();
    ~TbxStream() override;

    TbxStream(const TbxStream &) = delete;
    TbxStream &operator=(const TbxStream &) = delete;

    void open(const char *options) override;
    void close() override;
    bool init(uint32_t stepping, uint32_t device) override;
    void writeMemory(uint64_t physAddress, const void *memory, size_t size, uint32_t addressSpace, uint32_t hint) override;
    void writeMemoryWriteHeader(uint64_t physAddress, size_t size, uint32_t addressSpace, uint32_t hint) override;
    void writeGTT(uint32_t gttOffset, uint64_t entry) override;
    void writePTE(uint64_t physAddress, uint64_t entry, uint32_t addressSpace) override;
    void writeMMIOImpl(uint32_t offset, uint32_t value) override;
    void registerPoll(uint32_t registerOffset, uint32_t mask, uint32_t value, bool pollNotEqual, uint32_t timeoutAction) override;
    void readMemory(uint64_t physAddress, void *memory, size_t size);
};

struct TbxCommandStreamReceiver {
    static CommandStreamReceiver *create(const std::string &baseName, bool withAubDump, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

    using TbxStream = NEO::TbxStream;
};

typedef CommandStreamReceiver *(*TbxCommandStreamReceiverCreateFunc)(const std::string &baseName, bool withAubDump, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
} // namespace NEO
