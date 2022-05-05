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
    MockPrintfHandler(ClDevice &device) : PrintfHandler{device} {}
    ~MockPrintfHandler() override = default;

    bool printEnqueueOutput() override {
        return false;
    }
};