/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "cif/builtins/memory/buffer/buffer.h"
#include "cif/common/id.h"
#include "cif/common/cif.h"
#include "cif/common/cif_main.h"

#include <map>

namespace OCLRT {

extern bool failCreateCifMain;

using CreatorFuncT = CIF::ICIF *(*)(CIF::InterfaceId_t intId, CIF::Version_t version);

template <typename BaseType = CIF::ICIF>
struct MockCIF : BaseType {
    MockCIF()
        : refCount(1) {
    }

    void Release() override {
        auto prev = refCount--;
        assert(prev >= 1);
        if (prev == 1) {
            delete this;
        }
    }

    void Retain() override {
        ++refCount;
    }

    uint32_t GetRefCount() const override {
        return refCount;
    }

    CIF::Version_t GetEnabledVersion() const override {
        return 1;
    }

    bool GetSupportedVersions(CIF::InterfaceId_t intId, CIF::Version_t &verMin,
                              CIF::Version_t &verMax) const override {
        verMin = 1;
        verMax = CIF::MaxVersion;
        return true; // by default : no sub-interface are supported
    }

    uint32_t refCount;
};

struct MockCIFMain : MockCIF<CIF::CIFMain> {
    template <typename InterfaceT>
    static void setGlobalCreatorFunc(CreatorFuncT func) {
        globalCreators[InterfaceT::GetInterfaceId()] = func;
    }

    template <typename InterfaceT>
    void setDefaultCreatorFunc(CreatorFuncT func) {
        defaultCreators[InterfaceT::GetInterfaceId()] = func;
    }

    template <typename InterfaceT>
    static CreatorFuncT getGlobalCreatorFunc() {
        auto it = globalCreators.find(InterfaceT::GetInterfaceId());
        if (it == globalCreators.end()) {
            return nullptr;
        }
        return it->second;
    }

    template <typename InterfaceT>
    static void removeGlobalCreatorFunc() {
        auto it = globalCreators.find(InterfaceT::GetInterfaceId());
        if (it == globalCreators.end()) {
            return;
        }

        globalCreators.erase(it);
    }

    static void clearGlobalCreatorFuncs() {
        decltype(globalCreators) emptyCreators;
        globalCreators.swap(emptyCreators);
    }

    static std::map<CIF::InterfaceId_t, CreatorFuncT> globalCreators;

    MockCIFMain();
    ~MockCIFMain() {
    }

    CIF::Version_t GetBinaryVersion() const override {
        return 1;
    }

    CIF::ICIF *CreateInterfaceImpl(CIF::InterfaceId_t intId, CIF::Version_t version) override;

    CIF::InterfaceId_t FindIncompatibleImpl(CIF::InterfaceId_t entryPointInterface, CIF::CompatibilityDataHandle handle) const override {
        if (globalCreators.find(entryPointInterface) != globalCreators.end()) {
            return CIF::InvalidInterface;
        }
        if (defaultCreators.find(entryPointInterface) != defaultCreators.end()) {
            return CIF::InvalidInterface;
        }
        return entryPointInterface;
    }

    std::map<CIF::InterfaceId_t, CreatorFuncT> defaultCreators;
};

struct MockCIFBuffer : MockCIF<CIF::Builtins::BufferSimple> {
    MockCIFBuffer();
    static CIF::ICIF *Create(CIF::InterfaceId_t intId, CIF::Version_t version);
    static bool failAllocations;

    void SetAllocator(CIF::Builtins::AllocatorT allocator, CIF::Builtins::DeallocatorT deallocator,
                      CIF::Builtins::ReallocatorT reallocator) override {
        // unsupported in mock
    }

    void SetUnderlyingStorage(void *memory, size_t size, CIF::Builtins::DeallocatorT deallocator) override {
        if ((memory == nullptr) || (size == 0)) {
            data.clear();
            return;
        }
        data.assign((char *)memory, ((char *)memory) + size);
    }

    void SetUnderlyingStorage(const void *memory, size_t size) override {
        if ((memory == nullptr) || (size == 0)) {
            data.clear();
            return;
        }
        data.assign((char *)memory, ((char *)memory) + size);
    }

    void *DetachAllocation() override {
        // unsupported in mock
        return nullptr;
    }

    const void *GetMemoryRaw() const override {
        if (data.size() > 0) {
            return data.data();
        } else {
            return nullptr;
        }
    }

    void *GetMemoryRawWriteable() override {
        if (data.size() > 0) {
            return data.data();
        } else {
            return nullptr;
        }
    }

    size_t GetSizeRaw() const override {
        return data.size();
    }

    size_t GetCapacityRaw() const override {
        return data.capacity();
    }

    bool Resize(size_t newSize) override {
        data.resize(newSize);
        return true;
    }

    bool Reserve(size_t newCapacity) override {
        data.reserve(newCapacity);
        return true;
    }

    void Clear() override {
        data.clear();
    }

    void Deallocate() override {
        std::vector<char> rhs;
        rhs.swap(data);
    }

    bool AlignUp(uint32_t alignment) override {
        auto rest = data.size() & alignment;
        if (rest != 0) {
            data.resize(data.size() + alignment - rest);
        }
        return true;
    }

    bool PushBackRawBytes(const void *newData, size_t size) override {
        if ((newData == nullptr) || (size == 0)) {
            return true;
        }
        data.insert(data.end(), (char *)newData, ((char *)newData) + size);
        return true;
    }

    bool IsConst() const override {
        return false;
    }

    std::vector<char> data;
};
} // namespace OCLRT
