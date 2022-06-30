/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_interface.h"

#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <functional>
#include <map>
#include <string>

namespace NEO {

class MockCompilerInterface : public CompilerInterface {
  public:
    using CompilerInterface::fclDeviceContexts;
    using CompilerInterface::initialize;
    using CompilerInterface::isCompilerAvailable;
    using CompilerInterface::isFclAvailable;
    using CompilerInterface::isIgcAvailable;

    using CompilerInterface::fclMain;
    using CompilerInterface::igcMain;

    bool loadFcl() override {
        if (failLoadFcl) {
            return false;
        }
        return CompilerInterface::loadFcl();
    }

    bool loadIgc() override {
        if (failLoadIgc) {
            return false;
        }
        return CompilerInterface::loadIgc();
    }

    void setFclDeviceCtx(const Device &d, IGC::FclOclDeviceCtxTagOCL *ctx) {
        this->fclDeviceContexts[&d] = CIF::RAII::RetainAndPack<IGC::FclOclDeviceCtxTagOCL>(ctx);
    }

    std::map<const Device *, fclDevCtxUptr> &getFclDeviceContexts() {
        return this->fclDeviceContexts;
    }

    void setIgcDeviceCtx(const Device &d, IGC::IgcOclDeviceCtxTagOCL *ctx) {
        this->igcDeviceContexts[&d] = CIF::RAII::RetainAndPack<IGC::IgcOclDeviceCtxTagOCL>(ctx);
    }

    std::map<const Device *, igcDevCtxUptr> &getIgcDeviceContexts() {
        return this->igcDeviceContexts;
    }

    void setDeviceCtx(const Device &d, IGC::IgcOclDeviceCtxTagOCL *ctx) {
        setIgcDeviceCtx(d, ctx);
    }

    void setDeviceCtx(const Device &d, IGC::FclOclDeviceCtxTagOCL *ctx) {
        setFclDeviceCtx(d, ctx);
    }

    template <typename DeviceCtx>
    std::map<const Device *, CIF::RAII::UPtr_t<DeviceCtx>> &getDeviceContexts();

    std::unique_lock<SpinLock> lock() override {
        if (lockListener != nullptr) {
            lockListener(*this);
        }

        return std::unique_lock<SpinLock>(spinlock);
    }

    void setIgcMain(CIF::CIFMain *main) {
        this->igcMain.release();
        this->igcMain.reset(main);
    }

    void setFclMain(CIF::CIFMain *main) {
        this->fclMain.release();
        this->fclMain.reset(main);
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

    IGC::FclOclTranslationCtxTagOCL *getFclBaseTranslationCtx() {
        return this->fclBaseTranslationCtx.get();
    }

    TranslationOutput::ErrorCode getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary,
                                                    std::vector<char> &stateAreaHeader) override {
        if (this->sipKernelBinaryOverride.size() > 0) {
            retBinary = this->sipKernelBinaryOverride;
            this->requestedSipKernel = type;
            return TranslationOutput::ErrorCode::Success;
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
    bool failLoadFcl = false;
    bool failLoadIgc = false;
    bool failGetIgcDeviceCtx = false;

    using TranslationOpT = std::pair<IGC::CodeType::CodeType_t, IGC::CodeType::CodeType_t>;
    std::vector<TranslationOpT> requestedTranslationCtxs;

    std::vector<char> sipKernelBinaryOverride;
    SipKernelType requestedSipKernel = SipKernelType::COUNT;

    IGC::IgcOclDeviceCtxTagOCL *peekIgcDeviceCtx(Device *device) { return igcDeviceContexts[device].get(); }
};

template <>
inline std::map<const Device *, MockCompilerInterface::igcDevCtxUptr> &MockCompilerInterface::getDeviceContexts<IGC::IgcOclDeviceCtxTagOCL>() {
    return getIgcDeviceContexts();
}

template <>
inline std::map<const Device *, MockCompilerInterface::fclDevCtxUptr> &MockCompilerInterface::getDeviceContexts<IGC::FclOclDeviceCtxTagOCL>() {
    return getFclDeviceContexts();
}

struct MockCompilerInterfaceCaptureBuildOptions : CompilerInterface {
    TranslationOutput::ErrorCode compile(const NEO::Device &device, const TranslationInput &input, TranslationOutput &) override {
        buildOptions.clear();
        if ((input.apiOptions.size() > 0) && (input.apiOptions.begin() != nullptr)) {
            buildOptions.assign(input.apiOptions.begin(), input.apiOptions.end());
        }
        buildInternalOptions.clear();
        if ((input.internalOptions.size() > 0) && (input.internalOptions.begin() != nullptr)) {
            buildInternalOptions.assign(input.internalOptions.begin(), input.internalOptions.end());
        }
        return TranslationOutput::ErrorCode::Success;
    }

    TranslationOutput::ErrorCode build(const NEO::Device &device, const TranslationInput &input, TranslationOutput &out) override {
        return this->MockCompilerInterfaceCaptureBuildOptions::compile(device, input, out);
    }

    TranslationOutput::ErrorCode link(const NEO::Device &device,
                                      const TranslationInput &input,
                                      TranslationOutput &output) override {
        return this->MockCompilerInterfaceCaptureBuildOptions::compile(device, input, output);
    }

    std::string buildOptions;
    std::string buildInternalOptions;
};
} // namespace NEO
