/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/kernel/kernel.h"

namespace OCLRT {

struct MultiDispatchInfo;

class PrintfHandler {
  public:
    static PrintfHandler *create(const MultiDispatchInfo &multiDispatchInfo, Device &deviceArg);

    ~PrintfHandler();

    void prepareDispatch(const MultiDispatchInfo &multiDispatchInfo);
    void makeResident(CommandStreamReceiver &commandStreamReceiver);
    void printEnqueueOutput();

    GraphicsAllocation *getSurface() {
        return printfSurface;
    }

  protected:
    PrintfHandler(Device &device);

    static const uint32_t printfSurfaceInitialDataSize = sizeof(uint32_t);
    Device &device;
    Kernel *kernel = nullptr;
    GraphicsAllocation *printfSurface = nullptr;
};
} // namespace OCLRT
