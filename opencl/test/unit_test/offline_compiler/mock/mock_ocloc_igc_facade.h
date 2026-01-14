/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_igc_facade.h"

#include <optional>
#include <string>

class OclocArgHelper;

namespace NEO {

class MockOclocIgcFacade : public OclocIgcFacade {
  public:
    using OclocIgcFacade::igcDeviceCtx;

    bool shouldFailLoadingOfIgcLib{false};
    bool shouldFailLoadingOfIgcCreateMainFunction{false};
    bool shouldFailCreationOfIgcMain{false};
    bool shouldFailCreationOfIgcDeviceContext{false};
    std::optional<bool> isIgcInterfaceCompatibleReturnValue{};
    std::optional<std::string> getIncompatibleInterfaceReturnValue{};
    std::optional<bool> isPatchtokenInterfaceSupportedReturnValue{};

    MockOclocIgcFacade(OclocArgHelper *argHelper);
    ~MockOclocIgcFacade() override;

    std::unique_ptr<OsLibrary> loadIgcLibrary(const char *libName) const override;

    CIF::CreateCIFMainFunc_t loadCreateIgcMainFunction() const override;

    CIF::RAII::UPtr_t<CIF::CIFMain> createIgcMain(CIF::CreateCIFMainFunc_t createMainFunction) const override;

    bool isIgcInterfaceCompatible(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const override;

    std::string getIncompatibleInterface(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const override;

    bool isPatchtokenInterfaceSupported() const override;

    CIF::RAII::UPtr_t<NEO::IgcOclDeviceCtxTag> createIgcDeviceContext() const override;
};

} // namespace NEO
