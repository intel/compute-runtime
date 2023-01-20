/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_in_ops_base.h"
#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/helpers/debug_helpers.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
typedef std::vector<char> BuiltinResourceT;

class Device;
class SipKernel;
class MemoryManager;

inline constexpr ConstStringRef mediaKernelsBuildOptionsList[] = {
    "-D cl_intel_device_side_advanced_vme_enable",
    "-D cl_intel_device_side_avc_vme_enable",
    "-D cl_intel_device_side_vme_enable",
    "-D cl_intel_media_block_io",
    CompilerOptions::fastRelaxedMath};

inline constexpr CompilerOptions::ConstConcatenation<> mediaKernelsBuildOptions{mediaKernelsBuildOptionsList};

struct BuiltinCode {
    enum class ECodeType {
        Any = 0,          // for requesting "any" code available - priorities as below
        Binary = 1,       // ISA - highest priority
        Intermediate = 2, // SPIR/LLVM - medium prioroty
        Source = 3,       // OCL C - lowest priority
        COUNT,
        INVALID
    };

    static const char *getExtension(BuiltinCode::ECodeType ct) {
        switch (ct) {
        default:
            return "";
        case BuiltinCode::ECodeType::Binary:
            return ".bin";
        case BuiltinCode::ECodeType::Intermediate:
            return ".bc";
        case BuiltinCode::ECodeType::Source:
            return ".cl";
        }
    }

    BuiltinCode::ECodeType type;
    BuiltinResourceT resource;
    Device *targetDevice;
};

BuiltinResourceT createBuiltinResource(const char *ptr, size_t size);
BuiltinResourceT createBuiltinResource(const BuiltinResourceT &r);
std::string createBuiltinResourceName(EBuiltInOps::Type builtin, const std::string &extension);
StackVec<std::string, 3> getBuiltinResourceNames(EBuiltInOps::Type builtin, BuiltinCode::ECodeType type, const Device &device);
std::string joinPath(const std::string &lhs, const std::string &rhs);
const char *getBuiltinAsString(EBuiltInOps::Type builtin);
const char *getAdditionalBuiltinAsString(EBuiltInOps::Type builtin);

class Storage {
  public:
    Storage(const std::string &rootPath)
        : rootPath(rootPath) {
    }

    virtual ~Storage() = default;

    BuiltinResourceT load(const std::string &resourceName);

  protected:
    virtual BuiltinResourceT loadImpl(const std::string &fullResourceName) = 0;

    std::string rootPath;
};

class FileStorage : public Storage {
  public:
    FileStorage(const std::string &rootPath = "")
        : Storage(rootPath) {
    }

  protected:
    BuiltinResourceT loadImpl(const std::string &fullResourceName) override;
};

struct EmbeddedStorageRegistry {
    static EmbeddedStorageRegistry &getInstance() {
        static EmbeddedStorageRegistry gsr;
        return gsr;
    }

    void store(const std::string &name, BuiltinResourceT &&resource) {
        resources.emplace(name, BuiltinResourceT(std::move(resource)));
    }

    const BuiltinResourceT *get(const std::string &name) const;

  private:
    using ResourcesContainer = std::unordered_map<std::string, BuiltinResourceT>;
    ResourcesContainer resources;
};

class EmbeddedStorage : public Storage {
  public:
    EmbeddedStorage(const std::string &rootPath)
        : Storage(rootPath) {
    }

  protected:
    BuiltinResourceT loadImpl(const std::string &fullResourceName) override;
};

class BuiltinsLib {
  public:
    BuiltinsLib();
    BuiltinCode getBuiltinCode(EBuiltInOps::Type builtin, BuiltinCode::ECodeType requestedCodeType, Device &device);

  protected:
    BuiltinResourceT getBuiltinResource(EBuiltInOps::Type builtin, BuiltinCode::ECodeType requestedCodeType, Device &device);

    using StoragesContainerT = std::vector<std::unique_ptr<Storage>>;
    StoragesContainerT allStorages; // sorted by priority allStorages[0] will be checked before allStorages[1], etc.

    std::mutex mutex;
};

class BuiltIns {
  public:
    BuiltIns();
    virtual ~BuiltIns();

    MOCKABLE_VIRTUAL const SipKernel &getSipKernel(SipKernelType type, Device &device);
    MOCKABLE_VIRTUAL void freeSipKernels(MemoryManager *memoryManager);

    BuiltinsLib &getBuiltinsLib() {
        DEBUG_BREAK_IF(!builtinsLib.get());
        return *builtinsLib;
    }

    void setCacheingEnableState(bool enableCacheing) {
        this->enableCacheing = enableCacheing;
    }

    bool isCacheingEnabled() const {
        return this->enableCacheing;
    }

  protected:
    // sip builtins
    std::pair<std::unique_ptr<SipKernel>, std::once_flag> sipKernels[static_cast<uint32_t>(SipKernelType::COUNT)];

    std::unique_ptr<BuiltinsLib> builtinsLib;

    bool enableCacheing = true;
};

template <EBuiltInOps::Type OpCode>
class BuiltInOp;

} // namespace NEO
