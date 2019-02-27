/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/built_ins/sip.h"

#include <vector>
namespace OCLRT {
class MockSipKernel : public SipKernel {
  public:
    static std::vector<char> dummyBinaryForSip;
    static std::vector<char> getDummyGenBinary();
    static std::vector<char> getBinary();
    static void initDummyBinary();
    static void shutDown();
};
} // namespace OCLRT