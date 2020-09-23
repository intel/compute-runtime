/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/unit_test/mocks/mock_compiler_interface.h"

#include "cif/common/cif_main.h"

namespace NEO {
class MockCompilerInterfaceSpirv : public MockCompilerInterface {
    TranslationOutput::ErrorCode compile(const NEO::Device &device, const TranslationInput &input, TranslationOutput &output) override;
    TranslationOutput::ErrorCode build(const NEO::Device &device, const TranslationInput &input, TranslationOutput &out) override;
};
} // namespace NEO