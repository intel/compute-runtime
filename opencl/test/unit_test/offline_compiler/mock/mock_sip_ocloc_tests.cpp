/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_sip.h"

#include <vector>

static std::vector<char> dummyBinaryForSip;

using namespace NEO;

const char *MockSipKernel::dummyBinaryForSip = "12345678";

std::vector<char> MockSipKernel::getDummyGenBinary() {
    return std::vector<char>(dummyBinaryForSip, dummyBinaryForSip + sizeof(MockSipKernel::dummyBinaryForSip));
}
