/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_sip.h"

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/test_files.h"

#include "cif/macros/enable.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <fstream>
#include <map>

namespace NEO {
MockSipKernel::MockSipKernel(SipKernelType type, GraphicsAllocation *sipAlloc) : SipKernel(type, sipAlloc) {
    this->mockSipMemoryAllocation =
        std::make_unique<MemoryAllocation>(0u,
                                           GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL,
                                           nullptr,
                                           MemoryConstants::pageSize * 10u,
                                           0u,
                                           MemoryConstants::pageSize,
                                           MemoryPool::System4KBPages, 3u);
}

MockSipKernel::MockSipKernel() : SipKernel(SipKernelType::Csr, nullptr) {
    this->mockSipMemoryAllocation =
        std::make_unique<MemoryAllocation>(0u,
                                           GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL,
                                           nullptr,
                                           MemoryConstants::pageSize * 10u,
                                           0u,
                                           MemoryConstants::pageSize,
                                           MemoryPool::System4KBPages, 3u);
}

MockSipKernel::~MockSipKernel() = default;

const char *MockSipKernel::dummyBinaryForSip = "12345678";

std::vector<char> MockSipKernel::getDummyGenBinary() {
    return std::vector<char>(dummyBinaryForSip, dummyBinaryForSip + sizeof(MockSipKernel::dummyBinaryForSip));
}

GraphicsAllocation *MockSipKernel::getSipAllocation() const {
    return mockSipMemoryAllocation.get();
}
} // namespace NEO
