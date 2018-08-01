/*
* Copyright (c) 2018, Intel Corporation
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

#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "unit_tests/mocks/linux/mock_drm_memory_manager.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"
#include "gtest/gtest.h"

#include <atomic>
#include <memory>
#include <thread>

using namespace OCLRT;
using namespace std;

TEST(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSharedAllocationIsCreatedFromMultipleThreadsThenSingleBoIsReused) {
    class MockDrm : public Drm {
      public:
        MockDrm(int fd) : Drm(fd) {}

        int ioctl(unsigned long request, void *arg) override {
            if (request == DRM_IOCTL_PRIME_FD_TO_HANDLE) {
                auto *primeToHandleParams = (drm_prime_handle *)arg;
                primeToHandleParams->handle = 10;
            }
            return 0;
        }
    };

    auto mock = make_unique<MockDrm>(0);
    auto memoryManager = make_unique<TestedDrmMemoryManager>(mock.get());

    osHandle handle = 3;
    constexpr size_t maxThreads = 10;

    GraphicsAllocation *createdAllocations[maxThreads];
    thread threads[maxThreads];
    atomic<size_t> index(0);

    auto createFunction = [&]() {
        size_t indexFree = index++;
        createdAllocations[indexFree] = memoryManager->createGraphicsAllocationFromSharedHandle(handle, false, true);
        EXPECT_NE(nullptr, createdAllocations[indexFree]);
    };

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i] = std::thread(createFunction);
    }

    while (index < maxThreads) {
        EXPECT_GE(1u, memoryManager->sharingBufferObjects.size());
    }

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i].join();
        memoryManager->freeGraphicsMemory(createdAllocations[i]);
    }
}
