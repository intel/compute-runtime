/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "test.h"

#include "runtime/helpers/task_information.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"

#include <memory>

using namespace OCLRT;

TEST(CommandTest, mapUnmapSubmitWithoutTerminateFlagFlushesCsr) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr;
    csr.setMemoryManager(device->getMemoryManager());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::MAP, buffer, size, offset, false, csr, *cmdQ.get()));
    CompletionStamp completionStamp = command->submit(20, false);

    auto expectedTaskCount = initialTaskCount + 1;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, mapUnmapSubmitWithTerminateFlagAbortsFlush) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr;
    csr.setMemoryManager(device->getMemoryManager());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::MAP, buffer, size, offset, false, csr, *cmdQ.get()));
    CompletionStamp completionStamp = command->submit(20, true);

    auto submitTaskCount = csr.peekTaskCount();
    EXPECT_EQ(initialTaskCount, submitTaskCount);

    auto expectedTaskCount = 0u;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, markerSubmitWithoutTerminateFlagFlushesCsr) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr;
    csr.setMemoryManager(device->getMemoryManager());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();
    std::unique_ptr<Command> command(new CommandMarker(*cmdQ.get(), csr, CL_COMMAND_MARKER, 0));
    CompletionStamp completionStamp = command->submit(20, false);

    auto expectedTaskCount = initialTaskCount + 1;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, markerSubmitWithTerminateFlagAbortsFlush) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr;
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();
    std::unique_ptr<Command> command(new CommandMarker(*cmdQ.get(), csr, CL_COMMAND_MARKER, 0));
    CompletionStamp completionStamp = command->submit(20, true);

    auto submitTaskCount = csr.peekTaskCount();
    EXPECT_EQ(initialTaskCount, submitTaskCount);

    auto expectedTaskCount = 0u;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}
