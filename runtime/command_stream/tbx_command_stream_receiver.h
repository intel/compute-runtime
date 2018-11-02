/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/aub_mem_dump/aub_mem_dump.h"

// To Enable TBX Serve support for "igdrcl_dll" project, do following when configuring with cmake:
// 1. cmake -DHAVE_TBX_SERVER=ON .
//      <or, when connecting to the localhost>
//    cmake -DHAVE_TBX_SERVER=ON .
//      <or, on Windows systems, edit HAVE_TBX_SERVER settings using>
//    notepad build_runtime_vs2013.bat

namespace OCLRT {
struct HardwareInfo;
class CommandStreamReceiver;
class TbxSockets;
class ExecutionEnvironment;

class TbxStream : public AubMemDump::AubStream {
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
    void writePTE(uint64_t physAddress, uint64_t entry) override;
    void writeMMIOImpl(uint32_t offset, uint32_t value) override;
    void registerPoll(uint32_t registerOffset, uint32_t mask, uint32_t value, bool pollNotEqual, uint32_t timeoutAction) override;
    void readMemory(uint64_t physAddress, void *memory, size_t size);
};

struct TbxCommandStreamReceiver {
    static CommandStreamReceiver *create(const HardwareInfo &hwInfo, bool withAubDump, ExecutionEnvironment &executionEnvironment);

    using TbxStream = OCLRT::TbxStream;
};

typedef CommandStreamReceiver *(*TbxCommandStreamReceiverCreateFunc)(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment);
} // namespace OCLRT
