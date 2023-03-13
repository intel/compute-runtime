/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/assert_handler/assert_handler.h"

struct MockAssertHandler : NEO::AssertHandler {

    using NEO::AssertHandler::assertBufferSize;
    using NEO::AssertHandler::AssertHandler;

    void printAssertAndAbort() override {
        printAssertAndAbortCalled++;
        NEO::AssertHandler::printAssertAndAbort();
    }

    uint32_t printAssertAndAbortCalled = 0;
};
