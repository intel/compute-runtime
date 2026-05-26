/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/source/sharings/sharing_factory.h"
#include "level_zero/api/opencl/source/sharings/va/va_sharing_defines.h"

#include <memory>

namespace NEO {
namespace LEO {

class Context;

struct VaCreateContextProperties {
    VADisplay vaDisplay = nullptr;
};

class VaSharingContextBuilder : public SharingContextBuilder {
  protected:
    std::unique_ptr<VaCreateContextProperties> contextData;

  public:
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

class VaSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override;
    std::string getExtensions(DriverInfo *driverInfo) override;
    void fillGlobalDispatchTable() override;
    void *getExtensionFunctionAddress(const std::string &functionName) override;
    virtual void *getExtensionFunctionAddressExtra(const std::string &functionName);
};

} // namespace LEO
} // namespace NEO
