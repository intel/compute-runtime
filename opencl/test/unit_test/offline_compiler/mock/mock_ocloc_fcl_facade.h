/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_fcl_facade.h"

#include <optional>
#include <string>

class OclocArgHelper;

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

    MockOclocFclFacade(OclocArgHelper *argHelper);
    ~MockOclocFclFacade() override;

    std::unique_ptr<OsLibrary> loadFclLibrary() const override;
    CIF::CreateCIFMainFunc_t loadCreateFclMainFunction() const override;

    CIF::RAII::UPtr_t<CIF::CIFMain> createFclMain(CIF::CreateCIFMainFunc_t createMainFunction) const override;

    bool isFclInterfaceCompatible() const override;

    std::string getIncompatibleInterface() const override;

    CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> createFclDeviceContext() const override;

    bool shouldPopulateFclInterface() const override;

    CIF::RAII::UPtr_t<IGC::PlatformTagOCL> getPlatformHandle() const override;

    void populateFclInterface(IGC::PlatformTagOCL &handle, const HardwareInfo &hwInfo) override;
};

} // namespace NEO
