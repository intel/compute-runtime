/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#define IGA_BUILDING_DLL
#include "igad.h"

#undef IGA_BUILDING_DLL

#include <cstdint>
#include <optional>
#include <string>

namespace NEO {

class IgaDllMock {
  public:
    IgaDllMock();
    ~IgaDllMock();

    IgaDllMock(const IgaDllMock &other) = delete;
    IgaDllMock(IgaDllMock &&other) = delete;
    IgaDllMock &operator=(const IgaDllMock &other) = delete;
    IgaDllMock &operator=(IgaDllMock &&other) = delete;

    static IgaDllMock &getUsedInstance();

    iga_status_t assemble(
        iga_context_t ctx,
        const iga_assemble_options_t *opts,
        const char *kernelText,
        void **output,
        uint32_t *outputSize);

    iga_status_t contextCreate(
        const iga_context_options_t *opts,
        iga_context_t *ctx);

    iga_status_t contextGetErrors(
        iga_context_t ctx,
        const iga_diagnostic_t **ds,
        uint32_t *dsLen);

    iga_status_t contextGetWarnings(
        iga_context_t ctx,
        const iga_diagnostic_t **ds,
        uint32_t *dsLen);

    iga_status_t contextRelease(iga_context_t ctx);

    iga_status_t disassemble(
        iga_context_t ctx,
        const iga_disassemble_options_t *opts,
        const void *input,
        uint32_t inputSize,
        const char *(*fmtLabelName)(int32_t, void *),
        void *fmtLabelCtx,
        char **kernelText);

    const char *statusToString(const iga_status_t st);

    int assembleCalledCount{0};
    int assembleReturnValue{0};
    std::optional<std::string> assembleOutput{};

    int contextCreateCalledCount{0};
    int contextCreateReturnValue{0};

    int contextGetErrorsCalledCount{0};
    int contextGetErrorsReturnValue{0};
    iga_diagnostic_t contextGetErrorsOutput{};
    std::optional<std::string> contextGetErrorsOutputString{};
    bool shouldContextGetErrorsReturnZeroSizeAndNonNullptr{false};
    bool shouldContextGetErrorsReturnNullptrAndNonZeroSize{false};

    int contextGetWarningsCalledCount{0};
    int contextGetWarningsReturnValue{0};
    iga_diagnostic_t contextGetWarningsOutput{};
    std::optional<std::string> contextGetWarningsOutputString{};
    bool shouldContextGetWarningsReturnZeroSizeAndNonNullptr{false};
    bool shouldContextGetWarningsReturnNullptrAndNonZeroSize{false};

    int contextReleaseCalledCount{0};
    int contextReleaseReturnValue{0};

    int disassembleCalledCount{0};
    int disassembleReturnValue{0};
    std::optional<std::string> disassembleOutputKernelText{};

    int statusToStringCalledCount{0};
    std::string statusToStringReturnValue{};

  private:
    inline static IgaDllMock *usedInstance{nullptr};
};

} // namespace NEO