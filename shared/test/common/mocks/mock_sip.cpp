/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_sip.h"

#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/test_files.h"

#include <fstream>
#include <map>

namespace NEO {
MockSipKernel::MockSipKernel(SipKernelType type, GraphicsAllocation *sipAlloc) : SipKernel(type, sipAlloc, {'s', 's', 'a', 'h'}) {
    createMockSipAllocation();
}

MockSipKernel::MockSipKernel() : SipKernel(SipKernelType::Csr, nullptr, {'s', 's', 'a', 'h'}) {
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
                                           AllocationType::KERNEL_ISA_INTERNAL,
                                           nullptr,
                                           MemoryConstants::pageSize * 10u,
                                           0u,
                                           MemoryConstants::pageSize,
                                           MemoryPool::System4KBPages, 3u);
}

} // namespace NEO
