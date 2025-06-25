/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/arrayref.h"

#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_program.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace L0::MCL {
struct Variable;
struct VariableDispatch;
struct MutableCommandList;
struct KernelData;
struct Usage;
struct KernelDispatch;
struct StateBaseAddressOffsets;
} // namespace L0::MCL

namespace L0::MCL::Program::Encoder {
using ConstStringRef = NEO::ConstStringRef;

class MclEncoder {
  public:
    struct MclEncoderArgs {
        ArrayRef<const uint8_t> cmdBuffer;
        ArrayRef<const uint8_t> module;
        const std::vector<std::unique_ptr<KernelData>> *kernelData;
        const std::vector<StateBaseAddressOffsets> *sba;
        const std::vector<std::unique_ptr<KernelDispatch>> *dispatchs;
        const std::vector<std::unique_ptr<Variable>> *vars;
        bool explicitIh = false;
        bool explicitSsh = false;
    };
    static std::vector<uint8_t> encode(const MclEncoderArgs &args);

    MclEncoder();
    void parseKernelData(const std::vector<std::unique_ptr<KernelData>> &kernelData);
    void parseCS(ArrayRef<const uint8_t> cmdBuffer, const std::vector<StateBaseAddressOffsets> &sba);
    void parseDispatchs(const std::vector<std::unique_ptr<KernelDispatch>> &dispatchs);
    void parseVars(const std::vector<std::unique_ptr<Variable>> &vars);
    void parseZebin(ArrayRef<const uint8_t> zebin);
    std::vector<uint8_t> encodeProgram();

  protected:
    std::pair<Sections::SectionType, uint64_t> getSectionTypeAndOffset(ConstStringRef sectionName);

    size_t addSymbol(const std::string &name, Sections::SectionType section, Symbols::SymbolType type, uint64_t value, size_t size);
    void addRelocation(Sections::SectionType section, size_t symbolId, Relocations::RelocType type, uint64_t offset, int64_t addend = 0U);

    void addIhSymbol();
    void addGsbSymbol();
    void addSshSymbol();

    uint64_t getSshOffset(uint64_t oldOffset);
    uint64_t getIohOffset(uint64_t oldOffset);

    struct DispatchedSsh {
        uint64_t offsetSsh;
        size_t size;
        uint64_t newOffset;
    };
    std::vector<DispatchedSsh> dispatchedSshs;

    struct DispatchedIoh {
        uint64_t offsetIoh;
        size_t size;
        uint64_t newOffset;
    };
    std::vector<DispatchedIoh> dispatchedIohs;

    struct {
        uint8_t cs = 0U;
        uint8_t ih = 0U;
        uint8_t ioh = 0U;
        uint8_t ssh = 0U;
        uint8_t consts = 0U;
        uint8_t vars = 0U;
    } sectionIndecies;

    std::unordered_map<std::string, uint64_t> symbolNameToSymIdx;
    std::unordered_map<VariableDispatch *, uint64_t> varDispatchToSymIdx;
    MclProgram program;
    size_t iohCounter = 0U;
    size_t sshCounter = 0U;
    bool usesZebin = false;
};
} // namespace L0::MCL::Program::Encoder
