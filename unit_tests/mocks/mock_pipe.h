/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/pipe.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

using namespace OCLRT;

class MockPipeStorage {
  public:
    MockPipeStorage() {
        mockGfxAllocation = new MockGraphicsAllocation(data, sizeof(data) / 2);
    }
    MockPipeStorage(bool unaligned) {
        mockGfxAllocation = new MockGraphicsAllocation(alignUp(&data, 4), sizeof(data) / 2);
    }
    char data[256];
    MockGraphicsAllocation *mockGfxAllocation;
};

class MockPipe : public MockPipeStorage, public Pipe {
  public:
    MockPipe(Context *context) : MockPipeStorage(), Pipe(context, 0, 1, 128, nullptr, &data, mockGfxAllocation) {
    }
    ~MockPipe() {
        if (!getContext()) {
            delete mockGfxAllocation;
        }
    }
};
