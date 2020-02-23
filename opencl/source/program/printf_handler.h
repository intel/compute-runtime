/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "opencl/source/kernel/kernel.h"

namespace NEO {

class ClDevice;
struct MultiDispatchInfo;

class PrintfHandler {
  public:
    static PrintfHandler *create(const MultiDispatchInfo &multiDispatchInfo, ClDevice &deviceArg);

    ~PrintfHandler();

    void prepareDispatch(const MultiDispatchInfo &multiDispatchInfo);
    void makeResident(CommandStreamReceiver &commandStreamReceiver);
    void printEnqueueOutput();

    GraphicsAllocation *getSurface() {
        return printfSurface;
    }

  protected:
    PrintfHandler(ClDevice &device);

    static const uint32_t printfSurfaceInitialDataSize = sizeof(uint32_t);
    ClDevice &device;
    Kernel *kernel = nullptr;
    GraphicsAllocation *printfSurface = nullptr;
};
} // namespace NEO
