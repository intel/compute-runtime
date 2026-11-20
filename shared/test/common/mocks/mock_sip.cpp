/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_sip.h"

#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_sip_wrapper.h"

namespace NEO {

MockSipKernel::MockSipKernel(SipKernelType type, GraphicsAllocation *sipAlloc) : SipKernel(type, sipAlloc, {}), mockStateSaveAreaHeader(MockSipData::createStateSaveAreaHeaderV3WithoutHeapless()) {
    createMockSipAllocation();
}

MockSipKernel::MockSipKernel() : MockSipKernel(SipKernelType::csr, nullptr) {
}

MockSipKernel::~MockSipKernel() = default;

GraphicsAllocation *MockSipKernel::getSipAllocation() const {
    if (tempSipMemoryAllocation) {
        return tempSipMemoryAllocation.get();
    }
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
void MockSipKernel::createTempSipAllocation(size_t osContextCount) {
    this->tempSipMemoryAllocation =
        std::make_unique<MemoryAllocation>(0u,
                                           1u /*num gmms*/,
                                           AllocationType::kernelIsaInternal,
                                           nullptr,
                                           MemoryConstants::pageSize * 10u,
                                           0u,
                                           MemoryConstants::pageSize,
                                           MemoryPool::system4KBPages,
                                           osContextCount);
}

MockSipWrapper::MockSipWrapper() : mockFreadReturnBackup(&IoFunctions::mockFreadReturn, size_t{MockSipKernel::dummyBinaryForSipSize}), mockFtellReturnBackup(&IoFunctions::mockFtellReturn, MockSipKernel::dummyBinaryForSipSize), mockFreadBufferBackup(&IoFunctions::mockFreadBuffer, const_cast<char *>(MockSipKernel::dummyBinaryForSip)) {

    debugManager.flags.LoadBinarySipFromFile.set("dummySip");
}

} // namespace NEO
