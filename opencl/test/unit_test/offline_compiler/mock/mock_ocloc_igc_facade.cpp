/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_igc_facade.h"

#include "shared/source/os_interface/os_library.h"

class OclocArgHelper;

namespace NEO {

MockOclocIgcFacade::MockOclocIgcFacade(OclocArgHelper *argHelper) : OclocIgcFacade(argHelper){};
MockOclocIgcFacade::~MockOclocIgcFacade() = default;

std::unique_ptr<OsLibrary> MockOclocIgcFacade::loadIgcLibrary(const char *libName) const {
    if (shouldFailLoadingOfIgcLib) {
        return nullptr;
    } else {
        return OclocIgcFacade::loadIgcLibrary(libName);
    }
}

CIF::CreateCIFMainFunc_t MockOclocIgcFacade::loadCreateIgcMainFunction() const {
    if (shouldFailLoadingOfIgcCreateMainFunction) {
        return nullptr;
    } else {
        return OclocIgcFacade::loadCreateIgcMainFunction();
    }
}

CIF::RAII::UPtr_t<CIF::CIFMain> MockOclocIgcFacade::createIgcMain(CIF::CreateCIFMainFunc_t createMainFunction) const {
    if (shouldFailCreationOfIgcMain) {
        return nullptr;
    } else {
        return OclocIgcFacade::createIgcMain(createMainFunction);
    }
}

bool MockOclocIgcFacade::isIgcInterfaceCompatible(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const {
    if (isIgcInterfaceCompatibleReturnValue.has_value()) {
        return *isIgcInterfaceCompatibleReturnValue;
    } else {
        return OclocIgcFacade::isIgcInterfaceCompatible(interfacesToIgnore);
    }
}

std::string MockOclocIgcFacade::getIncompatibleInterface(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const {
    if (getIncompatibleInterfaceReturnValue.has_value()) {
        return *getIncompatibleInterfaceReturnValue;
    } else {
        return OclocIgcFacade::getIncompatibleInterface(interfacesToIgnore);
    }
}

bool MockOclocIgcFacade::isPatchtokenInterfaceSupported() const {
    if (isPatchtokenInterfaceSupportedReturnValue.has_value()) {
        return *isPatchtokenInterfaceSupportedReturnValue;
    } else {
        return OclocIgcFacade::isPatchtokenInterfaceSupported();
    }
}

CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> MockOclocIgcFacade::createIgcDeviceContext() const {
    if (shouldFailCreationOfIgcDeviceContext) {
        return nullptr;
    } else {
        return OclocIgcFacade::createIgcDeviceContext();
    }
}

CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtx<3>> MockOclocIgcFacade::createIgcDeviceContext3() const {
    if (shouldFailCreationOfIgcDeviceContext3) {
        return nullptr;
    } else {
        return OclocIgcFacade::createIgcDeviceContext3();
    }
}

CIF::RAII::UPtr_t<IGC::PlatformTagOCL> MockOclocIgcFacade::getIgcPlatformHandle() const {
    if (shouldReturnInvalidIgcPlatformHandle) {
        return nullptr;
    } else {
        return OclocIgcFacade::getIgcPlatformHandle();
    }
}

CIF::RAII::UPtr_t<IGC::GTSystemInfoTagOCL> MockOclocIgcFacade::getGTSystemInfoHandle() const {
    if (shouldReturnInvalidGTSystemInfoHandle) {
        return nullptr;
    } else {
        return OclocIgcFacade::getGTSystemInfoHandle();
    }
}

CIF::RAII::UPtr_t<IGC::IgcFeaturesAndWorkaroundsTagOCL> MockOclocIgcFacade::getIgcFeaturesAndWorkaroundsHandle() const {
    if (shouldReturnInvalidIgcFeaturesAndWorkaroundsHandle) {
        return nullptr;
    } else {
        return OclocIgcFacade::getIgcFeaturesAndWorkaroundsHandle();
    }
}

} // namespace NEO