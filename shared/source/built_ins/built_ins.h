/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_in_ops_base.h"
#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/utilities/stackvec.h"

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
class OsContext;

struct BuiltinCode {
    enum class ECodeType {
        any = 0,          // for requesting "any" code available - priorities as below
        binary = 1,       // ISA - highest priority
        intermediate = 2, // SPIR/LLVM - medium prioroty
        source = 3,       // OCL C - lowest priority
        count,
        invalid
    };

    static const char *getExtension(BuiltinCode::ECodeType ct) {
        switch (ct) {
        default:
            return "";
        case BuiltinCode::ECodeType::binary:
            return ".bin";
        case BuiltinCode::ECodeType::intermediate:
            return ".spv";
        case BuiltinCode::ECodeType::source:
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
const char *getBuiltinAsString(EBuiltInOps::Type builtin);

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
    inline static bool exists = false;

    static EmbeddedStorageRegistry &getInstance() {
        static EmbeddedStorageRegistry gsr;
        return gsr;
    }

    void store(const std::string &name, BuiltinResourceT &&resource) {
        resources.emplace(name, BuiltinResourceT(std::move(resource)));
    }

    const BuiltinResourceT *get(const std::string &name) const;

    ~EmbeddedStorageRegistry() {
        exists = false;
    }

  protected:
    EmbeddedStorageRegistry() {
        exists = true;
    }

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
    MOCKABLE_VIRTUAL const SipKernel &getSipKernel(Device &device, OsContext *context);
    MOCKABLE_VIRTUAL void freeSipKernels(MemoryManager *memoryManager);

    BuiltinsLib &getBuiltinsLib() {
        DEBUG_BREAK_IF(!builtinsLib.get());
        return *builtinsLib;
    }

  protected:
    // sip builtins
    std::pair<std::unique_ptr<SipKernel>, std::once_flag> sipKernels[static_cast<uint32_t>(SipKernelType::count)];

    std::unique_ptr<BuiltinsLib> builtinsLib;

    using ContextId = uint32_t;
    std::unordered_map<ContextId, std::pair<std::unique_ptr<SipKernel>, std::once_flag>> perContextSipKernels;
};

template <EBuiltInOps::Type opCode>
class BuiltInOp;

} // namespace NEO
