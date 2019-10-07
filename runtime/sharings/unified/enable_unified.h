/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/sharings/sharing_factory.h"

#include <memory>

namespace NEO {
class Context;

struct UnifiedCreateContextProperties {
};

class UnifiedSharingContextBuilder : public SharingContextBuilder {
  protected:
    std::unique_ptr<UnifiedCreateContextProperties> contextData;

  public:
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

class UnifiedSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override;
    std::string getExtensions() override;
    void *getExtensionFunctionAddress(const std::string &functionName) override;
};
} // namespace NEO
