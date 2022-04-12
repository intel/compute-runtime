/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_igc_facade.h"

#include <optional>
#include <string>

namespace NEO {

class MockOclocIgcFacade : public OclocIgcFacade {
  public:
    using OclocIgcFacade::igcDeviceCtx;

    bool shouldFailLoadingOfIgcLib{false};
    bool shouldFailLoadingOfIgcCreateMainFunction{false};
    bool shouldFailCreationOfIgcMain{false};
    bool shouldFailCreationOfIgcDeviceContext{false};
    bool shouldReturnInvalidIgcPlatformHandle{false};
    bool shouldReturnInvalidGTSystemInfoHandle{false};
    bool shouldReturnInvalidIgcFeaturesAndWorkaroundsHandle{false};
    std::optional<bool> isIgcInterfaceCompatibleReturnValue{};
    std::optional<std::string> getIncompatibleInterfaceReturnValue{};
    std::optional<bool> isPatchtokenInterfaceSupportedReturnValue{};

    MockOclocIgcFacade(OclocArgHelper *argHelper) : OclocIgcFacade{argHelper} {}
    ~MockOclocIgcFacade() override = default;

    std::unique_ptr<OsLibrary> loadIgcLibrary() const override {
        if (shouldFailLoadingOfIgcLib) {
            return nullptr;
        } else {
            return OclocIgcFacade::loadIgcLibrary();
        }
    }

    CIF::CreateCIFMainFunc_t loadCreateIgcMainFunction() const override {
        if (shouldFailLoadingOfIgcCreateMainFunction) {
            return nullptr;
        } else {
            return OclocIgcFacade::loadCreateIgcMainFunction();
        }
    }

    CIF::RAII::UPtr_t<CIF::CIFMain> createIgcMain(CIF::CreateCIFMainFunc_t createMainFunction) const override {
        if (shouldFailCreationOfIgcMain) {
            return nullptr;
        } else {
            return OclocIgcFacade::createIgcMain(createMainFunction);
        }
    }

    bool isIgcInterfaceCompatible(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const override {
        if (isIgcInterfaceCompatibleReturnValue.has_value()) {
            return *isIgcInterfaceCompatibleReturnValue;
        } else {
            return OclocIgcFacade::isIgcInterfaceCompatible(interfacesToIgnore);
        }
    }

    std::string getIncompatibleInterface(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const override {
        if (getIncompatibleInterfaceReturnValue.has_value()) {
            return *getIncompatibleInterfaceReturnValue;
        } else {
            return OclocIgcFacade::getIncompatibleInterface(interfacesToIgnore);
        }
    }

    bool isPatchtokenInterfaceSupported() const override {
        if (isPatchtokenInterfaceSupportedReturnValue.has_value()) {
            return *isPatchtokenInterfaceSupportedReturnValue;
        } else {
            return OclocIgcFacade::isPatchtokenInterfaceSupported();
        }
    }

    CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> createIgcDeviceContext() const override {
        if (shouldFailCreationOfIgcDeviceContext) {
            return nullptr;
        } else {
            return OclocIgcFacade::createIgcDeviceContext();
        }
    }

    CIF::RAII::UPtr_t<IGC::PlatformTagOCL> getIgcPlatformHandle() const override {
        if (shouldReturnInvalidIgcPlatformHandle) {
            return nullptr;
        } else {
            return OclocIgcFacade::getIgcPlatformHandle();
        }
    }

    CIF::RAII::UPtr_t<IGC::GTSystemInfoTagOCL> getGTSystemInfoHandle() const override {
        if (shouldReturnInvalidGTSystemInfoHandle) {
            return nullptr;
        } else {
            return OclocIgcFacade::getGTSystemInfoHandle();
        }
    }

    CIF::RAII::UPtr_t<IGC::IgcFeaturesAndWorkaroundsTagOCL> getIgcFeaturesAndWorkaroundsHandle() const override {
        if (shouldReturnInvalidIgcFeaturesAndWorkaroundsHandle) {
            return nullptr;
        } else {
            return OclocIgcFacade::getIgcFeaturesAndWorkaroundsHandle();
        }
    }
};

} // namespace NEO