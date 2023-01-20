/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/program/printf_handler.h"

using namespace NEO;

class MockPrintfHandler : public PrintfHandler {
  public:
    using PrintfHandler::PrintfHandler;
    using PrintfHandler::printfSurface;
    using PrintfHandler::printfSurfaceInitialDataSizePtr;
    MockPrintfHandler(Device &device) : PrintfHandler{device} {}
    ~MockPrintfHandler() override = default;

    bool printEnqueueOutput() override {
        if (callBasePrintEnqueueOutput) {
            return PrintfHandler::printEnqueueOutput();
        }
        return printEnqueueOutputReturnValue;
    }

    bool callBasePrintEnqueueOutput = false;
    bool printEnqueueOutputReturnValue = false;
};
