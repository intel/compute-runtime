/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_sip.h"

#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/test/common/helpers/test_files.h"

#include <fstream>
#include <map>

namespace NEO {
MockSipKernel::MockSipKernel(SipKernelType type, GraphicsAllocation *sipAlloc) : SipKernel(type, sipAlloc, {'s', 's', 'a', 'h'}) {
    createMockSipAllocation();
}

MockSipKernel::MockSipKernel() : SipKernel(SipKernelType::csr, nullptr, {'s', 's', 'a', 'h'}) {
    createMockSipAllocation();
}

MockSipKernel::~MockSipKernel() = default;

const char *MockSipKernel::dummyBinaryForSip = "12345678";

std::vector<char> MockSipKernel::getDummyGenBinary() {
    return std::vector<char>(dummyBinaryForSip, dummyBinaryForSip + sizeof(MockSipKernel::dummyBinaryForSip));
}

GraphicsAllocation *MockSipKernel::getSipAllocation() const {
    return mockSipMemoryAllocation.get();
}

const std::vector<char> &MockSipKernel::getStateSaveAreaHeader() const {
    return mockStateSaveAreaHeader;
}

void MockSipKernel::createMockSipAllocation() {
    this->mockSipMemoryAllocation =
        std::make_unique<MemoryAllocation>(0u,
                                           1u /*num gmms*/,
                                           AllocationType::kernelIsaInternal,
                                           nullptr,
                                           MemoryConstants::pageSize * 10u,
                                           0u,
                                           MemoryConstants::pageSize,
                                           MemoryPool::system4KBPages,
                                           256u);
}

} // namespace NEO
