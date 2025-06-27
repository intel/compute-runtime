/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_fcl_facade.h"

#include "shared/source/os_interface/os_library.h"

#include <optional>
#include <string>

class OclocArgHelper;

namespace NEO {

MockOclocFclFacade::MockOclocFclFacade(OclocArgHelper *argHelper) : OclocFclFacade(argHelper){};
MockOclocFclFacade::~MockOclocFclFacade() = default;

std::unique_ptr<OsLibrary> MockOclocFclFacade::loadFclLibrary() const {
    if (shouldFailLoadingOfFclLib) {
        return nullptr;
    } else {
        return OclocFclFacade::loadFclLibrary();
    }
}

CIF::CreateCIFMainFunc_t MockOclocFclFacade::loadCreateFclMainFunction() const {
    if (shouldFailLoadingOfFclCreateMainFunction) {
        return nullptr;
    } else {
        return OclocFclFacade::loadCreateFclMainFunction();
    }
}

CIF::RAII::UPtr_t<CIF::CIFMain> MockOclocFclFacade::createFclMain(CIF::CreateCIFMainFunc_t createMainFunction) const {
    if (shouldFailCreationOfFclMain) {
        return nullptr;
    } else {
        return OclocFclFacade::createFclMain(createMainFunction);
    }
}

bool MockOclocFclFacade::isFclInterfaceCompatible() const {
    if (isFclInterfaceCompatibleReturnValue.has_value()) {
        return *isFclInterfaceCompatibleReturnValue;
    } else {
        return OclocFclFacade::isFclInterfaceCompatible();
    }
}

std::string MockOclocFclFacade::getIncompatibleInterface() const {
    if (getIncompatibleInterfaceReturnValue.has_value()) {
        return *getIncompatibleInterfaceReturnValue;
    } else {
        return OclocFclFacade::getIncompatibleInterface();
    }
}

CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> MockOclocFclFacade::createFclDeviceContext() const {
    if (shouldFailCreationOfFclDeviceContext) {
        return nullptr;
    } else {
        return OclocFclFacade::createFclDeviceContext();
    }
}

bool MockOclocFclFacade::shouldPopulateFclInterface() const {
    if (shouldPopulateFclInterfaceReturnValue.has_value()) {
        return *shouldPopulateFclInterfaceReturnValue;
    } else {
        return OclocFclFacade::shouldPopulateFclInterface();
    }
}

CIF::RAII::UPtr_t<IGC::PlatformTagOCL> MockOclocFclFacade::getPlatformHandle() const {
    if (shouldReturnInvalidFclPlatformHandle) {
        return nullptr;
    } else {
        return OclocFclFacade::getPlatformHandle();
    }
}

void MockOclocFclFacade::populateFclInterface(IGC::PlatformTagOCL &handle, const HardwareInfo &hwInfo) {
    ++populateFclInterfaceCalledCount;
    OclocFclFacade::populateFclInterface(handle, hwInfo);
}

} // namespace NEO
