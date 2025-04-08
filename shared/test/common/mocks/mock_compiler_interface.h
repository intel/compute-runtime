/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/sip.h"
#include "shared/source/compiler_interface/compiler_interface.h"

#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <functional>
#include <map>
#include <optional>
#include <string>

namespace NEO {

class MockCompilerInterface : public CompilerInterface {
  public:
    using CompilerInterface::cache;
    using CompilerInterface::fclBaseTranslationCtx;
    using CompilerInterface::fclDeviceContexts;
    using CompilerInterface::initialize;
    using CompilerInterface::isCompilerAvailable;
    using CompilerInterface::isFclAvailable;
    using CompilerInterface::isIgcAvailable;

    using CompilerInterface::defaultIgc;
    using CompilerInterface::fcl;

    using CompilerInterface::customCompilerLibraries;
    using CompilerInterface::finalizerDeviceContexts;
    using CompilerInterface::useIgcAsFcl;

    bool loadFcl() override {
        if (failLoadFcl) {
            return false;
        }
        return CompilerInterface::loadFcl();
    }

    bool loadIgcBasedCompiler(CompilerLibraryEntry &entryPoint, const char *libName) override {
        if (failLoadIgc) {
            return false;
        }

        if (igcLibraryNameOverride.has_value()) {
            libName = *igcLibraryNameOverride;
        }

        return CompilerInterface::loadIgcBasedCompiler(entryPoint, libName);
    }

    void setFclDeviceCtx(const Device &d, IGC::FclOclDeviceCtxTagOCL *ctx) {
        this->fclDeviceContexts[&d] = CIF::RAII::RetainAndPack<IGC::FclOclDeviceCtxTagOCL>(ctx);
    }

    std::unordered_map<const Device *, fclDevCtxUptr> &getFclDeviceContexts() {
        return this->fclDeviceContexts;
    }

    void setIgcDeviceCtx(const Device &d, IGC::IgcOclDeviceCtxTagOCL *ctx) {
        this->igcDeviceContexts[&d] = CIF::RAII::RetainAndPack<IGC::IgcOclDeviceCtxTagOCL>(ctx);
    }

    void setFinalizerDeviceCtx(const Device &d, IGC::IgcOclDeviceCtxTagOCL *ctx) {
        this->finalizerDeviceContexts[&d] = CIF::RAII::RetainAndPack<IGC::IgcOclDeviceCtxTagOCL>(ctx);
    }

    std::unordered_map<const Device *, igcDevCtxUptr> &getIgcDeviceContexts() {
        return this->igcDeviceContexts;
    }

    void setDeviceCtx(const Device &d, IGC::IgcOclDeviceCtxTagOCL *ctx) {
        setIgcDeviceCtx(d, ctx);
    }

    void setDeviceCtx(const Device &d, IGC::FclOclDeviceCtxTagOCL *ctx) {
        setFclDeviceCtx(d, ctx);
    }

    template <typename DeviceCtx>
    std::unordered_map<const Device *, CIF::RAII::UPtr_t<DeviceCtx>> &getDeviceContexts();

    std::unique_lock<SpinLock> lock() override {
        if (lockListener != nullptr) {
            lockListener(*this);
        }

        return std::unique_lock<SpinLock>(spinlock);
    }

    void setIgcMain(CIF::CIFMain *main) {
        this->defaultIgc.entryPoint.release();
        this->defaultIgc.entryPoint.reset(main);
    }

    void setFclMain(CIF::CIFMain *main) {
        this->fcl.entryPoint.release();
        this->fcl.entryPoint.reset(main);
    }

    IGC::IgcOclDeviceCtxTagOCL *getIgcDeviceCtx(const Device &device) override {
        if (failGetIgcDeviceCtx) {
            return nullptr;
        }

        return CompilerInterface::getIgcDeviceCtx(device);
    }

    CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> createFclTranslationCtx(const Device &device,
                                                                               IGC::CodeType::CodeType_t inType,
                                                                               IGC::CodeType::CodeType_t outType) override {
        requestedTranslationCtxs.emplace_back(inType, outType);
        if (failCreateFclTranslationCtx) {
            return nullptr;
        }

        return CompilerInterface::createFclTranslationCtx(device, inType, outType);
    }

    CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> createIgcTranslationCtx(const Device &device,
                                                                               IGC::CodeType::CodeType_t inType,
                                                                               IGC::CodeType::CodeType_t outType) override {
        requestedTranslationCtxs.emplace_back(inType, outType);
        if (failCreateIgcTranslationCtx) {
            return nullptr;
        }

        return CompilerInterface::createIgcTranslationCtx(device, inType, outType);
    }

    CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> createFinalizerTranslationCtx(const Device &device,
                                                                                     IGC::CodeType::CodeType_t inType,
                                                                                     IGC::CodeType::CodeType_t outType) override {
        requestedTranslationCtxs.emplace_back(inType, outType);
        if (failCreateFinalizerTranslationCtx) {
            return nullptr;
        }

        return CompilerInterface::createFinalizerTranslationCtx(device, inType, outType);
    }

