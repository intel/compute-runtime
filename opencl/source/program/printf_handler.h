/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <memory>

namespace NEO {

class CommandStreamReceiver;
class Kernel;
class GraphicsAllocation;
class Device;
struct MultiDispatchInfo;

class PrintfHandler {
  public:
    static PrintfHandler *create(const MultiDispatchInfo &multiDispatchInfo, Device &deviceArg);

    MOCKABLE_VIRTUAL ~PrintfHandler();

    void prepareDispatch(const MultiDispatchInfo &multiDispatchInfo);
    void makeResident(CommandStreamReceiver &commandStreamReceiver);
    MOCKABLE_VIRTUAL bool printEnqueueOutput();

    GraphicsAllocation *getSurface() {
        return printfSurface;
    }

  protected:
    PrintfHandler(Device &device);

    std::unique_ptr<uint32_t> printfSurfaceInitialDataSizePtr;
    Device &device;
    Kernel *kernel = nullptr;
    GraphicsAllocation *printfSurface = nullptr;
};
} // namespace NEO
