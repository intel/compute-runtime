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

#include "perf_test_utils.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/hash.h"

#include <fstream>
#include <string>

using namespace OCLRT;
using namespace std;

const char *perfLogPath = "perf_logs/";

// Global reference time
long long refTime = 0;

void setReferenceTime() {
    if (refTime == 0) {
        Timer t1, t2, t3;
        long long time1 = 0;
        long long time2 = 0;
        long long time3 = 0;

        Timer::setFreq();
        void *bufferDst = alignedMalloc(128 * 4096, 4096);
        void *bufferSrc1 = alignedMalloc(128 * 4096, 4096);
        void *bufferSrc2 = alignedMalloc(128 * 4096, 4096);
        void *bufferSrc3 = alignedMalloc(128 * 4096, 4096);
        t1.start();
        memset(bufferSrc1, 0, 128 * 4096);
        memcpy(bufferDst, bufferSrc1, 128 * 4096);
        t1.end();

        t2.start();
        memset(bufferSrc2, 1, 128 * 4096);
        memcpy(bufferDst, bufferSrc2, 128 * 4096);
        t2.end();

        t3.start();
        memset(bufferSrc3, 2, 128 * 4096);
        memcpy(bufferDst, bufferSrc3, 128 * 4096);
        t3.end();

        time1 = t1.get();
        time2 = t2.get();
        time3 = t3.get();

        refTime = majorityVote(time1, time2, time3);

        alignedFree(bufferDst);
        alignedFree(bufferSrc1);
        alignedFree(bufferSrc2);
        alignedFree(bufferSrc3);
    }
}

bool getTestRatio(uint64_t hash, double &ratio) {

    ifstream file;
    string filename(perfLogPath);
    double data = 0.0;

    filename.append(std::to_string(hash));

    file.open(filename);
    if (file.is_open()) {
        file >> data;
        ratio = data;
        file.close();
        return true;
    }
    ratio = 0.0;
    return false;
}

bool saveTestRatio(uint64_t hash, double ratio) {

    ofstream file;
    string filename(perfLogPath);
    double data = 0.0;

    filename.append(std::to_string(hash));

    file.open(filename);
    if (file.is_open()) {
        file << ratio;
        file.close();
        return true;
    }
    return false;
}

bool isInRange(double data, double reference, double multiplier) {
    double lower = reference / multiplier;
    double higher = reference * multiplier;

    if ((data >= lower) && (data <= higher)) {
        return true;
    }
    return false;
}

bool isLowerThanReference(double data, double reference, double multiplier) {

    double higher = multiplier * reference;

    if (data <= higher) {
        return true;
    }
    return false;
}

bool updateTestRatio(uint64_t hash, double ratio) {

    double oldRatio = 0.0;

    if (getTestRatio(hash, oldRatio)) {
        if (oldRatio != 0.0) {

            if (isInRange(ratio, oldRatio, 2.000)) {
                double newRatio = (0.8000 * oldRatio + 0.2000 * ratio);
                if (newRatio < 0.8 * oldRatio)
                    return false;
                saveTestRatio(hash, newRatio);
                return true;
            }
        }
    } else {
        saveTestRatio(hash, ratio);
    }
    return false;
}
