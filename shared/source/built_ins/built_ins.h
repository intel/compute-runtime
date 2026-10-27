/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_in_ops_base.h"
#include "shared/source/built_ins/built_in_resource.h"
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
