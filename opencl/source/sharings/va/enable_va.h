/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/source/sharings/va/va_sharing_defines.h"

#include <memory>

namespace NEO {
class Context;
class DriverInfo;

struct VaCreateContextProperties {
    VADisplay vaDisplay = nullptr;
};

class VaSharingContextBuilder : public SharingContextBuilder {
  protected:
    std::unique_ptr<VaCreateContextProperties> contextData;

  public:
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

class VaSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override;
    std::string getExtensions(DriverInfo *driverInfo) override;
    void fillGlobalDispatchTable() override;
    void *getExtensionFunctionAddress(const std::string &functionName) override;
};
} // namespace NEO
