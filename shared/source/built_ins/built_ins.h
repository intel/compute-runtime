/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_in_ops_base.h"
#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/mem_lifetime.h"
#include "shared/source/utilities/stackvec.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NEO {

class Device;
class SipKernel;
class MemoryManager;
class OsContext;

namespace BuiltIn {

struct Resource {
    Resource() = default;

    Resource(const char *ptr, size_t size, bool persistentMemory)
        : size(size), persistentMemory(persistentMemory) {
        if (ptr == nullptr || size == 0) {
            return;
        }

        if (persistentMemory) {
            data = ptr;
        } else {
            auto copy = new char[size];
            memcpy_s(copy, size, ptr, size);
            data = copy;
        }
    }

    Resource(const Resource &rhs)
        : Resource(rhs.data, rhs.size, rhs.persistentMemory) {
    }

    Resource(Resource &&rhs) noexcept
        : size(rhs.size), data(rhs.data), persistentMemory(rhs.persistentMemory) {
        rhs.size = 0;
        rhs.data = nullptr;
        rhs.persistentMemory = false;
    }

    bool empty() const {
        return size == 0;
    }

    friend bool operator==(const Resource &lhs, const Resource &rhs) {
        if (lhs.size != rhs.size) {
            return false;
        }

        if (lhs.persistentMemory && rhs.persistentMemory) {
            return lhs.data == rhs.data;
        }

        if (lhs.size == 0) {
            return true;
        }

        UNRECOVERABLE_IF(lhs.data == nullptr || rhs.data == nullptr);

        return std::memcmp(lhs.data, rhs.data, lhs.size) == 0;
    }

    Resource &operator=(const Resource &rhs) {
        if (this == &rhs) {
            return *this;
        }

        Resource copy(rhs);
        *this = std::move(copy);
        return *this;
    }

    Resource &operator=(Resource &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        if (!persistentMemory) {
            delete[] data;
        }

        size = rhs.size;
        data = rhs.data;
        persistentMemory = rhs.persistentMemory;

        rhs.size = 0;
        rhs.data = nullptr;
        rhs.persistentMemory = false;
        return *this;
    }

    ~Resource() {
        if (!persistentMemory) {
            delete[] data;
        }
    }

    size_t size = 0;
    const char *data = nullptr;
    bool persistentMemory = false;
};

enum class CodeType {
    any = 0,          // for requesting "any" code available - priorities as below
    binary = 1,       // ISA - highest priority
    intermediate = 2, // SPIR/LLVM - medium priority
    source = 3,       // OCL C - lowest priority
    count,
    invalid
};

struct Code {
    static constexpr const char *getExtension(CodeType ct) {
        switch (ct) {
        default:
            return "";
        case CodeType::binary:
            return ".bin";
        case CodeType::intermediate:
            return ".spv";
        case CodeType::source:
            return ".cl";
        }
    }

    CodeType type;
    Resource resource;
    Device *targetDevice;
};

} // namespace BuiltIn

namespace BuiltIn {

Resource createResource(const char *ptr, size_t size, bool persistentSrcMemory);
Resource createResource(const Resource &r);
inline Resource createResource(const char *ptr, size_t size) {
    return createResource(ptr, size, false);
};

StackVec<std::string, 3> getResourceNames(BaseKernel kernel, const AddressingMode &mode, CodeType type, const Device &device);

template <BaseKernel kernel, CodeType codeType>
constexpr auto makeResourceName() {
    constexpr std::string_view baseName = getAsString(kernel);
    constexpr std::string_view extension = Code::getExtension(codeType);

    std::array<char, baseName.size() + extension.size() + 1> name{};
    for (size_t i = 0; i < baseName.size(); ++i) {
        name[i] = baseName[i];
    }
    for (size_t i = 0; i < extension.size(); ++i) {
        name[baseName.size() + i] = extension[i];
    }
    return name;
}

template <BaseKernel kernel, CodeType codeType>
inline constexpr auto resourceName = makeResourceName<kernel, codeType>();

class Storage {
  public:
    Storage(const std::string &rootPath)
        : rootPath(rootPath) {
    }

    virtual ~Storage() = default;

    Resource load(const std::string &resourceName);

  protected:
    virtual Resource loadImpl(const std::string &fullResourceName) = 0;

    std::string rootPath;
};

class FileStorage : public Storage {
  public:
    FileStorage(const std::string &rootPath = "")
        : Storage(rootPath) {
    }

  protected:
    Resource loadImpl(const std::string &fullResourceName) override;
};

class EmbeddedStorage : public Storage {
  public:
    EmbeddedStorage(const std::string &rootPath)
        : Storage(rootPath) {
    }

  protected:
    Resource loadImpl(const std::string &fullResourceName) override;
};

class ResourceLoader {
  public:
    ResourceLoader();
    Code getBuiltinCode(BaseKernel kernel, const AddressingMode &mode, CodeType requestedCodeType, Device &device);

  protected:
    Resource getBuiltinResource(BaseKernel kernel, const AddressingMode &mode, CodeType requestedCodeType, Device &device);

    using StoragesContainerT = std::vector<std::unique_ptr<Storage>>;
    StoragesContainerT allStorages; // sorted by priority allStorages[0] will be checked before allStorages[1], etc.

    std::mutex mutex;
};

} // namespace BuiltIn

class BuiltIns {
  public:
    BuiltIns();
    virtual ~BuiltIns();

    MOCKABLE_VIRTUAL const SipKernel &getSipKernel(SipKernelType type, Device &device);
    MOCKABLE_VIRTUAL const SipKernel &getSipKernel(Device &device, OsContext *context);
    MOCKABLE_VIRTUAL void freeSipKernels(MemoryManager *memoryManager);

    BuiltIn::ResourceLoader &getBuiltinsLib() {
        DEBUG_BREAK_IF(!builtinsLib.get());
        return *builtinsLib;
    }

  protected:
    // sip builtins
    std::pair<std::unique_ptr<SipKernel>, std::once_flag> sipKernels[static_cast<uint32_t>(SipKernelType::count)];

    std::unique_ptr<BuiltIn::ResourceLoader> builtinsLib;

    using ContextId = uint32_t;
    std::unordered_map<ContextId, std::pair<std::unique_ptr<SipKernel>, std::once_flag>> perContextSipKernels;
};

} // namespace NEO
