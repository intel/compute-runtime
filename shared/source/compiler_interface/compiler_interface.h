/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/spinlock.h"

#include "cif/common/cif_main.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <map>
#include <unordered_map>

namespace NEO {
enum class SipKernelType : std::uint32_t;
class OsLibrary;
class CompilerCache;
class Device;

using specConstValuesMap = std::unordered_map<uint32_t, uint64_t>;

struct TranslationInput {
    TranslationInput(IGC::CodeType::CodeType_t srcType, IGC::CodeType::CodeType_t outType, IGC::CodeType::CodeType_t preferredIntermediateType = IGC::CodeType::undefined)
        : srcType(srcType), preferredIntermediateType(preferredIntermediateType), outType(outType) {
    }

    bool allowCaching = true;

    ArrayRef<const char> src;
    ArrayRef<const char> apiOptions;
    ArrayRef<const char> internalOptions;
    const char *tracingOptions = nullptr;
    uint32_t tracingOptionsCount = 0;
    IGC::CodeType::CodeType_t srcType = IGC::CodeType::invalid;
    IGC::CodeType::CodeType_t preferredIntermediateType = IGC::CodeType::invalid;
    IGC::CodeType::CodeType_t outType = IGC::CodeType::invalid;
    void *gtPinInput = nullptr;

    specConstValuesMap specializedValues;
};

struct TranslationOutput {
    enum class ErrorCode {
        success = 0,
        compilerNotAvailable,
        compilationFailure,
        buildFailure,
        linkFailure,
        alreadyCompiled,
        unknownError,
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

    static void makeCopy(MemAndSize &dst, CIF::Builtins::BufferSimple *src);
};

struct SpecConstantInfo {
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> idsBuffer;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> sizesBuffer;
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
    static CompilerInterfaceT *createInstance(std::unique_ptr<CompilerCache> &&cache, bool requireFcl) {
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

    MOCKABLE_VIRTUAL TranslationOutput::ErrorCode getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary,
                                                                     std::vector<char> &stateSaveAreaHeader);

    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcFeaturesAndWorkaroundsTagOCL> getIgcFeaturesAndWorkarounds(const NEO::Device &device);

    bool addOptionDisableZebin(std::string &options, std::string &internalOptions);
    bool disableZebin(std::string &options, std::string &internalOptions);

  protected:
    MOCKABLE_VIRTUAL bool initialize(std::unique_ptr<CompilerCache> &&cache, bool requireFcl);
    MOCKABLE_VIRTUAL bool loadFcl();
    MOCKABLE_VIRTUAL bool loadIgc();

    template <template <CIF::Version_t> class EntryPointT>
    std::once_flag &getIcbeVersionCallOnceFlag();

    template <template <CIF::Version_t> class EntryPointT>
    bool checkIcbeVersionOnce(CIF::CIFMain *main, const char *libName);

    bool verifyIcbeVersion();

    static SpinLock spinlock;
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<SpinLock> lock() {
        return std::unique_lock<SpinLock>{spinlock};
    }
    std::unique_ptr<CompilerCache> cache;

    using igcDevCtxUptr = CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL>;
    using fclDevCtxUptr = CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL>;

    std::unique_ptr<OsLibrary> igcLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> igcMain;
    std::map<const Device *, igcDevCtxUptr> igcDeviceContexts;
    std::string igcRevision;
    size_t igcLibSize{};
    time_t igcLibMTime{};

    std::unique_ptr<OsLibrary> fclLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> fclMain;
    std::map<const Device *, fclDevCtxUptr> fclDeviceContexts;
    CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> fclBaseTranslationCtx;

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

    std::once_flag igcIcbeCheckVersionCallOnce;
    std::once_flag fclIcbeCheckVersionCallOnce;
};
} // namespace NEO
