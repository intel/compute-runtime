/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/mocks/mock_cif.h"

#include "cif/builtins/memory/buffer/buffer.h"
#include "cif/export/library_api.h"

namespace OCLRT {

bool failCreateCifMain = false;
}

namespace CIF {

namespace Builtins {

template <typename... ArgsT>
Buffer<0>::Buffer(ArgsT &&... args) {
}

Buffer<0>::~Buffer() {
}

void BufferSimple::SetAllocator(CIF::Builtins::AllocatorT allocator, CIF::Builtins::DeallocatorT deallocator,
                                CIF::Builtins::ReallocatorT reallocator) {
}

void BufferSimple::SetUnderlyingStorage(void *memory, size_t size, CIF::Builtins::DeallocatorT deallocator) {
}

void BufferSimple::SetUnderlyingStorage(const void *memory, size_t size) {
}

void *BufferSimple::DetachAllocation() {
    return nullptr;
}

const void *BufferSimple::GetMemoryRaw() const {
    return nullptr;
}

void *BufferSimple::GetMemoryRawWriteable() {
    return nullptr;
}

size_t BufferSimple::GetSizeRaw() const {
    return 0;
}

size_t BufferSimple::GetCapacityRaw() const {
    return 0;
}

bool BufferSimple::Resize(size_t newSize) {
    return false;
}

bool BufferSimple::Reserve(size_t newCapacity) {
    return false;
}

void BufferSimple::Clear() {
}

void BufferSimple::Deallocate() {
}

bool BufferSimple::AlignUp(uint32_t alignment) {
    return false;
}

bool BufferSimple::PushBackRawBytes(const void *newData, size_t size) {
    return false;
}

bool BufferSimple::IsConst() const {
    return false;
}
} // namespace Builtins
} // namespace CIF

namespace OCLRT {

std::map<CIF::InterfaceId_t, CreatorFuncT> MockCIFMain::globalCreators;

bool MockCIFBuffer::failAllocations = false;

CIF::ICIF *MockCIFBuffer::Create(CIF::InterfaceId_t intId, CIF::Version_t version) {
    if (failAllocations) {
        return nullptr;
    }

    if (version != CIF::Builtins::BufferSimple::GetVersion()) {
        return nullptr;
    }

    return new MockCIFBuffer();
}

MockCIFBuffer::MockCIFBuffer() {
}

MockCIFMain::MockCIFMain() {
    defaultCreators[CIF::Builtins::BufferSimple::GetInterfaceId()] = MockCIFBuffer::Create;
}

CIF::ICIF *MockCIFMain::CreateInterfaceImpl(CIF::InterfaceId_t intId, CIF::Version_t version) {
    auto it = globalCreators.find(intId);
    if ((it == globalCreators.end()) || (it->second == nullptr)) {
        it = defaultCreators.find(intId);
        if ((it == defaultCreators.end()) || (it->second == nullptr)) {
            return nullptr;
        }
    }

    return it->second(intId, version);
}
} // namespace OCLRT

extern CIF::CIFMain *CreateCIFMainImpl() {
    if (OCLRT::failCreateCifMain) {
        return nullptr;
    }

    return new OCLRT::MockCIFMain;
}
