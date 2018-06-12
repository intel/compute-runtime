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

#include "cl_api_tests.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/helpers/memory_management.h"

using namespace OCLRT;

typedef api_tests ContextTest;

namespace ULT {

// multiplier of reference ratio that is compared ( checked if less than ) with current result
const double multiplier = 1.5000;
// ratio results that are not checked be EXPECT ( very short time tests are not chceked due to high fluctuations )
const double ratioThreshold = 0.005;

//------------------------------------------------------------------------------
// clCreateContext
//------------------------------------------------------------------------------

TEST_F(ContextTest, clCreateContext) {

    double previousRatio = -1.0;
    uint64_t hash = getHash(__FUNCTION__, strlen(__FUNCTION__));

    bool success = getTestRatio(hash, previousRatio);
    long long times[3] = {0, 0, 0};

    for (int i = 0; i < 3; i++) {
        Timer t;
        t.start();
        auto context = clCreateContext(nullptr, num_devices, devices, nullptr, nullptr, &retVal);
        t.end();

        times[i] = t.get();
        ((Context *)context)->release();
    }

    long long time = majorityVote(times[0], times[1], times[2]);

    double ratio = static_cast<double>(time) / static_cast<double>(refTime);

    if (success && previousRatio > ratioThreshold) {
        EXPECT_TRUE(isLowerThanReference(ratio, previousRatio, multiplier)) << "Current: " << ratio << " previous: " << previousRatio << "\n";
    }

    updateTestRatio(hash, ratio);
}

TEST_F(ContextTest, clReleaseContext) {

    double previousRatio = -1.0;
    uint64_t hash = getHash(__FUNCTION__, strlen(__FUNCTION__));

    bool success = getTestRatio(hash, previousRatio);
    long long times[3] = {0, 0, 0};

    cl_context contexts[3];
    cl_device_id clDevice = pDevice;

    for (int i = 0; i < 3; i++) {
        contexts[i] = Context::create(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal);
        Timer t;
        t.start();
        auto retVal = clReleaseContext(contexts[i]);
        t.end();

        times[i] = t.get();
    }

    long long time = majorityVote(times[0], times[1], times[2]);

    double ratio = static_cast<double>(time) / static_cast<double>(refTime);

    if (success && previousRatio > ratioThreshold) {
        EXPECT_TRUE(isLowerThanReference(ratio, previousRatio, multiplier)) << "Current: " << ratio << " previous: " << previousRatio << "\n";
    }

    updateTestRatio(hash, ratio);
}

TEST_F(ContextTest, clRetainContext) {
    double previousRatio = -1.0;
    uint64_t hash = getHash(__FUNCTION__, strlen(__FUNCTION__));

    bool success = getTestRatio(hash, previousRatio);
    long long times[3] = {0, 0, 0};

    for (int i = 0; i < 3; i++) {
        Timer t;
        t.start();
        auto retVal = clRetainContext(pContext);
        t.end();

        times[i] = t.get();
        pContext->release();
    }

    long long time = majorityVote(times[0], times[1], times[2]);
    double ratio = static_cast<double>(time) / static_cast<double>(refTime);

    if (success && previousRatio > ratioThreshold) {
        EXPECT_TRUE(isLowerThanReference(ratio, previousRatio, multiplier)) << "Current: " << ratio << " previous: " << previousRatio << "\n";
    }

    updateTestRatio(hash, ratio);
}
} // namespace ULT
