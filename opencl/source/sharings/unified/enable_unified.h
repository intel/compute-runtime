/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/sharings/sharing_factory.h"

#include <memory>

namespace NEO {
class Context;

struct UnifiedCreateContextProperties {
};

class UnifiedSharingContextBuilder : public SharingContextBuilder {
  protected:
    std::unique_ptr<UnifiedCreateContextProperties> contextData;

  public:
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

class UnifiedSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override;
    std::string getExtensions(DriverInfo *driverInfo) override;
    void *getExtensionFunctionAddress(const std::string &functionName) override;
};
} // namespace NEO
