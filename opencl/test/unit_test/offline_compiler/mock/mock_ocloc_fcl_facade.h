/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_fcl_facade.h"

#include <optional>
#include <string>

namespace NEO {

class MockOclocFclFacade : public OclocFclFacade {
  public:
    using OclocFclFacade::fclDeviceCtx;

    bool shouldFailLoadingOfFclLib{false};
    bool shouldFailLoadingOfFclCreateMainFunction{false};
    bool shouldFailCreationOfFclMain{false};
    bool shouldFailCreationOfFclDeviceContext{false};
    bool shouldReturnInvalidFclPlatformHandle{false};
    std::optional<bool> isFclInterfaceCompatibleReturnValue{};
    std::optional<std::string> getIncompatibleInterfaceReturnValue{};
    std::optional<bool> shouldPopulateFclInterfaceReturnValue{};
    int populateFclInterfaceCalledCount{0};

    MockOclocFclFacade(OclocArgHelper *argHelper) : OclocFclFacade{argHelper} {}
    ~MockOclocFclFacade() override = default;

    std::unique_ptr<OsLibrary> loadFclLibrary() const override {
        if (shouldFailLoadingOfFclLib) {
            return nullptr;
        } else {
            return OclocFclFacade::loadFclLibrary();
        }
    }

    CIF::CreateCIFMainFunc_t loadCreateFclMainFunction() const override {
        if (shouldFailLoadingOfFclCreateMainFunction) {
            return nullptr;
        } else {
            return OclocFclFacade::loadCreateFclMainFunction();
        }
    }

    CIF::RAII::UPtr_t<CIF::CIFMain> createFclMain(CIF::CreateCIFMainFunc_t createMainFunction) const override {
        if (shouldFailCreationOfFclMain) {
            return nullptr;
        } else {
            return OclocFclFacade::createFclMain(createMainFunction);
        }
    }

    bool isFclInterfaceCompatible() const override {
        if (isFclInterfaceCompatibleReturnValue.has_value()) {
            return *isFclInterfaceCompatibleReturnValue;
        } else {
            return OclocFclFacade::isFclInterfaceCompatible();
        }
    }

    std::string getIncompatibleInterface() const override {
        if (getIncompatibleInterfaceReturnValue.has_value()) {
            return *getIncompatibleInterfaceReturnValue;
        } else {
            return OclocFclFacade::getIncompatibleInterface();
        }
    }

    CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> createFclDeviceContext() const override {
        if (shouldFailCreationOfFclDeviceContext) {
            return nullptr;
        } else {
            return OclocFclFacade::createFclDeviceContext();
        }
    }

    bool shouldPopulateFclInterface() const override {
        if (shouldPopulateFclInterfaceReturnValue.has_value()) {
            return *shouldPopulateFclInterfaceReturnValue;
        } else {
            return OclocFclFacade::shouldPopulateFclInterface();
        }
    }

    CIF::RAII::UPtr_t<IGC::PlatformTagOCL> getPlatformHandle() const override {
        if (shouldReturnInvalidFclPlatformHandle) {
            return nullptr;
        } else {
            return OclocFclFacade::getPlatformHandle();
        }
    }

    void populateFclInterface(IGC::PlatformTagOCL &handle, const PLATFORM &platform) override {
        ++populateFclInterfaceCalledCount;
        OclocFclFacade::populateFclInterface(handle, platform);
    }
};

} // namespace NEO