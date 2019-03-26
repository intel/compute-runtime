/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/pipe.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

using namespace NEO;

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
