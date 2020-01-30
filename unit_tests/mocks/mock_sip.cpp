/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_sip.h"

#include "core/helpers/file_io.h"
#include "core/helpers/hw_info.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/os_inc_base.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/mocks/mock_program.h"

#include "cif/macros/enable.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <fstream>
#include <map>

namespace NEO {
MockSipKernel::MockSipKernel(SipKernelType type, Program *sipProgram) : SipKernel(type, sipProgram) {
    this->mockSipMemoryAllocation =
        std::make_unique<MemoryAllocation>(0u,
                                           GraphicsAllocation::AllocationType::KERNEL_ISA,
                                           nullptr,
                                           MemoryConstants::pageSize * 10u,
                                           0u,
                                           MemoryConstants::pageSize,
                                           MemoryPool::System4KBPages);
}

MockSipKernel::MockSipKernel() : SipKernel(SipKernelType::Csr, nullptr) {
    this->mockSipMemoryAllocation =
        std::make_unique<MemoryAllocation>(0u,
                                           GraphicsAllocation::AllocationType::KERNEL_ISA,
                                           nullptr,
                                           MemoryConstants::pageSize * 10u,
                                           0u,
                                           MemoryConstants::pageSize,
                                           MemoryPool::System4KBPages);
    this->program = new MockProgram(this->executionEnvironment, nullptr, false);
}

MockSipKernel::~MockSipKernel() = default;

std::vector<char> MockSipKernel::dummyBinaryForSip;
std::vector<char> MockSipKernel::getDummyGenBinary() {
    if (dummyBinaryForSip.empty()) {
        dummyBinaryForSip = getBinary();
    }
    return dummyBinaryForSip;
}
std::vector<char> MockSipKernel::getBinary() {
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd8_", ".gen");

    size_t binarySize = 0;
    auto binary = loadDataFromFile(testFile.c_str(), binarySize);
    UNRECOVERABLE_IF(binary == nullptr);

    std::vector<char> ret{binary.get(), binary.get() + binarySize};

    return ret;
}
void MockSipKernel::initDummyBinary() {
    dummyBinaryForSip = getBinary();
}
void MockSipKernel::shutDown() {
    MockSipKernel::dummyBinaryForSip.clear();
}

GraphicsAllocation *MockSipKernel::getSipAllocation() const {
    return mockSipMemoryAllocation.get();
}
} // namespace NEO
