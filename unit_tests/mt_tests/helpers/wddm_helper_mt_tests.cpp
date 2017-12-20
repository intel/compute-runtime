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

#include "runtime/helpers/wddm_helper.h"
#include "gtest/gtest.h"

#include <thread>

TEST(MtTestWddmFixture, givenCurrentPagingFenceValueWhenValueChangedThenValueIsSet) {
    std::atomic<uint64_t> currentPagingFenceValue;
    std::atomic<int> testCount;
    std::atomic<int> maxValue;
    currentPagingFenceValue.store(0);
    testCount.store(100);
    maxValue.store(0);
    int threadsCount = 8;
    std::thread threads[8];
    for (int i = 0; i < threadsCount; i++) {
        threads[i] = std::thread([&]() {
            while (testCount-- > 0) {
                uint64_t newVal = ++maxValue;
                interlockedMax(currentPagingFenceValue, newVal);
            }
        });
    }
    for (int i = 0; i < threadsCount; i++) {
        threads[i].join();
    }
    uint64_t endValue = currentPagingFenceValue.load();
    EXPECT_EQ(endValue, 100u);
}