/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstdint>
#include <memory>

namespace NEO {

class CommandStreamReceiver;
class Kernel;
class GraphicsAllocation;
class Device;
struct MultiDispatchInfo;

class PrintfHandler : NonCopyableAndNonMovableClass {
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
