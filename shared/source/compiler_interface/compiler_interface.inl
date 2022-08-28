/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/validators.h"
#include "shared/source/os_interface/os_library.h"

#include "cif/builtins/memory/buffer/buffer.h"
#include "cif/common/cif.h"
#include "cif/import/library_api.h"
#include "ocl_igc_interface/ocl_translation_output.h"

namespace NEO {
using CIFBuffer = CIF::Builtins::BufferSimple;

template <typename TranslationCtx>
inline CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> translate(TranslationCtx *tCtx, CIFBuffer *src, CIFBuffer *options,
                                                                    CIFBuffer *internalOptions) {
    if (false == NEO::areNotNullptr(tCtx, src, options, internalOptions)) {
        return nullptr;
    }

    auto ret = tCtx->Translate(src, options, internalOptions, nullptr, 0);
    if (ret == nullptr) {
        return nullptr; // assume OOM or internal error
    }

    if ((ret->GetOutput() == nullptr) || (ret->GetBuildLog() == nullptr) || (ret->GetDebugData() == nullptr)) {
        return nullptr; // assume OOM or internal error
    }

    return ret;
}
template <typename TranslationCtx>
inline CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> translate(TranslationCtx *tCtx, CIFBuffer *src, CIFBuffer *options,
                                                                    CIFBuffer *internalOptions, void *gtpinInit) {
    if (false == NEO::areNotNullptr(tCtx, src, options, internalOptions)) {
        return nullptr;
    }

    auto ret = tCtx->Translate(src, options, internalOptions, nullptr, 0, gtpinInit);
    if (ret == nullptr) {
        return nullptr; // assume OOM or internal error
    }

    if ((ret->GetOutput() == nullptr) || (ret->GetBuildLog() == nullptr) || (ret->GetDebugData() == nullptr)) {
        return nullptr; // assume OOM or internal error
    }

    return ret;
}

template <typename TranslationCtx>
inline bool getSpecConstantsInfoImpl(TranslationCtx *tCtx,
                                     CIFBuffer *src,
                                     CIFBuffer *outSpecConstantsIds,
                                     CIFBuffer *outSpecConstantsSizes) {
    if (!NEO::areNotNullptr(tCtx, src, outSpecConstantsIds, outSpecConstantsSizes)) {
        return false;
    }
    return tCtx->GetSpecConstantsInfoImpl(src, outSpecConstantsIds, outSpecConstantsSizes);
}

template <typename TranslationCtx>
inline CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> translate(TranslationCtx *tCtx, CIFBuffer *src, CIFBuffer *specConstantsIds, CIFBuffer *specConstantsValues, CIFBuffer *options,
                                                                    CIFBuffer *internalOptions, void *gtpinInit) {
    if (false == NEO::areNotNullptr(tCtx, src, options, internalOptions)) {
        return nullptr;
    }

    auto ret = tCtx->Translate(src, specConstantsIds, specConstantsValues, options, internalOptions, nullptr, 0, gtpinInit);
    if (ret == nullptr) {
        return nullptr; // assume OOM or internal error
    }

    if (!NEO::areNotNullptr(ret->GetOutput(), ret->GetBuildLog(), ret->GetDebugData())) {
        return nullptr; // assume OOM or internal error
    }

    return ret;
}

CIF::CIFMain *createMainNoSanitize(CIF::CreateCIFMainFunc_t createFunc);

template <template <CIF::Version_t> class EntryPointT>
inline bool loadCompiler(const char *libName, std::unique_ptr<OsLibrary> &outLib,
                         CIF::RAII::UPtr_t<CIF::CIFMain> &outLibMain) {
    std::string loadLibraryError;
    auto lib = std::unique_ptr<OsLibrary>(OsLibrary::load(libName, &loadLibraryError));
    if (lib == nullptr) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Compiler Library %s could not be loaded with error: %s\n", libName, loadLibraryError.c_str());
        DEBUG_BREAK_IF(true); // could not load library
        return false;
    }

    auto createMain = reinterpret_cast<CIF::CreateCIFMainFunc_t>(lib->getProcAddress(CIF::CreateCIFMainFuncName));
    UNRECOVERABLE_IF(createMain == nullptr); // invalid compiler library

    auto main = CIF::RAII::UPtr(createMainNoSanitize(createMain));
    if (main == nullptr) {
        DEBUG_BREAK_IF(true); // could not create main entry point
        return false;
    }

    std::vector<CIF::InterfaceId_t> interfacesToIgnore;
    if (DebugManager.flags.ZebinIgnoreIcbeVersion.get()) {
        interfacesToIgnore.push_back(IGC::OclGenBinaryBase::GetInterfaceId());
    }
    if (false == main->IsCompatible<EntryPointT>(&interfacesToIgnore)) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Installed Compiler Library %s is incompatible\n", libName);
        DEBUG_BREAK_IF(true); // given compiler library is not compatible
        return false;
    }

    outLib = std::move(lib);
    outLibMain = std::move(main);

    return true;
}

} // namespace NEO