    IGC::FclOclTranslationCtxTagOCL *getFclBaseTranslationCtx() {
        return this->fclBaseTranslationCtx.get();
    }

    TranslationOutput::ErrorCode getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary,
                                                    std::vector<char> &stateAreaHeader) override {
        if (this->sipKernelBinaryOverride.size() > 0) {
            retBinary = this->sipKernelBinaryOverride;
            this->requestedSipKernel = type;
            return TranslationOutput::ErrorCode::success;
        } else {
            return CompilerInterface::getSipKernelBinary(device, type, retBinary, stateAreaHeader);
        }
    }

    CIF::RAII::UPtr_t<IGC::IgcFeaturesAndWorkaroundsTagOCL> getIgcFeaturesAndWorkarounds(const NEO::Device &device) override {
        if (nullptr != igcFeaturesAndWorkaroundsTagOCL) {
            return CIF::RAII::RetainAndPack<IGC::IgcFeaturesAndWorkaroundsTagOCL>(igcFeaturesAndWorkaroundsTagOCL);
        } else {
            return CompilerInterface::getIgcFeaturesAndWorkarounds(device);
        }
    };

    static std::vector<char> getDummyGenBinary();
    static void releaseDummyGenBinary();

    void (*lockListener)(MockCompilerInterface &compInt) = nullptr;
    void *lockListenerData = nullptr;
    IGC::IgcFeaturesAndWorkaroundsTagOCL *igcFeaturesAndWorkaroundsTagOCL = nullptr;
    bool failCreateFclTranslationCtx = false;
    bool failCreateIgcTranslationCtx = false;
    bool failCreateFinalizerTranslationCtx = false;
    bool failLoadFcl = false;
    bool failLoadIgc = false;
    bool failGetIgcDeviceCtx = false;
    std::optional<const char *> igcLibraryNameOverride;

    using TranslationOpT = std::pair<IGC::CodeType::CodeType_t, IGC::CodeType::CodeType_t>;
    std::vector<TranslationOpT> requestedTranslationCtxs;

    std::vector<char> sipKernelBinaryOverride;
    SipKernelType requestedSipKernel = SipKernelType::count;

    IGC::IgcOclDeviceCtxTagOCL *peekIgcDeviceCtx(Device *device) { return igcDeviceContexts[device].get(); }
};

template <>
inline std::unordered_map<const Device *, MockCompilerInterface::igcDevCtxUptr> &MockCompilerInterface::getDeviceContexts<IGC::IgcOclDeviceCtxTagOCL>() {
    return getIgcDeviceContexts();
}

template <>
inline std::unordered_map<const Device *, MockCompilerInterface::fclDevCtxUptr> &MockCompilerInterface::getDeviceContexts<IGC::FclOclDeviceCtxTagOCL>() {
    return getFclDeviceContexts();
}

struct MockCompilerInterfaceCaptureBuildOptions : CompilerInterface {
    TranslationOutput::ErrorCode compile(const NEO::Device &device, const TranslationInput &input, TranslationOutput &out) override {
        buildOptions.clear();
        if ((input.apiOptions.size() > 0) && (input.apiOptions.begin() != nullptr)) {
            buildOptions.assign(input.apiOptions.begin(), input.apiOptions.end());
        }
        buildInternalOptions.clear();
        if ((input.internalOptions.size() > 0) && (input.internalOptions.begin() != nullptr)) {
            buildInternalOptions.assign(input.internalOptions.begin(), input.internalOptions.end());
        }

        auto copy = [](TranslationOutput::MemAndSize &dst, TranslationOutput::MemAndSize &src) {
            if (src.size > 0) {
                dst.size = src.size;
                dst.mem.reset(new char[src.size]);
                std::memcpy(dst.mem.get(), src.mem.get(), src.size);
            }
        };
        copy(out.debugData, output.debugData);
        copy(out.intermediateRepresentation, output.intermediateRepresentation);
        copy(out.deviceBinary, output.intermediateRepresentation);
        out.intermediateCodeType = output.intermediateCodeType;

        return TranslationOutput::ErrorCode::success;
    }

    TranslationOutput::ErrorCode build(const NEO::Device &device, const TranslationInput &input, TranslationOutput &out) override {
        return this->MockCompilerInterfaceCaptureBuildOptions::compile(device, input, out);
    }

    TranslationOutput::ErrorCode link(const NEO::Device &device,
                                      const TranslationInput &input,
                                      TranslationOutput &output) override {
        return this->MockCompilerInterfaceCaptureBuildOptions::compile(device, input, output);
    }

    TranslationOutput output;
    std::string buildOptions;
    std::string buildInternalOptions;
};
} // namespace NEO
