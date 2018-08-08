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

#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_device.h"
#include "gen_cmd_parse.h"
#include "unit_tests/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "gtest/gtest.h"

namespace OCLRT {

void AUBCommandStreamFixture::SetUp(CommandQueue *pCmdQ) {
    ASSERT_NE(pCmdQ, nullptr);
    auto &device = reinterpret_cast<MockDevice &>(pCmdQ->getDevice());

    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream strfilename;
    strfilename << testInfo->test_case_name() << "_" << testInfo->name();

    const auto &hwInfo = device.getHardwareInfo();
    pCommandStreamReceiver = AUBCommandStreamReceiver::create(hwInfo, strfilename.str(), true, *device.executionEnvironment);
    ASSERT_NE(nullptr, pCommandStreamReceiver);

    device.resetCommandStreamReceiver(pCommandStreamReceiver);

    CommandStreamFixture::SetUp(pCmdQ);

    pTagMemory = pCommandStreamReceiver->getTagAddress();
    this->commandQueue = pCmdQ;
}

void AUBCommandStreamFixture::TearDown() {
    CommandStreamFixture::TearDown();
}
} // namespace OCLRT
