/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_sip.h"

#include <vector>

static std::vector<char> dummyBinaryForSip;

using namespace NEO;

std::vector<char> MockSipKernel::dummyBinaryForSip;

std::vector<char> MockSipKernel::getDummyGenBinary() {
    return MockSipKernel::dummyBinaryForSip;
}

std::vector<char> MockSipKernel::getBinary() {
    return MockSipKernel::dummyBinaryForSip;
}

void MockSipKernel::initDummyBinary() {
}

void MockSipKernel::shutDown() {
}
