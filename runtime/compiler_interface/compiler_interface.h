/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/sip.h"
#include "runtime/compiler_interface/binary_cache.h"
#include "runtime/os_interface/os_library.h"

#include "CL/cl_platform.h"
#include "cif/common/cif_main.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <map>
#include <mutex>

namespace NEO {
class Device;
class Program;

struct TranslationArgs {
    char *pInput = nullptr;                 // data to be translated
    uint32_t InputSize = 0;                 // size of data to be translated
    const char *pOptions = nullptr;         // list of build/compile options
    uint32_t OptionsSize = 0;               // size of options list
    const char *pInternalOptions = nullptr; // list of build/compile options
    uint32_t InternalOptionsSize = 0;       // size of options list
    void *pTracingOptions = nullptr;        // instrumentation options
    uint32_t TracingOptionsCount = 0;       // number of instrumentation options
    void *GTPinInput = nullptr;             // input structure for GTPin requests
};

class CompilerInterface {
  public:
    CompilerInterface();
    CompilerInterface(const CompilerInterface &) = delete;
    CompilerInterface &operator=(const CompilerInterface &) = delete;
    virtual ~CompilerInterface();

    static CompilerInterface *createInstance() {
        auto instance = new CompilerInterface();
        if (!instance->initialize()) {
            delete instance;
            instance = nullptr;
        }
        return instance;
    }

    MOCKABLE_VIRTUAL cl_int build(Program &program, const TranslationArgs &pInputArgs, bool enableCaching);

    MOCKABLE_VIRTUAL cl_int compile(Program &program, const TranslationArgs &pInputArgs);

    MOCKABLE_VIRTUAL cl_int link(Program &program, const TranslationArgs &pInputArgs);

    cl_int createLibrary(Program &program, const TranslationArgs &pInputArgs);

    MOCKABLE_VIRTUAL cl_int getSipKernelBinary(SipKernelType kernel, const Device &device, std::vector<char> &retBinary);

    BinaryCache *replaceBinaryCache(BinaryCache *newCache);

  protected:
    bool initialize();

    static std::mutex mtx;
    MOCKABLE_VIRTUAL std::unique_lock<std::mutex> lock() {
        return std::unique_lock<std::mutex>{mtx};
    }
    std::unique_ptr<BinaryCache> cache = nullptr;

    static bool useLlvmText;

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
    MOCKABLE_VIRTUAL IGC::CodeType::CodeType_t getPreferredIntermediateRepresentation(const Device &device);

    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> createFclTranslationCtx(const Device &device,
                                                                                                IGC::CodeType::CodeType_t inType,
                                                                                                IGC::CodeType::CodeType_t outType);
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> createIgcTranslationCtx(const Device &device,
                                                                                                IGC::CodeType::CodeType_t inType,
                                                                                                IGC::CodeType::CodeType_t outType);

    bool isCompilerAvailable() const {
        return (fclMain != nullptr) && (igcMain != nullptr);
    }
};
} // namespace NEO
