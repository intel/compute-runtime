/*
 * Copyright (C) 2018-2022 Intel Corporation
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

    MOCKABLE_VIRTUAL ~PrintfHandler();

    void prepareDispatch(const MultiDispatchInfo &multiDispatchInfo);
    void makeResident(CommandStreamReceiver &commandStreamReceiver);
    MOCKABLE_VIRTUAL bool printEnqueueOutput();

    GraphicsAllocation *getSurface() {
        return printfSurface;
    }

  protected:
    PrintfHandler(ClDevice &device);

    std::unique_ptr<uint32_t> printfSurfaceInitialDataSizePtr;
    ClDevice &device;
    Kernel *kernel = nullptr;
    GraphicsAllocation *printfSurface = nullptr;
};
} // namespace NEO
