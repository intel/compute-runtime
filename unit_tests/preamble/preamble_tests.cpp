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

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/preamble.h"
#include "runtime/utilities/stackvec.h"
#include "unit_tests/gen_common/test.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

#include <gtest/gtest.h>

#include <algorithm>

using PreambleTest = ::testing::Test;

using namespace OCLRT;

HWTEST_F(PreambleTest, PreemptionIsTakenIntoAccountWhenProgrammingPreamble) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));

    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    uint32_t cmdSizePreambleMidThread = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    uint32_t cmdSizePreemptionMidThread = static_cast<uint32_t>(PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice));

    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    uint32_t cmdSizePreambleDisabled = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    uint32_t cmdSizePreemptionDisabled = static_cast<uint32_t>(PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice));

    EXPECT_LE(cmdSizePreemptionMidThread, cmdSizePreambleMidThread);
    EXPECT_LE(cmdSizePreemptionDisabled, cmdSizePreambleDisabled);

    EXPECT_LE(cmdSizePreemptionDisabled, cmdSizePreemptionMidThread);
    EXPECT_LE((cmdSizePreemptionMidThread - cmdSizePreemptionDisabled), (cmdSizePreambleMidThread - cmdSizePreambleDisabled));

    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    StackVec<char, 8192> preambleBuffer(8192);
    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    StackVec<char, 4096> preemptionBuffer;
    preemptionBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preemptionStream(&*preemptionBuffer.begin(), preemptionBuffer.size());

    uintptr_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface(reinterpret_cast<void *>(minCsrAlignment), 1024);

    PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U,
                                                ThreadArbitrationPolicy::threadArbirtrationPolicyRoundRobin, &csrSurface);

    PreemptionHelper::programPreamble<FamilyType>(preemptionStream, *mockDevice, &csrSurface);

    ASSERT_LE(preemptionStream.getUsed(), preambleStream.getUsed());

    auto it = std::search(&preambleBuffer[0], &preambleBuffer[preambleStream.getUsed()],
                          &preemptionBuffer[0], &preemptionBuffer[preemptionStream.getUsed()]);
    EXPECT_NE(&preambleBuffer[preambleStream.getUsed()], it);
}
