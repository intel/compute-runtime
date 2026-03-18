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
#include "shared/source/utilities/stackvec.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {

class Device;
class SipKernel;
class MemoryManager;
class OsContext;

namespace BuiltIn {

using Resource = std::vector<char>;

enum class CodeType {
    any = 0,          // for requesting "any" code available - priorities as below
    binary = 1,       // ISA - highest priority
    intermediate = 2, // SPIR/LLVM - medium priority
    source = 3,       // OCL C - lowest priority
    count,
    invalid
};

struct Code {
    static const char *getExtension(CodeType ct) {
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

Resource createResource(const char *ptr, size_t size);
Resource createResource(const Resource &r);
std::string createResourceName(Group builtInGroup, const std::string &extension);
StackVec<std::string, 3> getResourceNames(Group builtInGroup, CodeType type, const Device &device);
const char *getAsString(Group builtInGroup);

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

struct EmbeddedStorageRegistry {
    inline static bool exists = false;

    static EmbeddedStorageRegistry &getInstance() {
        static EmbeddedStorageRegistry gsr;
        return gsr;
    }

    void store(const std::string &name, Resource &&resource) {
        resources.emplace(name, Resource(std::move(resource)));
    }

    const Resource *get(const std::string &name) const;

    ~EmbeddedStorageRegistry() {
        exists = false;
    }

  protected:
    EmbeddedStorageRegistry() {
        exists = true;
    }

    using ResourcesContainer = std::unordered_map<std::string, Resource>;
    ResourcesContainer resources;
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
    Code getBuiltinCode(Group builtInGroup, CodeType requestedCodeType, Device &device);

  protected:
    Resource getBuiltinResource(Group builtInGroup, CodeType requestedCodeType, Device &device);

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

namespace BuiltIn {
template <Group opCode>
class Op;
} // namespace BuiltIn

} // namespace NEO
