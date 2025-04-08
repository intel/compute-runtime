/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/spinlock.h"
#include "shared/source/utilities/stackvec.h"

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
struct TargetDevice;

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
    MemAndSize finalizerInputRepresentation;
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

    template <typename ContainerT, typename SeparatorT>
    static void append(ContainerT &dst, CIF::Builtins::BufferSimple *src, const SeparatorT *separator, size_t separatorLen) {
        if ((nullptr == src) || (src->GetSizeRaw() == 0)) {
            return;
        }
        if ((false == dst.empty()) && separator && (separatorLen > 0)) {
            dst.append(separator, separatorLen);
        }
        dst.append(src->GetMemory<char>(), src->GetSize<char>());
    }

    template <typename ContainerT, typename SeparatorT>
    static void append(ContainerT &dst, CIF::Builtins::BufferSimple *src, const SeparatorT *separator) {
        append(dst, src, separator, 1);
    }

    static void makeCopy(MemAndSize &dst, CIF::Builtins::BufferSimple *src);
};

struct SpecConstantInfo {
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> idsBuffer;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> sizesBuffer;
};

enum class CachingMode {
    none,
    direct,
    preProcess
};

class CompilerInterface : NEO::NonCopyableAndNonMovableClass {
  public:
    CompilerInterface();
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
    struct CompilerLibraryEntry {
        std::string revision;
        size_t libSize{};
        time_t libMTime{};
        std::unique_ptr<OsLibrary> library;
        CIF::RAII::UPtr_t<CIF::CIFMain> entryPoint;
    };

    MOCKABLE_VIRTUAL bool initialize(std::unique_ptr<CompilerCache> &&cache, bool requireFcl);
    MOCKABLE_VIRTUAL bool loadFcl();
    MOCKABLE_VIRTUAL bool loadIgcBasedCompiler(CompilerLibraryEntry &entryPoint, const char *libName);

    template <template <CIF::Version_t> class EntryPointT>
    std::once_flag &getIcbeVersionCallOnceFlag();

    static SpinLock spinlock;
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<SpinLock> lock() {
        return std::unique_lock<SpinLock>{spinlock};
    }
    std::unique_ptr<CompilerCache> cache;

    using igcDevCtxUptr = CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL>;
    using finalizerDevCtxUptr = CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL>;
    using fclDevCtxUptr = CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL>;

    CompilerLibraryEntry defaultIgc;
    std::mutex customCompilerLibraryLoadMutex;
    std::unordered_map<std::string, std::unique_ptr<CompilerLibraryEntry>> customCompilerLibraries;
    std::unordered_map<const Device *, igcDevCtxUptr> igcDeviceContexts;

    CompilerLibraryEntry fcl;
    std::unordered_map<const Device *, fclDevCtxUptr> fclDeviceContexts;
    CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> fclBaseTranslationCtx;

    std::unordered_map<const Device *, finalizerDevCtxUptr> finalizerDeviceContexts;
    IGC::CodeType::CodeType_t finalizerInputType = IGC::CodeType::undefined;

    MOCKABLE_VIRTUAL IGC::FclOclDeviceCtxTagOCL *getFclDeviceCtx(const Device &device);
    MOCKABLE_VIRTUAL IGC::IgcOclDeviceCtxTagOCL *getIgcDeviceCtx(const Device &device);
    MOCKABLE_VIRTUAL IGC::IgcOclDeviceCtxTagOCL *getFinalizerDeviceCtx(const Device &device);
    MOCKABLE_VIRTUAL IGC::CodeType::CodeType_t getPreferredIntermediateRepresentation(const Device &device);

    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> createFclTranslationCtx(const Device &device,
                                                                                                IGC::CodeType::CodeType_t inType,
                                                                                                IGC::CodeType::CodeType_t outType);
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> createIgcTranslationCtx(const Device &device,
                                                                                                IGC::CodeType::CodeType_t inType,
                                                                                                IGC::CodeType::CodeType_t outType);
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> createFinalizerTranslationCtx(const Device &device,
                                                                                                      IGC::CodeType::CodeType_t inType,
                                                                                                      IGC::CodeType::CodeType_t outType);
    bool isFclAvailable(const Device *device);
    bool isIgcAvailable(const Device *device);
    bool isFinalizerAvailable(const Device *device);
    bool useIgcAsFcl(const Device *device);

    const CompilerLibraryEntry *getCustomCompilerLibrary(const char *libName);

    const CompilerLibraryEntry *getIgc(const char *libName) {
        if (libName == nullptr) {
            if (defaultIgc.entryPoint == nullptr) {
                return nullptr;
            }
            return &defaultIgc;
        }

        return getCustomCompilerLibrary(libName);
    }

    const CompilerLibraryEntry *getIgc(const Device *device);

    const CompilerLibraryEntry *getFinalizer(const char *libName) {
        if (libName == nullptr) {
            return nullptr;
        }

        return getCustomCompilerLibrary(libName);
    }

    const CompilerLibraryEntry *getFinalizer(const Device *device);

    bool isCompilerAvailable(const Device *device, IGC::CodeType::CodeType_t translationSrc, IGC::CodeType::CodeType_t translationDst) {
        bool requiresFcl = (IGC::CodeType::oclC == translationSrc);
        bool requiresIgc = (IGC::CodeType::oclC != translationSrc) || ((IGC::CodeType::spirV != translationDst) && (IGC::CodeType::llvmBc != translationDst) && (IGC::CodeType::llvmLl != translationDst));
        bool requiresFinalizer = (finalizerInputType != IGC::CodeType::undefined) && ((translationDst == IGC::CodeType::oclGenBin) || (translationSrc == finalizerInputType));
        return (isFclAvailable(device) || (false == requiresFcl)) && (isIgcAvailable(device) || (false == requiresIgc)) && ((false == requiresFinalizer) || isFinalizerAvailable(device));
    }
};

static_assert(NEO::NonCopyableAndNonMovable<CompilerInterface>);

class CompilerCacheHelper {
  public:
    static void packAndCacheBinary(CompilerCache &compilerCache, const std::string &kernelFileHash, const NEO::TargetDevice &targetDevice, const NEO::TranslationOutput &translationOutput);
    static bool loadCacheAndSetOutput(CompilerCache &compilerCache, const std::string &kernelFileHash, NEO::TranslationOutput &output, const NEO::Device &device);
    static CachingMode getCachingMode(CompilerCache *compilerCache, IGC::CodeType::CodeType_t srcCodeType, const ArrayRef<const char> source);

  protected:
    static bool processPackedCacheBinary(ArrayRef<const uint8_t> archive, TranslationOutput &output, const NEO::Device &device);

    using WhitelistedIncludesVec = StackVec<std::string_view, 2>;
    static bool validateIncludes(const ArrayRef<const char> source, const WhitelistedIncludesVec &whitelistedIncludes);
    static WhitelistedIncludesVec whitelistedIncludes;
};

} // namespace NEO
