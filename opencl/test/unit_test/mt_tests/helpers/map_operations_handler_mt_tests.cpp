/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/mem_obj/map_operations_handler.h"

#include "gtest/gtest.h"

#include <vector>

namespace NEO {

struct MockMapOperationsHandler : public MapOperationsHandler {
    using MapOperationsHandler::isOverlapping;
    using MapOperationsHandler::mappedPointers;
};

struct MapOperationsHandlerMtTests : public ::testing::Test {
    MockMapOperationsHandler mockHandler;
    MockGraphicsAllocation mockAllocation;
    std::vector<void *> mappedPtrs;

    void SetUp() override {
        MemObjSizeArray size = {{10, 10, 10}};
        MemObjOffsetArray offset = {{0, 0, 0}};
        cl_map_flags mapFlags = CL_MAP_READ;

        for (size_t i = 1; i <= 10; i++) {
            auto ptr = reinterpret_cast<void *>(0x1000lu * i);

            mappedPtrs.push_back(ptr);
            EXPECT_TRUE(mockHandler.add(ptr, 10, mapFlags, size, offset, 0, &mockAllocation));
        }
    }
};

TEST_F(MapOperationsHandlerMtTests, giveMapOperationsHandlerWhenAddingFindingAndRemovingThenExecuteSafely) {
    std::atomic<bool> removed = false;
    std::atomic<bool> t1Started = false;
    std::atomic<bool> t2Started = false;
    std::atomic<bool> t3Started = false;

    auto find = [&](std::atomic<bool> *threadStarted) {
        while (!removed.load()) {
            for (auto &ptr : mappedPtrs) {
                MapInfo out;
                mockHandler.findInfoForHostPtr(ptr, 1, out);
            }

            threadStarted->store(true);
            std::this_thread::yield();
        }
    };

    auto remove = [&]() {
        while (!t1Started.load() || !t2Started.load() || !t3Started.load()) {
            std::this_thread::yield();
        }

        for (auto &ptr : mappedPtrs) {
            mockHandler.remove(ptr);
        }

        removed.store(true);
    };

    std::thread t1(find, &t1Started);
    std::thread t2(find, &t2Started);
    std::thread t3(find, &t3Started);
    std::thread t4(remove);

    t1.join();
    t2.join();
    t3.join();
    t4.join();
}
} // namespace NEO
