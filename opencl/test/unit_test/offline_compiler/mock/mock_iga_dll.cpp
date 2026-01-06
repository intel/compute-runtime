/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_iga_dll.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

#define REQUIRE_NOT_NULL(pointer) throwIfNullptr(pointer, #pointer, __LINE__, __FILE__)

extern "C" IGA_API iga_status_t iga_assemble(
    iga_context_t ctx,
    const iga_assemble_options_t *opts,
    const char *kernelText,
    void **output,
    uint32_t *outputSize) {
    return NEO::IgaDllMock::getUsedInstance().assemble(ctx, opts, kernelText, output, outputSize);
}

extern "C" IGA_API iga_status_t iga_context_create(
    const iga_context_options_t *opts,
    iga_context_t *ctx) {
    return NEO::IgaDllMock::getUsedInstance().contextCreate(opts, ctx);
}

extern "C" IGA_API iga_status_t iga_context_get_errors(
    iga_context_t ctx,
    const iga_diagnostic_t **ds,
    uint32_t *dsLen) {
    return NEO::IgaDllMock::getUsedInstance().contextGetErrors(ctx, ds, dsLen);
}

extern "C" IGA_API iga_status_t iga_context_get_warnings(
    iga_context_t ctx,
    const iga_diagnostic_t **ds,
    uint32_t *dsLen) {
    return NEO::IgaDllMock::getUsedInstance().contextGetWarnings(ctx, ds, dsLen);
}

extern "C" IGA_API iga_status_t iga_context_release(iga_context_t ctx) {
    return NEO::IgaDllMock::getUsedInstance().contextRelease(ctx);
}

extern "C" IGA_API iga_status_t iga_disassemble(
    iga_context_t ctx,
    const iga_disassemble_options_t *opts,
    const void *input,
    uint32_t inputSize,
    const char *(*fmtLabelName)(int32_t, void *),
    void *fmtLabelCtx,
    char **kernelText) {
    return NEO::IgaDllMock::getUsedInstance().disassemble(ctx, opts, input, inputSize, fmtLabelName, fmtLabelCtx, kernelText);
}

extern "C" IGA_API const char *iga_status_to_string(const iga_status_t st) {
    return NEO::IgaDllMock::getUsedInstance().statusToString(st);
}

namespace NEO {

template <typename T>
void throwIfNullptr(const T *ptr, const char *variableName, const int lineNumber, const char *file) {
    if (!ptr) {
        std::stringstream errorMessage{};
        errorMessage << "IgaDllMock expected a valid pointer, but received NULL!\n"
                     << "Variable: " << variableName << '\n'
                     << "File: " << file << '\n'
                     << "Line:" << lineNumber << '\n';

        throw std::invalid_argument{errorMessage.str()};
    }
}

IgaDllMock::IgaDllMock() {
    if (usedInstance) {
        std::cerr << "Logic error! Used instance of IgaDllMock already registered! Aborting..." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    usedInstance = this;
}

IgaDllMock::~IgaDllMock() {
    usedInstance = nullptr;
}

IgaDllMock &IgaDllMock::getUsedInstance() {
    if (!usedInstance) {
        std::cerr << "Logic error! Used instance of IgaDllMock has not been registered! Aborting..." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return *usedInstance;
}

iga_status_t IgaDllMock::assemble(
    iga_context_t ctx,
    const iga_assemble_options_t *opts,
    const char *kernelText,
    void **output,
    uint32_t *outputSize) {
    ++assembleCalledCount;

    if (assembleOutput.has_value()) {
        REQUIRE_NOT_NULL(output);
        REQUIRE_NOT_NULL(outputSize);

        *output = assembleOutput->data();
        *outputSize = static_cast<uint32_t>(assembleOutput->size() + 1u);
    }

    return static_cast<iga_status_t>(assembleReturnValue);
}

iga_status_t IgaDllMock::contextCreate(
    const iga_context_options_t *opts,
    iga_context_t *ctx) {
    ++contextCreateCalledCount;
    return static_cast<iga_status_t>(contextCreateReturnValue);
}

iga_status_t IgaDllMock::contextGetErrors(
    iga_context_t ctx,
    const iga_diagnostic_t **ds,
    uint32_t *dsLen) {
    ++contextGetErrorsCalledCount;

    REQUIRE_NOT_NULL(ds);
    REQUIRE_NOT_NULL(dsLen);

    if (shouldContextGetErrorsReturnZeroSizeAndNonNullptr) {
        *ds = &contextGetErrorsOutput;
        *dsLen = 0;
    } else if (shouldContextGetErrorsReturnNullptrAndNonZeroSize) {
        *ds = nullptr;
        *dsLen = 1;
    } else if (contextGetErrorsOutputString.has_value()) {
        contextGetErrorsOutput.message = contextGetErrorsOutputString->c_str();
        *ds = &contextGetErrorsOutput;
        *dsLen = static_cast<uint32_t>(contextGetErrorsOutputString->size() + 1);
    } else {
        *ds = nullptr;
        *dsLen = 0;
    }

    return static_cast<iga_status_t>(contextGetErrorsReturnValue);
}

iga_status_t IgaDllMock::contextGetWarnings(
    iga_context_t ctx,
    const iga_diagnostic_t **ds,
    uint32_t *dsLen) {
    ++contextGetWarningsCalledCount;

    REQUIRE_NOT_NULL(ds);
    REQUIRE_NOT_NULL(dsLen);

    if (shouldContextGetWarningsReturnZeroSizeAndNonNullptr) {
        *ds = &contextGetWarningsOutput;
        *dsLen = 0;
    } else if (shouldContextGetWarningsReturnNullptrAndNonZeroSize) {
        *ds = nullptr;
        *dsLen = 1;
    } else if (contextGetWarningsOutputString.has_value()) {
        contextGetWarningsOutput.message = contextGetWarningsOutputString->c_str();
        *ds = &contextGetWarningsOutput;
        *dsLen = static_cast<uint32_t>(contextGetWarningsOutputString->size() + 1);
    } else {
        *ds = nullptr;
        *dsLen = 0;
    }

    return static_cast<iga_status_t>(contextGetWarningsReturnValue);
}

iga_status_t IgaDllMock::contextRelease(iga_context_t ctx) {
    ++contextReleaseCalledCount;
    return static_cast<iga_status_t>(contextReleaseReturnValue);
}

iga_status_t IgaDllMock::disassemble(
    iga_context_t ctx,
    const iga_disassemble_options_t *opts,
    const void *input,
    uint32_t inputSize,
    const char *(*fmtLabelName)(int32_t, void *),
    void *fmtLabelCtx,
    char **kernelText) {
    ++disassembleCalledCount;

    if (disassembleOutputKernelText.has_value()) {
        REQUIRE_NOT_NULL(kernelText);
        *kernelText = disassembleOutputKernelText->data();
    }

    return static_cast<iga_status_t>(disassembleReturnValue);
}

const char *IgaDllMock::statusToString(const iga_status_t st) {
    ++statusToStringCalledCount;
    return statusToStringReturnValue.c_str();
}

} // namespace NEO