/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>
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
