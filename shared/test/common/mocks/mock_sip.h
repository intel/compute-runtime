/*
 * Copyright (C) 2018-2025 Intel Corporation
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
    using SipKernel::selectSipClassType;
    using SipKernel::type;

    MockSipKernel(SipKernelType type, GraphicsAllocation *sipAlloc);
    MockSipKernel();
    ~MockSipKernel() override;

    static const char *dummyBinaryForSip;
    static std::vector<char> getDummyGenBinary();

    GraphicsAllocation *getSipAllocation() const override;
    const std::vector<char> &getStateSaveAreaHeader() const override;

    void createMockSipAllocation();
    void createTempSipAllocation(size_t osContextCount);

    std::unique_ptr<MemoryAllocation> mockSipMemoryAllocation;
    std::unique_ptr<MemoryAllocation> tempSipMemoryAllocation;
    std::vector<char> mockStateSaveAreaHeader;
    MockExecutionEnvironment executionEnvironment;
};

namespace MockSipData {
extern std::unique_ptr<MockSipKernel> mockSipKernel;
extern SipKernelType calledType;
extern bool called;
extern bool returned;
extern bool useMockSip;
extern bool uninitializedSipRequested;
extern uint64_t totalWmtpDataSize;

void clearUseFlags();
std::vector<char> createStateSaveAreaHeader(uint32_t version);
std::vector<char> createStateSaveAreaHeader(uint32_t version, uint16_t grfNum);
std::vector<char> createStateSaveAreaHeader(uint32_t version, uint16_t grfNum, uint16_t mmeNum);
} // namespace MockSipData
} // namespace NEO
