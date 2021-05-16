/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/mem_obj/pipe.h"

using namespace NEO;

class MockPipeStorage {
  public:
    MockPipeStorage() {
        mockGfxAllocation = new MockGraphicsAllocation(data, sizeof(data) / 2);
    }
    MockPipeStorage(bool unaligned) {
        mockGfxAllocation = new MockGraphicsAllocation(alignUp(&data, 4), sizeof(data) / 2);
    }
    char data[256]{};
    MockGraphicsAllocation *mockGfxAllocation = nullptr;
};

class MockPipe : public MockPipeStorage, public Pipe {
  public:
    MockPipe(Context *context) : MockPipeStorage(), Pipe(context, 0, 1, 128, nullptr, &data, GraphicsAllocationHelper::toMultiGraphicsAllocation(mockGfxAllocation)) {
    }
    ~MockPipe() override {
        if (!getContext()) {
            delete mockGfxAllocation;
        }
    }
};
