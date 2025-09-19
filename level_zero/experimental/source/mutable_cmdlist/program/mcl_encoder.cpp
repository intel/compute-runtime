/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_encoder.h"

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_imp.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"

#include <limits>
#include <set>

namespace L0::MCL::Program::Encoder {
size_t MclEncoder::addSymbol(const std::string &name, Sections::SectionType section, Symbols::SymbolType type, uint64_t value, size_t size) {
    Symbols::Symbol symbol(name, type, section, value, size);

    const auto symbolId = program.symbols.size();
    symbolNameToSymIdx[name] = symbolId;
    program.symbols.push_back(symbol);
    return symbolId;
}

void MclEncoder::addRelocation(Sections::SectionType section, size_t symbolId, Relocations::RelocType type, uint64_t offset, int64_t addend) {
    Relocations::Relocation relocation(section, symbolId, type, offset, addend);
    program.relocs[section].push_back(relocation);
}

uint64_t MclEncoder::getIohOffset(uint64_t oldOffset) {
    for (auto &iohDp : dispatchedIohs) {
        if (oldOffset >= iohDp.offsetIoh && oldOffset < iohDp.offsetIoh + iohDp.size) {
            return oldOffset - iohDp.offsetIoh + iohDp.newOffset;
        }
    }
    DEBUG_BREAK_IF(true);
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MclEncoder::getSshOffset(uint64_t oldOffset) {
    for (auto &sshDp : dispatchedSshs) {
        if (oldOffset >= sshDp.offsetSsh && oldOffset < sshDp.offsetSsh + sshDp.size) {
            return oldOffset - sshDp.offsetSsh + sshDp.newOffset;
        }
    }
    DEBUG_BREAK_IF(true);
    return std::numeric_limits<uint64_t>::max();
}

std::pair<Sections::SectionType, uint64_t> MclEncoder::getSectionTypeAndOffset(ConstStringRef sectionName) {
    using namespace NEO::Zebin::Elf;
    uint64_t offsetInSection = 0U;
    Sections::SectionType type = Sections::SectionType::shtUndef;

    if (sectionName.startsWith(SectionNames::textPrefix.data())) {
        auto refKernelName = sectionName.substr(SectionNames::textPrefix.length());
        auto symbolName = Symbols::SymbolNames::kernelPrefix.str() + refKernelName.str();
        if (auto result = symbolNameToSymIdx.find(symbolName);
            result != symbolNameToSymIdx.end()) {
            offsetInSection = program.symbols[result->second].value;
            type = Sections::SectionType::shtIh;
        } else {
            DEBUG_BREAK_IF(true);
        }
    } else if (sectionName == SectionNames::dataConst) {
        type = Sections::SectionType::shtDataConst;
    } else if (sectionName == SectionNames::dataGlobal) {
        type = Sections::SectionType::shtDataVar;
    }
    return {type, offsetInSection};
}

std::vector<uint8_t> MclEncoder::encode(const MclEncoderArgs &args) {
    MclEncoder encoder{};
    encoder.parseKernelData(*args.kernelData);
    encoder.parseDispatches(*args.dispatches);
    encoder.parseCS(args.cmdBuffer, *args.sba);
    if (args.explicitIh) {
        encoder.addGsbSymbol();
        encoder.addIhSymbol();
    }
    if (args.explicitSsh) {
        encoder.addSshSymbol();
    }
    encoder.parseVars(*args.vars);
    if (NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::zebin>(args.module)) {
        encoder.parseZebin(args.module);
    }
    return encoder.encodeProgram();
}

MclEncoder::MclEncoder() {
    using namespace Symbols::SymbolNames;
    using Section = Sections::SectionType;

    addSymbol(iohBaseSymbol.data(), Section::shtUndef, Symbols::SymbolType::base, 0U, 8U);
}

void MclEncoder::parseKernelData(const std::vector<std::unique_ptr<KernelData>> &kernelDataVec) {
    for (size_t kernelDataId = 0U; kernelDataId < kernelDataVec.size(); kernelDataId++) {
        auto &kernelData = kernelDataVec[kernelDataId];
        auto &isa = kernelData->kernelIsa;
        auto &kernelName = kernelData->kernelName;

        auto kernelOffset = program.ih.size();
        program.ih.insert(program.ih.end(), isa.begin(), isa.end());
        auto symbolName = Symbols::SymbolNames::kernelPrefix.str() + kernelName;
        addSymbol(symbolName, Sections::SectionType::shtIh, Symbols::SymbolType::heap, kernelOffset, isa.size());

        auto kernelDataSymbolName = Symbols::SymbolNames::kernelDataPrefix.str() + kernelName;
        uint8_t indirectOffset = (kernelData->indirectOffset >> Symbols::KernelSymbol::indirectOffsetBitShift);
        auto kernelDataSymbolValue = Symbols::KernelSymbol(static_cast<uint32_t>(kernelDataId), kernelData->skipPerThreadDataLoad, kernelData->simdSize, kernelData->passInlineData, indirectOffset);
        addSymbol(kernelDataSymbolName, Sections::SectionType::shtUndef, Symbols::SymbolType::kernel, kernelDataSymbolValue.data, 0U);
    }
}

void MclEncoder::addIhSymbol() {
    using namespace Symbols::SymbolNames;
    using Section = Sections::SectionType;

    addSymbol(ihBaseSymbol.data(), Section::shtUndef, Symbols::SymbolType::base, 0U, 8U);
}

void MclEncoder::addGsbSymbol() {
    using namespace Symbols::SymbolNames;
    using Section = Sections::SectionType;

    addSymbol(gpuBaseSymbol.data(), Section::shtUndef, Symbols::SymbolType::base, 0U, 8U);
}

void MclEncoder::addSshSymbol() {
    using namespace Symbols::SymbolNames;
    using Section = Sections::SectionType;

    addSymbol(sshBaseSymbol.data(), Section::shtUndef, Symbols::SymbolType::base, 0U, 8U);
}

void MclEncoder::parseCS(ArrayRef<const uint8_t> cmdBuffer, const std::vector<StateBaseAddressOffsets> &sbaVec) {
    using SectionType = Sections::SectionType;
    using namespace Symbols::SymbolNames;

    program.cs.insert(program.cs.begin(), cmdBuffer.begin(), cmdBuffer.end());
    if (sbaVec.size() > 0) {
        DEBUG_BREAK_IF(sbaVec.size() != 1U);
        auto &sba = sbaVec[0];
        auto gpuBaseSymId = addSymbol(gpuBaseSymbol.data(), SectionType::shtUndef, Symbols::SymbolType::base, 0U, 8U);
        addRelocation(SectionType::shtCs, gpuBaseSymId, Relocations::RelocType::sba, sba.gsba);

        auto ihBaseSymId = addSymbol(ihBaseSymbol.data(), SectionType::shtUndef, Symbols::SymbolType::base, 0U, 8U);
        addRelocation(SectionType::shtCs, ihBaseSymId, Relocations::RelocType::sba, sba.isba);

        if (sba.ssba != undefined<CommandBufferOffset>) {
            auto sshSymId = addSymbol(sshBaseSymbol.data(), SectionType::shtUndef, Symbols::SymbolType::base, 0U, 8U);
            addRelocation(SectionType::shtCs, sshSymId, Relocations::RelocType::sba, sba.ssba);
        }
    }
}

void MclEncoder::parseDispatches(const std::vector<std::unique_ptr<KernelDispatch>> &dispatches) {
    for (uint32_t dispatchId = 0U; dispatchId < dispatches.size(); dispatchId++) {
        auto &dispatch = *(dispatches[dispatchId].get());

        using namespace Symbols::SymbolNames;
        using RelocType = Relocations::RelocType;
        using SecType = Sections::SectionType;

        Symbols::DispatchSymbolValue value{};
        value.dispatchId = dispatchId;

        auto kernelDataSymbolName = Symbols::SymbolNames::kernelDataPrefix.str() + dispatch.kernelData->kernelName;
        auto &kernelDataSymbol = program.symbols[symbolNameToSymIdx[kernelDataSymbolName]];
        value.kernelDataId = Symbols::KernelSymbol(kernelDataSymbol.value).kernelDataId;

        const std::string dispatchSymbolName = dispatchPrefix.str() + std::to_string(dispatchId);
        auto dispatchSymId = addSymbol(dispatchSymbolName, SecType::shtUndef, Symbols::SymbolType::dispatch, value.data, 0U);

        constexpr auto alignment = 64U;
        const auto unaligned = program.ioh.size() & (alignment - 1);
        if (unaligned) {
            auto padding = alignment - unaligned;
            program.ioh.resize(program.ioh.size() + padding, 0U);
        }
        const size_t iohOffset = program.ioh.size();
        program.ioh.insert(program.ioh.end(), dispatch.indirectObjectHeap.begin(), dispatch.indirectObjectHeap.end());

        const std::string iohSymName = Symbols::SymbolNames::iohPrefix.data() + std::to_string(dispatchId);
        addSymbol(iohSymName, SecType::shtIoh, Symbols::SymbolType::heap, iohOffset, dispatch.indirectObjectHeap.size());

        const size_t crossThreadOffset = iohOffset;
        addRelocation(Sections::SectionType::shtIoh, dispatchSymId, Relocations::RelocType::crossThreadDataStart, crossThreadOffset);

        size_t perThreadOffset = undefined<IndirectObjectHeapOffset>;
        if (dispatch.offsets.perThreadOffset != undefined<IndirectObjectHeapOffset>) {
            perThreadOffset = iohOffset + (dispatch.offsets.perThreadOffset - dispatch.offsets.crossThreadOffset);
        }
        addRelocation(Sections::SectionType::shtIoh, dispatchSymId, Relocations::RelocType::perThreadDataStart, perThreadOffset);

        DispatchedIoh dspIoh{};
        dspIoh.newOffset = iohOffset;
        dspIoh.offsetIoh = dispatch.offsets.crossThreadOffset;
        dspIoh.size = dispatch.indirectObjectHeap.size();
        dispatchedIohs.push_back(dspIoh);

        if (dispatch.surfaceStateHeapSize != 0U) {
            const size_t sshSize = dispatch.surfaceStateHeapSize;
            const size_t sshOffset = program.ssh.size();
            program.ssh.resize(program.ssh.size() + sshSize, 0U);

            const std::string sshSymName = Symbols::SymbolNames::sshPrefix.data() + std::to_string(dispatchId);
            addSymbol(sshSymName, Sections::SectionType::shtSsh, Symbols::SymbolType::heap, sshOffset, sshSize);

            addRelocation(Sections::SectionType::shtSsh, dispatchSymId, Relocations::RelocType::surfaceStateHeapStart, sshOffset);
            const size_t bindingTableOffset = sshOffset + dispatch.offsets.btOffset;
            addRelocation(Sections::SectionType::shtSsh, dispatchSymId, RelocType::bindingTable, bindingTableOffset);

            DispatchedSsh dspSsh{};
            dspSsh.newOffset = sshOffset;
            dspSsh.offsetSsh = dispatch.offsets.sshOffset;
            dspSsh.size = sshSize;
            dispatchedSshs.push_back(dspSsh);

            size_t numEntries = static_cast<size_t>(dispatch.offsets.btOffset >> 6); // bindingTableOffset / 64 -> numEntries
            ArrayRef<int> bindingTable = {reinterpret_cast<int *>(ptrOffset(program.ssh.data(), sshOffset + dispatch.offsets.btOffset)), numEntries};
            for (size_t i = 0; i < bindingTable.size(); i++) {
                bindingTable[i] = static_cast<int>(sshOffset + i * 64);
            }
        }

        auto kernelName = Symbols::SymbolNames::kernelPrefix.str() + dispatch.kernelData->kernelName;
        auto kernelSymId = symbolNameToSymIdx[kernelName];
        addRelocation(Sections::SectionType::shtIh, dispatchSymId, RelocType::kernelStart, program.symbols[kernelSymId].value);

        addRelocation(SecType::shtCs, dispatchSymId, RelocType::walkerCommand, dispatch.offsets.walkerCmdOffset);
        if (dispatch.varDispatch) {
            const auto crossThreadOffset = dispatch.offsets.crossThreadOffset;
            auto addCrossThreadDataReloc = [this, &dispatchSymId, crossThreadOffset](auto relocType, auto offset) {
                if (isDefined(offset)) {
                    addRelocation(SecType::shtIoh, dispatchSymId, relocType, crossThreadOffset + offset);
                }
            };

            auto &payloadOffsets = dispatch.varDispatch->getIndirectDataOffsets();
            addCrossThreadDataReloc(RelocType::numWorkGroups, payloadOffsets.numWorkGroups);
            addCrossThreadDataReloc(RelocType::localWorkSize, payloadOffsets.localWorkSize);
            addCrossThreadDataReloc(RelocType::localWorkSize2, payloadOffsets.localWorkSize2);
            addCrossThreadDataReloc(RelocType::enqLocalWorkSize, payloadOffsets.enqLocalWorkSize);
            addCrossThreadDataReloc(RelocType::globalWorkSize, payloadOffsets.globalWorkSize);
            addCrossThreadDataReloc(RelocType::workDimensions, payloadOffsets.workDimensions);

            varDispatchToSymIdx[dispatch.varDispatch.get()] = dispatchSymId;
        }
    }
}

void MclEncoder::parseVars(const std::vector<std::unique_ptr<Variable>> &vars) {
    using RelType = Relocations::RelocType;
    using SecType = Sections::SectionType;

    size_t unnamedTmpVarCtr = 0U;
    size_t unnamedVarCtr = 0U;
    for (size_t varId = 0U; varId < vars.size(); varId++) {
        auto &var = vars[varId];
        auto varType = var->getType();
        DEBUG_BREAK_IF(varType == VariableType::none);

        const auto &varName = var->getDesc().name;
        std::string symbolName = "";
        Symbols::VariableSymbolValue symValue{};
        symValue.variableType = varType;
        symValue.id = static_cast<uint32_t>(varId);
        if (var->getDesc().isTemporary) {
            symbolName = Symbols::SymbolNames::varPrefix.str() + Symbols::SymbolNames::tmpPrefix.str();
            symbolName += varName.empty() ? ("no_name." + std::to_string(unnamedTmpVarCtr++)) : varName;
            symValue.flags.isTmp = true;
            symValue.flags.isScalable = var->getDesc().isScalable;
        } else {
            symbolName = Symbols::SymbolNames::varPrefix.str();
            symbolName += varName.empty() ? ("no_name." + std::to_string(unnamedVarCtr++)) : varName;
        }

        auto symSize = var->getDesc().size;
        DEBUG_BREAK_IF(symSize == 0U);
        auto symbolId = addSymbol(symbolName, SecType::shtUndef, Symbols::SymbolType::var, symValue.data, symSize);

        if (varType == VariableType::buffer) {
            auto &bufferUsages = var->getBufferUsages();

            for (auto &statelessOffset : bufferUsages.statelessIndirect) {
                addRelocation(SecType::shtIoh, symbolId, RelType::bufferStateless, getIohOffset(statelessOffset));
            }

            for (auto &bufferOffset : bufferUsages.bufferOffset) {
                addRelocation(SecType::shtIoh, symbolId, RelType::bufferOffset, getIohOffset(bufferOffset));
            }

            for (auto &bindfulOffset : bufferUsages.bindful) {
                addRelocation(SecType::shtSsh, symbolId, RelType::bufferBindful, getSshOffset(bindfulOffset));
            }
            for (const auto &bindfulOffset : bufferUsages.bindfulWithoutOffset) {
                addRelocation(SecType::shtSsh, symbolId, RelType::bufferBindfulWithoutOffset, getSshOffset(bindfulOffset));
            }

            for (auto &bindlessOffset : bufferUsages.bindless) {
                addRelocation(SecType::shtIoh, symbolId, RelType::bufferBindless, bindlessOffset);
            }
            for (auto &bindlessOffset : bufferUsages.bindlessWithoutOffset) {
                addRelocation(SecType::shtIoh, symbolId, RelType::bufferBindlessWithoutOffset, bindlessOffset);
            }

            for (auto &csOffset : bufferUsages.commandBufferOffsets) {
                addRelocation(SecType::shtCs, symbolId, RelType::bufferAddress, csOffset);
            }
        } else if (varType == VariableType::value) {
            auto &valueUsages = var->getValueUsages();

            for (const auto &statelessOffset : valueUsages.statelessIndirect) {
                addRelocation(SecType::shtIoh, symbolId, RelType::valueStateless, getIohOffset(statelessOffset));
            }
            for (const auto &statelessSize : valueUsages.statelessIndirectPatchSize) {
                addRelocation(SecType::shtIoh, symbolId, RelType::valueStatelessSize, statelessSize);
            }

            for (auto &csOffset : valueUsages.commandBufferOffsets) {
                addRelocation(SecType::shtCs, symbolId, RelType::valueCmdBuffer, csOffset);
            }
            for (auto &csSize : valueUsages.commandBufferPatchSize) {
                addRelocation(SecType::shtCs, symbolId, RelType::valueCmdBufferSize, csSize);
            }
        } else if (varType == VariableType::groupCount) {
            UNRECOVERABLE_IF(varId >= std::numeric_limits<uint16_t>::max());
            for (auto &dispatch : var->getDispatches()) {
                auto dispatchSymbolValue = reinterpret_cast<Symbols::DispatchSymbolValue *>(&program.symbols[varDispatchToSymIdx[dispatch]].value);
                dispatchSymbolValue->groupCountVarId = static_cast<uint16_t>(varId);
            }
        } else if (varType == VariableType::groupSize) {
            UNRECOVERABLE_IF(varId >= std::numeric_limits<uint16_t>::max());
            for (auto &dispatch : var->getDispatches()) {
                auto dispatchSymbolValue = reinterpret_cast<Symbols::DispatchSymbolValue *>(&program.symbols[varDispatchToSymIdx[dispatch]].value);
                dispatchSymbolValue->groupSizeVarId = static_cast<uint16_t>(varId);
            }
        }
    }
}

void MclEncoder::parseZebin(ArrayRef<const uint8_t> zebin) {
    using namespace NEO::Zebin::Elf;
    using ElfRel = ElfRel<EI_CLASS_64>;
    using ElfSymbol = ElfSymbolEntry<EI_CLASS_64>;

    std::string warnings, errors;
    auto elf = decodeElf(zebin, warnings, errors);
    if (false == errors.empty()) {
        DEBUG_BREAK_IF(true);
        return;
    }

    std::unordered_map<std::string, ArrayRef<const ElfRel>> sectionsElfRelocs;
    ArrayRef<const ElfSymbol> elfSymbols;
    ArrayRef<const uint8_t> dataConst;
    ArrayRef<const uint8_t> dataVar;
    ArrayRef<const uint8_t> dataString;

    ArrayRef<const uint8_t> refExternalFunctions;

    for (size_t i = 0; i < elf.sectionHeaders.size(); i++) {
        auto &section = elf.sectionHeaders[i];
        auto &hdr = section.header;
        auto &data = section.data;

        auto secName = elf.getSectionName(static_cast<uint32_t>(i));
        auto secNameRef = ConstStringRef(secName);
        if (hdr->type == SHT_REL) {
            auto targetSecName = secNameRef.substr(SectionNames::relTablePrefix.length() - 1); // leave the '.' at the end
            sectionsElfRelocs[targetSecName.str()] = {reinterpret_cast<const ElfRel *>(data.begin()), hdr->size / hdr->entsize};
        } else if (hdr->type == SHT_SYMTAB) {
            elfSymbols = {reinterpret_cast<const ElfSymbol *>(data.begin()), hdr->size / hdr->entsize};
        } else if (hdr->type == SHT_PROGBITS) {
            if (secNameRef == SectionNames::dataConst) {
                dataConst = data;
            } else if (secNameRef == SectionNames::dataGlobal) {
                dataVar = data;
            } else if (secNameRef == SectionNames::dataConstString) {
                dataString = data;
            } else if ((secNameRef == SectionNames::functions) || (secNameRef == SectionNames::text)) {
                refExternalFunctions = data;
            }
        }
    }

    std::copy(dataConst.begin(), dataConst.end(), program.consts.begin());
    std::copy(dataVar.begin(), dataVar.end(), program.vars.begin());
    std::copy(dataString.begin(), dataString.end(), program.strings.begin());

    // check used zebin symbols in relocs
    std::set<uint64_t> usedSymbolsId;
    for (auto &sectionElfRelocs : sectionsElfRelocs) {
        const auto elfRelocations = sectionElfRelocs.second;
        for (auto &elfReloc : elfRelocations) {
            usedSymbolsId.insert(elfReloc.getSymbolTableIndex());
        };
    }
    // Parse zebin symbols
    std::set<size_t> undefSymbols;
    std::unordered_map<size_t, size_t> symIdMapping;
    for (auto &usedSymbolId : usedSymbolsId) {
        const auto &elfSymbol = elfSymbols[usedSymbolId];

        auto symbolSectionName = elf.getSectionName(elfSymbol.shndx);
        Sections::SectionType symbolSection = Sections::SectionType::shtUndef;
        Symbols::SymbolType symbolType;
        uint64_t symbolValue = elfSymbol.value;
        uint64_t symbolSize = elfSymbol.size;
        auto symbolName = elf.getSymbolName(elfSymbol.name);

        if (symbolSectionName == SectionNames::functions) {
            // add function to IH
            auto offsetInFunctions = elfSymbol.value;
            auto funcSize = elfSymbol.size;
            ArrayRef<const uint8_t> refFunc = {ptrOffset(refExternalFunctions.begin(), offsetInFunctions), funcSize};

            symbolValue = program.ih.size();
            symbolSection = Sections::SectionType::shtIh;
            symbolType = Symbols::SymbolType::function;
            program.ih.insert(program.ih.end(), refFunc.begin(), refFunc.end());
            symbolName = Symbols::SymbolNames::functionPrefix.str() + symbolName;
        } else {
            auto [symSec, offset] = getSectionTypeAndOffset(symbolSectionName);
            symbolSection = symSec;
            symbolType = static_cast<Symbols::SymbolType>(elfSymbol.getType());
            symbolValue += offset;
        }

        if (symbolSection == Sections::SectionType::shtUndef) { // skip this symbol
            undefSymbols.insert(usedSymbolId);
            continue;
        }

        // remove collisions - naive approach
        auto newSymbolId = addSymbol(symbolName, symbolSection, symbolType, symbolValue, symbolSize);
        symIdMapping[usedSymbolId] = newSymbolId;
    }

    // Parse zebin relocs
    for (auto &[targetSectionName, elfRelocations] : sectionsElfRelocs) {
        auto [section, offsetInSection] = getSectionTypeAndOffset(targetSectionName);
        DEBUG_BREAK_IF(section == Sections::SectionType::shtUndef);

        for (auto &elfReloc : elfRelocations) {
            uint64_t offset = elfReloc.offset + offsetInSection;
            auto type = static_cast<Relocations::RelocType>(elfReloc.getRelocationType());
            if (0 == undefSymbols.count(elfReloc.getSymbolTableIndex())) {
                addRelocation(section, symIdMapping[elfReloc.getSymbolTableIndex()], type, offset);
            }
        }
    }

    usesZebin = true;
}

std::vector<uint8_t> MclEncoder::encodeProgram() {
    using namespace NEO::Elf;
    using namespace Sections::SectionNames;
    using SectionType = Sections::SectionType;
    using ElfSymbol = NEO::Elf::ElfSymbolEntry<EI_CLASS_64>;
    using ElfRela = NEO::Elf::ElfRela<EI_CLASS_64>;
    ElfEncoder<EI_CLASS_64> elfEncoder;

    auto secIdx = 1U;
    elfEncoder.appendSection(SectionType::shtCs, csSectionName, program.cs);
    sectionIndecies.cs = secIdx++;

    elfEncoder.appendSection(SectionType::shtIh, ihSectionName, program.ih);
    sectionIndecies.ih = secIdx++;

    elfEncoder.appendSection(SectionType::shtIoh, iohSectionName, program.ioh);
    sectionIndecies.ioh = secIdx++;

    if (program.ssh.empty() == false) {
        elfEncoder.appendSection(SectionType::shtSsh, sshSectionName, program.ssh);
        sectionIndecies.ssh = secIdx++;
    }
    if (usesZebin) {
        if (program.consts.empty() == false) {
            elfEncoder.appendSection(SectionType::shtDataConst, dataConstSectionName, program.consts);
            sectionIndecies.consts = secIdx++;
        }

        if (program.vars.empty() == false) {
            elfEncoder.appendSection(SectionType::shtDataVar, dataVarSectionName, program.vars);
            sectionIndecies.vars = secIdx++;
        }

        if (program.strings.empty() == false) {
            elfEncoder.appendSection(SectionType::shtDataStrings, dataStringSectionName, program.strings);
        }
    }

    StringSectionBuilder ss;

    std::vector<ElfSymbol> localSymbols;
    localSymbols.resize(1);
    std::vector<ElfSymbol> globalSymbols;
    for (const auto &symbol : program.symbols) {
        ElfSymbol symbolElf;
        symbolElf.name = ss.appendString(symbol.name);
        symbolElf.shndx = static_cast<uint16_t>(symbol.section);
        symbolElf.value = symbol.value;
        symbolElf.size = symbol.size;
        symbolElf.setType(static_cast<uint8_t>(symbol.type));
        symbolElf.setBinding(STB_LOCAL);

        localSymbols.push_back(symbolElf);
    }

    auto &symTab = elfEncoder.appendSection(SHT_SYMTAB, Sections::SectionNames::symtabName,
                                            {reinterpret_cast<const uint8_t *>(localSymbols.data()), localSymbols.size() * sizeof(ElfSymbol)});

    auto &strTab = elfEncoder.appendSection(SHT_STRTAB, Sections::SectionNames::strtabName, ss.data());
    symTab.link = elfEncoder.getSectionHeaderIndex(strTab);
    symTab.info = static_cast<uint32_t>(localSymbols.size());

    // create rela section
    for (auto &[sectionType, relocs] : program.relocs) {
        std::vector<ElfRela> elfRelocations;

        for (auto &reloc : relocs) {
            ElfRela elfRelocation;
            elfRelocation.offset = reloc.offset;
            elfRelocation.addend = reloc.addend;
            elfRelocation.setSymbolTableIndex(reloc.symbolId + 1U);
            elfRelocation.setRelocationType(reloc.type);
            elfRelocations.push_back(elfRelocation);
        }

        std::string relaSectionName = Sections::SectionNames::relaPrefix.str();
        uint8_t targetSecIdx = 0U;
        switch (sectionType) {
        default:
            DEBUG_BREAK_IF(true);
            break;
        case Sections::SectionType::shtSsh:
            relaSectionName += Sections::SectionNames::sshSectionName.str();
            targetSecIdx = sectionIndecies.ssh;
            break;
        case Sections::SectionType::shtIh:
            relaSectionName += Sections::SectionNames::ihSectionName.str();
            targetSecIdx = sectionIndecies.ih;
            break;
        case Sections::SectionType::shtCs:
            relaSectionName += Sections::SectionNames::csSectionName.str();
            targetSecIdx = sectionIndecies.cs;
            break;
        case Sections::SectionType::shtIoh:
            relaSectionName += Sections::SectionNames::iohSectionName.str();
            targetSecIdx = sectionIndecies.ioh;
            break;
        case Sections::SectionType::shtDataConst:
            relaSectionName += Sections::SectionNames::dataConstSectionName.str();
            targetSecIdx = sectionIndecies.consts;
            break;
        case Sections::SectionType::shtDataVar:
            relaSectionName += Sections::SectionNames::dataVarSectionName.str();
            targetSecIdx = sectionIndecies.vars;
            break;
        }

        auto &rela = elfEncoder.appendSection(SHT_RELA, relaSectionName, {reinterpret_cast<const uint8_t *>(elfRelocations.data()), elfRelocations.size() * sizeof(ElfRela)});
        rela.info = (uint32_t)targetSecIdx;
        rela.link = elfEncoder.getSectionHeaderIndex(symTab);
    }

    return elfEncoder.encode();
}

} // namespace L0::MCL::Program::Encoder
