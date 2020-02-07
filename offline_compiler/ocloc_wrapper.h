/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cctype>
#include <memory>
#include <string>

struct OclocWrapper {
  public:
    OclocWrapper();
    ~OclocWrapper();

    OclocWrapper(OclocWrapper &) = delete;
    OclocWrapper(const OclocWrapper &&) = delete;
    OclocWrapper &operator=(const OclocWrapper &) = delete;
    OclocWrapper &operator=(OclocWrapper &&) = delete;

    int invokeOcloc(unsigned int numArgs, const char *argv[]);
    int invokeOcloc(unsigned int numArgs, const char *argv[],
                    const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                    const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                    uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs);
    int freeOutput(uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs);

  protected:
    bool tryLoadOcloc();
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};
