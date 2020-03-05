/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/binary_decoder.h"
#include "shared/offline_compiler/source/decoder/binary_encoder.h"
#include "shared/offline_compiler/source/multi_command.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/offline_compiler/source/offline_compiler.h"

using namespace NEO;

#ifdef _WIN32
#define SIGNATURE __declspec(dllexport) int __cdecl
#else
#define SIGNATURE int
#endif

extern "C" {
SIGNATURE oclocInvoke(unsigned int numArgs, const char *argv[],
                      const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                      const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                      uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs);
SIGNATURE oclocFreeOutput(uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs);
}
