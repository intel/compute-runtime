/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/sip.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include <memory>
#include <vector>

namespace NEO {
class MemoryAllocation;

class MockSipKernel : public SipKernel {
  public:
    using SipKernel::createHeaderFilename;
    using SipKernel::type;

    MockSipKernel(SipKernelType type, GraphicsAllocation *sipAlloc);
    MockSipKernel();
    ~MockSipKernel() override;

    static const char *dummyBinaryForSip;
    static std::vector<char> getDummyGenBinary();

    GraphicsAllocation *getSipAllocation() const override;
    const std::vector<char> &getStateSaveAreaHeader() const override;

    void createMockSipAllocation();

    std::unique_ptr<MemoryAllocation> mockSipMemoryAllocation;
    std::vector<char> mockStateSaveAreaHeader = {'s', 's', 'a', 'h'};
    MockExecutionEnvironment executionEnvironment;
};

namespace MockSipData {
extern std::unique_ptr<MockSipKernel> mockSipKernel;
extern SipKernelType calledType;
extern bool called;
extern bool returned;
extern bool useMockSip;

void clearUseFlags();
std::vector<char> createStateSaveAreaHeader(uint32_t version);
} // namespace MockSipData
} // namespace NEO
