/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_sip.h"

#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/test/common/mocks/mock_io_functions.h"

#include "StateSaveAreaHeaderWrapper.h"

namespace NEO {

static constexpr SIP::StateSaveAreaHeaderV3 mockSipStateSaveAreaHeaderV3 = {
    .versionHeader{
        .magic = "tssarea",
        .reserved1 = 0,
        .version = {3, 0, 0},
        .size = static_cast<uint8_t>(sizeof(SIP::StateSaveArea)),
        .reserved2 = {0, 0, 0}},
    .regHeader{}};

MockSipKernel::MockSipKernel(SipKernelType type, GraphicsAllocation *sipAlloc) : SipKernel(type, sipAlloc, {}) {
    this->mockStateSaveAreaHeader.resize(sizeof(mockSipStateSaveAreaHeaderV3));
    memcpy_s(this->mockStateSaveAreaHeader.data(), sizeof(mockSipStateSaveAreaHeaderV3), &mockSipStateSaveAreaHeaderV3, sizeof(mockSipStateSaveAreaHeaderV3));
    createMockSipAllocation();
}

MockSipKernel::MockSipKernel() : MockSipKernel(SipKernelType::csr, nullptr) {
}

MockSipKernel::~MockSipKernel() = default;

const char *MockSipKernel::dummyBinaryForSip = "12345678";

std::vector<char> MockSipKernel::getDummyGenBinary() {
    return std::vector<char>(dummyBinaryForSip, dummyBinaryForSip + sizeof(MockSipKernel::dummyBinaryForSip));
}

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

MockSipWrapper::MockSipWrapper() : mockFreadReturnBackup(&IoFunctions::mockFreadReturn, 1), mockFtellReturnBackup(&IoFunctions::mockFtellReturn, 1), mockFreadBufferBackup(&IoFunctions::mockFreadBuffer, const_cast<char *>(MockSipKernel::dummyBinaryForSip)) {

    debugManager.flags.LoadBinarySipFromFile.set("dummySip");
}

} // namespace NEO
