/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/compiler_interface/compiler_cache.h"
#include "core/helpers/string.h"
#include "core/os_interface/os_library.h"
#include "core/utilities/arrayref.h"
#include "core/utilities/spinlock.h"
#include "runtime/built_ins/sip.h"

#include "cif/common/cif_main.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <map>

namespace NEO {
class Device;

struct TranslationInput {
    TranslationInput(IGC::CodeType::CodeType_t srcType, IGC::CodeType::CodeType_t outType, IGC::CodeType::CodeType_t preferredIntermediateType = IGC::CodeType::undefined)
        : srcType(srcType), preferredIntermediateType(preferredIntermediateType), outType(outType) {
    }

    bool allowCaching = false;

    ArrayRef<const char> src;
    ArrayRef<const char> apiOptions;
    ArrayRef<const char> internalOptions;
    const char *tracingOptions = nullptr;
    uint32_t tracingOptionsCount = 0;
    IGC::CodeType::CodeType_t srcType = IGC::CodeType::invalid;
    IGC::CodeType::CodeType_t preferredIntermediateType = IGC::CodeType::invalid;
    IGC::CodeType::CodeType_t outType = IGC::CodeType::invalid;
    void *GTPinInput = nullptr;

    struct SpecConstants {
        CIF::Builtins::BufferLatest *idsBuffer = nullptr;
        CIF::Builtins::BufferLatest *sizesBuffer = nullptr;
        CIF::Builtins::BufferLatest *valuesBuffer = nullptr;
    } specConstants;
};

struct TranslationOutput {
    enum class ErrorCode {
        Success = 0,
        CompilerNotAvailable,
        CompilationFailure,
        BuildFailure,
        LinkFailure,
        AlreadyCompiled,
        UnknownError,
    };

    struct MemAndSize {
        std::unique_ptr<char[]> mem;
        size_t size = 0;
    };

    IGC::CodeType::CodeType_t intermediateCodeType = IGC::CodeType::invalid;
    MemAndSize intermediateRepresentation;
    MemAndSize deviceBinary;
    MemAndSize debugData;
    std::string frontendCompilerLog;
    std::string backendCompilerLog;

    template <typename ContainerT>
    static void makeCopy(ContainerT &dst, CIF::Builtins::BufferSimple *src) {
        if ((nullptr == src) || (src->GetSizeRaw() == 0)) {
            dst.clear();
            return;
        }
        dst.assign(src->GetMemory<char>(), src->GetSize<char>());
    }

    static void makeCopy(MemAndSize &dst, CIF::Builtins::BufferSimple *src) {
        if ((nullptr == src) || (src->GetSizeRaw() == 0)) {
            dst.mem.reset();
            dst.size = 0U;
            return;
        }

        dst.size = src->GetSize<char>();
        dst.mem = ::makeCopy(src->GetMemory<void>(), src->GetSize<char>());
    }
};

struct SpecConstantInfo {
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> idsBuffer;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> sizesBuffer;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> valuesBuffer;
};

class CompilerInterface {
  public:
    CompilerInterface();
    CompilerInterface(const CompilerInterface &) = delete;
    CompilerInterface &operator=(const CompilerInterface &) = delete;
    CompilerInterface(CompilerInterface &&) = delete;
    CompilerInterface &operator=(CompilerInterface &&) = delete;
    virtual ~CompilerInterface();

    template <typename CompilerInterfaceT = CompilerInterface>
    static CompilerInterfaceT *createInstance(std::unique_ptr<CompilerCache> cache, bool requireFcl) {
        auto instance = new CompilerInterfaceT();
        if (!instance->initialize(std::move(cache), requireFcl)) {
            delete instance;
            instance = nullptr;
        }
        return instance;
    }

    MOCKABLE_VIRTUAL TranslationOutput::ErrorCode build(const NEO::Device &device,
                                                        const TranslationInput &input,
                                                        TranslationOutput &output);

    MOCKABLE_VIRTUAL TranslationOutput::ErrorCode compile(const NEO::Device &device,
                                                          const TranslationInput &input,
                                                          TranslationOutput &output);

    MOCKABLE_VIRTUAL TranslationOutput::ErrorCode link(const NEO::Device &device,
                                                       const TranslationInput &input,
                                                       TranslationOutput &output);

    MOCKABLE_VIRTUAL TranslationOutput::ErrorCode getSpecConstantsInfo(const NEO::Device &device,
                                                                       ArrayRef<const char> srcSpirV, SpecConstantInfo &output);

    TranslationOutput::ErrorCode createLibrary(NEO::Device &device,
                                               const TranslationInput &input,
                                               TranslationOutput &output);

    MOCKABLE_VIRTUAL TranslationOutput::ErrorCode getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary);

  protected:
    MOCKABLE_VIRTUAL bool initialize(std::unique_ptr<CompilerCache> cache, bool requireFcl);
    MOCKABLE_VIRTUAL bool loadFcl();
    MOCKABLE_VIRTUAL bool loadIgc();

    static SpinLock spinlock;
    MOCKABLE_VIRTUAL std::unique_lock<SpinLock> lock() {
        return std::unique_lock<SpinLock>{spinlock};
    }
    std::unique_ptr<CompilerCache> cache = nullptr;

    using igcDevCtxUptr = CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL>;
    using fclDevCtxUptr = CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL>;

    std::unique_ptr<OsLibrary> igcLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> igcMain = nullptr;
    std::map<const Device *, igcDevCtxUptr> igcDeviceContexts;

    std::unique_ptr<OsLibrary> fclLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> fclMain = nullptr;
    std::map<const Device *, fclDevCtxUptr> fclDeviceContexts;
    CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> fclBaseTranslationCtx = nullptr;

    MOCKABLE_VIRTUAL IGC::FclOclDeviceCtxTagOCL *getFclDeviceCtx(const Device &device);
    MOCKABLE_VIRTUAL IGC::IgcOclDeviceCtxTagOCL *getIgcDeviceCtx(const Device &device);
    MOCKABLE_VIRTUAL IGC::CodeType::CodeType_t getPreferredIntermediateRepresentation(const Device &device);

    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> createFclTranslationCtx(const Device &device,
                                                                                                IGC::CodeType::CodeType_t inType,
                                                                                                IGC::CodeType::CodeType_t outType);
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> createIgcTranslationCtx(const Device &device,
                                                                                                IGC::CodeType::CodeType_t inType,
                                                                                                IGC::CodeType::CodeType_t outType);

    bool isFclAvailable() const {
        return (fclMain != nullptr);
    }

    bool isIgcAvailable() const {
        return (igcMain != nullptr);
    }

    bool isCompilerAvailable(IGC::CodeType::CodeType_t translationSrc, IGC::CodeType::CodeType_t translationDst) const {
        bool requiresFcl = (IGC::CodeType::oclC == translationSrc);
        bool requiresIgc = (IGC::CodeType::oclC != translationSrc) || ((IGC::CodeType::spirV != translationDst) && (IGC::CodeType::llvmBc != translationDst) && (IGC::CodeType::llvmLl != translationDst));
        return (isFclAvailable() || (false == requiresFcl)) && (isIgcAvailable() || (false == requiresIgc));
    }
};
} // namespace NEO
