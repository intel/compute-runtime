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
#include "cif/common/cif_main.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "runtime/built_ins/sip.h"
#include "runtime/compiler_interface/binary_cache.h"
#include "runtime/os_interface/os_library.h"

#include "CL/cl_platform.h"
#include <map>
#include <mutex>

namespace OCLRT {
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

    static void shutdown() {
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
} // namespace OCLRT
