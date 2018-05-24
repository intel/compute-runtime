/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#pragma once
#include "CL/cl.h"
#include "runtime/built_ins/sip.h"
#include "runtime/utilities/vec.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace OCLRT {
typedef std::vector<char> BuiltinResourceT;

class Context;
class Device;
class Kernel;
struct KernelInfo;
struct MultiDispatchInfo;
class Program;
class SchedulerKernel;

extern const char *mediaKernelsBuildOptions;

enum class EBuiltInOps : uint32_t {
    CopyBufferToBuffer = 0,
    CopyBufferRect,
    FillBuffer,
    CopyBufferToImage3d,
    CopyImage3dToBuffer,
    CopyImageToImage1d,
    CopyImageToImage2d,
    CopyImageToImage3d,
    FillImage1d,
    FillImage2d,
    FillImage3d,
    VmeBlockMotionEstimateIntel,
    VmeBlockAdvancedMotionEstimateCheckIntel,
    VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel,
    Scheduler,
    COUNT
};

BuiltinResourceT createBuiltinResource(const char *ptr, size_t size);
BuiltinResourceT createBuiltinResource(const BuiltinResourceT &r);
std::string createBuiltinResourceName(EBuiltInOps builtin, const std::string &extension,
                                      const std::string &platformName = "", uint32_t deviceRevId = 0);
std::string joinPath(const std::string &lhs, const std::string &rhs);
const char *getBuiltinAsString(EBuiltInOps builtin);

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

struct BuiltinCode {
    enum class ECodeType {
        Any = 0,          // for requesting "any" code available - priorities as below
        Binary = 1,       // ISA - highest priority
        Intermediate = 2, // SPIR/LLVM - medium prioroty
        Source = 3,       // OCL C - lowest priority
        COUNT,
        INVALID
    };

    static const char *getExtension(ECodeType ct) {
        switch (ct) {
        default:
            return "";
        case ECodeType::Binary:
            return ".bin";
        case ECodeType::Intermediate:
            return ".bc";
        case ECodeType::Source:
            return ".cl";
        }
    }

    ECodeType type;
    BuiltinResourceT resource;
    Device *targetDevice;
};

class BuiltinsLib {
  public:
    BuiltinsLib();
    BuiltinCode getBuiltinCode(EBuiltInOps builtin, BuiltinCode::ECodeType requestedCodeType, Device &device);

    static std::unique_ptr<Program> createProgramFromCode(const BuiltinCode &bc, Context &context, Device &device);

  protected:
    BuiltinResourceT getBuiltinResource(EBuiltInOps builtin, BuiltinCode::ECodeType requestedCodeType, Device &device);

    using StoragesContainerT = std::vector<std::unique_ptr<Storage>>;
    StoragesContainerT allStorages; // sorted by priority allStorages[0] will be checked before allStorages[1], etc.

    std::mutex mutex;
};

struct BuiltInKernel {
    const char *pSource = nullptr;
    Program *pProgram = nullptr;
    std::once_flag programIsInitialized; // guard for creating+building the program
    Kernel *pKernel = nullptr;

    BuiltInKernel() {
    }
};

class BuiltinDispatchInfoBuilder;

class BuiltIns {
  public:
    using HWFamily = int;
    std::pair<std::unique_ptr<BuiltinDispatchInfoBuilder>, std::once_flag> BuiltinOpsBuilders[static_cast<uint32_t>(EBuiltInOps::COUNT)];

    BuiltinDispatchInfoBuilder &getBuiltinDispatchInfoBuilder(EBuiltInOps op, Context &context, Device &device);
    std::unique_ptr<BuiltinDispatchInfoBuilder> setBuiltinDispatchInfoBuilder(EBuiltInOps op, Context &context, Device &device,
                                                                              std::unique_ptr<BuiltinDispatchInfoBuilder> newBuilder);

    static BuiltIns &getInstance();
    static void shutDown();
    Program *createBuiltInProgram(
        Context &context,
        Device &device,
        const char *kernelNames,
        int &errcodeRet);

    SchedulerKernel &getSchedulerKernel(Context &context);

    MOCKABLE_VIRTUAL const SipKernel &getSipKernel(SipKernelType type, Device &device);

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
    BuiltIns();
    virtual ~BuiltIns();

    // singleton
    static BuiltIns *pInstance;

    // scheduler kernel
    BuiltInKernel schedulerBuiltIn;

    // sip builtins
    std::pair<std::unique_ptr<SipKernel>, std::once_flag> sipKernels[static_cast<uint32_t>(SipKernelType::COUNT)];

    std::unique_ptr<BuiltinsLib> builtinsLib;

    using ProgramsContainerT = std::array<std::pair<std::unique_ptr<Program>, std::once_flag>, static_cast<size_t>(EBuiltInOps::COUNT)>;
    ProgramsContainerT builtinPrograms;
    bool enableCacheing = true;
};

template <typename HWFamily, EBuiltInOps OpCode>
class BuiltInOp;

} // namespace OCLRT
