/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_decoder.h"

#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_imp.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"

namespace L0::MCL::Program::Decoder {

void MclDecoder::decode(const MclDecoderArgs &args) {
    MclDecoder decoder;
    decoder.decodeElf(args.binary);
    decoder.createRequiredAllocations(args.device->getNEODevice(), *args.cmdContainer, args.allocs->ihAlloc,
                                      args.allocs->constsAlloc, args.allocs->varsAlloc, args.allocs->stringsAlloc,
                                      args.heapless);
    decoder.parseSymbols();
    decoder.parseRelocations();

    for (auto &varInfo : decoder.getVarInfo()) {
        Variable *var = Variable::createFromInfo(args.mcl->toHandle(), varInfo);
        args.variableStorage->push_back(std::unique_ptr<Variable>(var));
        if (varInfo.tmp) {
            args.tempVariables->push_back(var);
        } else {
            args.variableMap->insert({varInfo.name, var});
        }
    }

    for (auto &kernelData : decoder.getKernelData()) {
        args.kernelData->emplace_back(std::make_unique<KernelData>(kernelData));
    }
    args.sbaVec->push_back(decoder.getSbaOffsets());

    const auto &kernelDispatchInfo = decoder.getKernelDispatchInfos();
    args.kernelDispatches->reserve(kernelDispatchInfo.size());
    for (size_t dispatchId = 0U; dispatchId < kernelDispatchInfo.size(); dispatchId++) {
        const auto &di = kernelDispatchInfo[dispatchId];
        auto kdPtr = std::make_unique<KernelDispatch>();
        args.kernelDispatches->emplace_back(std::move(kdPtr));
        auto &dispatch = *(args.kernelDispatches->at(dispatchId).get());

        dispatch.offsets = di.dispatchOffsets;
        dispatch.surfaceStateHeapSize = di.sshSize;

        auto segIoh = decoder.getSegments().ioh;
        dispatch.indirectObjectHeap = ArrayRef<const uint8_t>::fromAny(reinterpret_cast<const uint8_t *>(segIoh.cpuPtr + dispatch.offsets.crossThreadOffset), di.iohSize);

        UNRECOVERABLE_IF(di.kernelDataId == undefined<size_t>);
        dispatch.kernelData = args.kernelData->at(di.kernelDataId).get();

        Variable *groupCount = nullptr;
        if (di.groupCountVarId != undefined<size_t>) {
            groupCount = args.variableStorage->at(di.groupCountVarId).get();
            groupCount->getDesc().type = VariableType::groupCount;
        }

        Variable *groupSize = nullptr;
        if (di.groupSizeVarId != undefined<size_t>) {
            groupSize = args.variableStorage->at(di.groupSizeVarId).get();
            groupSize->getDesc().type = VariableType::groupSize;
        }

        auto walkerPtr = std::unique_ptr<MutableComputeWalker>(args.mcl->getCommandWalker(dispatch.offsets.walkerCmdOffset, dispatch.kernelData->indirectOffset, 0u));
        args.mutableWalkerCmds->emplace_back(std::move(walkerPtr));
        auto mutableCommandWalker = (*args.mutableWalkerCmds->rbegin()).get();

        auto offsets = std::make_unique<MutableIndirectData::Offsets>(di.indirectDataOffsets);
        size_t crossThreadSize = 0;
        size_t perThreadSize = 0;
        void *perThreadDataAddress = nullptr;
        if (dispatch.offsets.perThreadOffset != undefined<IndirectObjectHeapOffset>) {
            crossThreadSize = dispatch.offsets.perThreadOffset - dispatch.offsets.crossThreadOffset;
            perThreadSize = di.iohSize - crossThreadSize;
            perThreadDataAddress = reinterpret_cast<void *>(segIoh.cpuPtr + dispatch.offsets.perThreadOffset);
        } else {
            crossThreadSize = di.iohSize - dispatch.offsets.crossThreadOffset;
        }

        ArrayRef<uint8_t> inlineData;
        if (dispatch.kernelData->passInlineData) {
            inlineData = {reinterpret_cast<uint8_t *>(mutableCommandWalker->getInlineDataPointer()), mutableCommandWalker->getInlineDataSize()};
        }

        auto crossThreadData = ArrayRef<uint8_t>::fromAny(reinterpret_cast<uint8_t *>(segIoh.cpuPtr + dispatch.offsets.crossThreadOffset), crossThreadSize);
        auto perThreadData = ArrayRef<uint8_t>::fromAny(reinterpret_cast<uint8_t *>(perThreadDataAddress), perThreadSize);

        auto mutableIndirectData = std::make_unique<MutableIndirectData>(std::move(offsets), crossThreadData, perThreadData, inlineData);

        if (isDefined(dispatch.offsets.sshOffset)) {
            auto bindingTableAddress = decoder.getSegments().ssh.address + dispatch.offsets.sshOffset + dispatch.offsets.btOffset;
            mutableCommandWalker->setBindingTablePointer(bindingTableAddress);
        }
        mutableCommandWalker->setKernelStartAddress(dispatch.kernelData->kernelStartAddress);
        auto indirectDataStartAddress = decoder.getSegments().ioh.address + dispatch.offsets.crossThreadOffset;
        mutableCommandWalker->setIndirectDataStartAddress(indirectDataStartAddress);

        const uint32_t initialGroupCount[3] = {1, 1, 1};
        const uint32_t initialGroupSize[3] = {1, 1, 1};
        const uint32_t initialGlobalOffset[3] = {0, 0, 0};

        MutableKernelDispatchParameters dispatchParams = {
            initialGroupCount,                    // groupCount
            initialGroupSize,                     // groupSize
            initialGlobalOffset,                  // globalOffset
            0,                                    // perThreadSize
            0,                                    // walkOrder
            1,                                    // numThreadsPerThreadGroup
            std::numeric_limits<uint32_t>::max(), // threadExecutionMask
            1,                                    // maxWorkGroupCountPerTile
            0,                                    // maxCooperativeGroupCount
            NEO::localRegionSizeParamNotSet,      // localRegionSize
            NEO::RequiredPartitionDim::none,      // requiredPartitionDim
            NEO::RequiredDispatchWalkOrder::none, // requiredDispatchWalkOrder
            false,                                // generationOfLocalIdsByRuntime
            false};                               // cooperativeDispatch

        dispatch.varDispatch = std::make_unique<VariableDispatch>(&dispatch, std::move(mutableIndirectData), mutableCommandWalker,
                                                                  groupSize, groupCount, nullptr, nullptr, args.device->getHwInfo().capabilityTable.grfSize,
                                                                  dispatchParams, args.partitionCount, args.cmdListEngine, false);
    }
}

void patchIncrement(CpuAddress address, uint64_t offset, uint64_t value, Relocations::RelocType type) {
    using RelocType = Relocations::RelocType;
    switch (type) {
    default:
        DEBUG_BREAK_IF(true);
        break;
    case RelocType::address: {
        uint64_t incrementValue = value;
        auto currentVal = reinterpret_cast<uint64_t *>(address + offset);
        *currentVal += incrementValue;
    } break;
    case RelocType::address32: {
        uint32_t incrementValue = static_cast<uint32_t>(value & 0xffffffff);
        auto currentVal = reinterpret_cast<uint32_t *>(address + offset);
        *currentVal += incrementValue;
    } break;
    case RelocType::address32Hi: {
        uint32_t incrementValue = static_cast<uint32_t>((value >> 32) & 0xffffffff);
        auto currentVal = reinterpret_cast<uint32_t *>(address + offset);
        *currentVal += incrementValue;
    } break;
    }
}

template <typename T>
void patch(CpuAddress address, uint64_t value, uint64_t shift = 0) {
    auto addr = reinterpret_cast<T *>(address);
    auto val = static_cast<T>((value << shift) >> shift);

    *addr = (*addr & maxNBitValue(shift)) | val;
}

void patch(CpuAddress address, uint64_t offset, uint64_t value, int64_t addend, Relocations::RelocType type) {
    using RelocType = Relocations::RelocType;
    switch (type) {
    default:
        DEBUG_BREAK_IF(true);
        return;
    case RelocType::address:
        return patch<uint64_t>(address + offset, value + addend);
    case RelocType::address32:
        value = value & 0xffffffff;
        return patch<uint32_t>(address + offset, value + addend);
    case RelocType::address32Hi:
        value = (value >> 32) & 0xffffffff;
        return patch<uint32_t>(address + offset, value + addend);
    }
}

bool MclDecoder::decodeElf(ArrayRef<const uint8_t> mclElfBinary) {
    std::string err, warn;
    auto elf = NEO::Elf::decodeElf(mclElfBinary, err, warn);
    if (false == err.empty()) {
        return false;
    }

    using ElfSectionHeader = NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64>;
    auto sectionHeaderPtr = reinterpret_cast<const ElfSectionHeader *>(mclElfBinary.begin() + elf.elfFileHeader->shOff);
    program.elfSh = {sectionHeaderPtr, elf.elfFileHeader->shNum};

    for (size_t i = 0U; i < elf.sectionHeaders.size(); i++) {
        auto &sec = elf.sectionHeaders[i];
        auto hdr = sec.header;
        auto data = sec.data;

        using SectionType = Sections::SectionType;
        auto secType = static_cast<SectionType>(hdr->type);
        if (secType >= SectionType::shtMclStart && secType <= SectionType::shtMclEnd) {
            auto segment = getSegmentBySecType(secType);
            segment->initData = data;
        } else if (secType == SectionType::shtStrtab && elf.getSectionName(static_cast<uint32_t>(i)).compare(Sections::SectionNames::strtabName.str()) == 0U) {
            program.elfStrTabData = {reinterpret_cast<const char *>(data.begin()), data.size()};
        } else if (secType == SectionType::shtSymtab) {
            program.elfSymbolData = {reinterpret_cast<const ElfSymbol *>(data.begin()), hdr->size / hdr->entsize};
        } else if (secType == SectionType::shtRela) {
            auto targetSectionType = static_cast<SectionType>(elf.sectionHeaders[hdr->info].header->type);
            program.elfRelocs.push_back({targetSectionType,
                                         {reinterpret_cast<const ElfRela *>(data.begin()), hdr->size / hdr->entsize}});
        }
    }
    return true;
}

void MclDecoder::createRequiredAllocations(NEO::Device *device, NEO::CommandContainer &cmdCtr,
                                           AllocPtrT &outIHAlloc, AllocPtrT &outConstsAlloc, AllocPtrT &outVarsAlloc,
                                           std::unique_ptr<char[]> &outStringsAlloc, bool heapless) {
    auto memoryManager = device->getMemoryManager();

    auto ihAllocPtr = memoryManager->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), program.segments.ih.initData.size(), NEO::AllocationType::kernelIsa, device->getDeviceBitfield()});
    UNRECOVERABLE_IF(ihAllocPtr == nullptr);
    cmdCtr.addToResidencyContainer(ihAllocPtr);

    if (heapless) {
        program.segments.ih.address = ihAllocPtr->getGpuAddress();
    } else {
        program.segments.ih.address = ihAllocPtr->getGpuAddressToPatch();
    }
    program.segments.ih.cpuPtr = reinterpret_cast<CpuAddress>(ihAllocPtr->getUnderlyingBuffer());
    outIHAlloc = AllocPtrT(ihAllocPtr);

    if (false == program.segments.consts.initData.empty()) {
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(
            {device->getRootDeviceIndex(), program.segments.consts.initData.size(), NEO::AllocationType::constantSurface, device->getDeviceBitfield()});
        UNRECOVERABLE_IF(allocation == nullptr);
        cmdCtr.addToResidencyContainer(allocation);

        program.segments.consts.address = allocation->getGpuAddressToPatch();
        program.segments.consts.cpuPtr = reinterpret_cast<CpuAddress>(allocation->getUnderlyingBuffer());
        outConstsAlloc = AllocPtrT(allocation);
    }

    if (false == program.segments.vars.initData.empty()) {
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(
            {device->getRootDeviceIndex(), program.segments.vars.initData.size(), NEO::AllocationType::globalSurface, device->getDeviceBitfield()});
        UNRECOVERABLE_IF(allocation == nullptr);
        cmdCtr.addToResidencyContainer(allocation);

        program.segments.vars.address = allocation->getGpuAddressToPatch();
        program.segments.vars.cpuPtr = reinterpret_cast<CpuAddress>(allocation->getUnderlyingBuffer());
        outVarsAlloc = AllocPtrT(allocation);
    }

    if (false == program.segments.strings.initData.empty()) {
        outStringsAlloc.reset(new char[program.segments.strings.initData.size()]);
        program.segments.strings.cpuPtr = reinterpret_cast<CpuAddress>(outStringsAlloc.get());
        program.segments.strings.address = reinterpret_cast<GpuAddress>(outStringsAlloc.get());
    }

    baseAddresses.generalState = cmdCtr.getIndirectObjectHeapBaseAddress();
    baseAddresses.instruction = cmdCtr.getInstructionHeapBaseAddress();

    auto ioh = cmdCtr.getIndirectHeap(NEO::HeapType::indirectObject);
    baseAddresses.indirectObject = ioh->getHeapGpuBase();

    if (heapless) {
        program.segments.ioh.address = ioh->getGraphicsAllocation()->getGpuAddress();
    } else {
        program.segments.ioh.address = ioh->getHeapGpuStartOffset();
    }
    program.segments.ioh.cpuPtr = reinterpret_cast<CpuAddress>(ioh->getGraphicsAllocation()->getUnderlyingBuffer());

    auto ssh = cmdCtr.getIndirectHeap(NEO::HeapType::surfaceState);
    if (false == program.segments.ssh.initData.empty()) {
        baseAddresses.surfaceState = ssh->getHeapGpuBase();

        program.segments.ssh.address = ssh->getHeapGpuStartOffset();
        program.segments.ssh.cpuPtr = reinterpret_cast<CpuAddress>(ssh->getGraphicsAllocation()->getUnderlyingBuffer());
    }

    NEO::LinearStream *csStream = cmdCtr.getCommandStream();
    program.segments.cs.cpuPtr = reinterpret_cast<CpuAddress>(csStream->getCpuBase());

    // copy to allocs
    auto copyMemToAlloc = [&memoryManager](auto alloc, const auto &src) {
        memoryManager->copyMemoryToAllocation(alloc, 0, src.begin(), src.size());
    };
    copyMemToAlloc(csStream->getGraphicsAllocation(), program.segments.cs.initData);
    UNRECOVERABLE_IF(csStream->getUsed() > program.segments.cs.initData.size());
    csStream->getSpace(program.segments.cs.initData.size() - csStream->getUsed());
    copyMemToAlloc(outIHAlloc.get(), program.segments.ih.initData);
    if (false == program.segments.ssh.initData.empty()) {
        UNRECOVERABLE_IF(ssh == nullptr);
        copyMemToAlloc(ioh->getGraphicsAllocation(), program.segments.ioh.initData);
    }

    if (false == program.segments.consts.initData.empty()) {
        copyMemToAlloc(outConstsAlloc.get(), program.segments.consts.initData);
    }
    if (false == program.segments.vars.initData.empty()) {
        copyMemToAlloc(outVarsAlloc.get(), program.segments.vars.initData);
    }
    if (false == program.segments.strings.initData.empty()) {
        memcpy_s(outStringsAlloc.get(), program.segments.strings.initData.size(),
                 program.segments.strings.initData.begin(), program.segments.strings.initData.size());
    }
}

void MclDecoder::parseSymbols() {
    using SymType = Symbols::SymbolType;
    program.symbols.resize(program.elfSymbolData.size());

    for (size_t symId = 0; symId < program.elfSymbolData.size(); symId++) {
        const auto &elfSymbol = program.elfSymbolData[symId];
        auto symbolName = getSymName(elfSymbol.name);

        Symbols::Symbol &symbol = program.symbols[symId];
        symbol.type = static_cast<SymType>(elfSymbol.getType());
        symbol.section = static_cast<Sections::SectionType>(program.elfSh[elfSymbol.shndx].type);
        symbol.size = elfSymbol.size;
        symbol.value = elfSymbol.value;
        symbol.name = symbolName.str();

        switch (symbol.type) {
        default:
            relocateSymbol(symbol);
            break;
        case SymType::base:
            parseBaseSymbol(symbolName, symbol);
            break;
        case SymType::dispatch:
            parseDispatchSymbol(symbolName, symbol);
            break;
        case SymType::var:
            parseVarSymbol(symbolName, symbol);
            break;
        case SymType::heap: {
            if (symbol.section == Sections::SectionType::shtIh) {
                KernelData kernelData{};
                kernelData.kernelName = symbolName.substr(Symbols::SymbolNames::kernelPrefix.length()).str();
                kernelData.kernelStartOffset = symbol.value;
                kernelData.kernelStartAddress = program.segments.ih.address + symbol.value;
                kernelData.kernelIsa = {program.segments.ih.initData.begin() + symbol.value, symbol.size};
                kernelDataVec.push_back(kernelData);
            } else if (symbol.section == Sections::SectionType::shtIoh) {
                auto dispatchId = std::stoi(symbolName.substr(Symbols::SymbolNames::iohPrefix.length()).str());
                UNRECOVERABLE_IF(static_cast<size_t>(dispatchId) >= dispatchInfos.size());
                auto &dispatch = dispatchInfos[dispatchId];
                dispatch.iohSize = symbol.size;
            } else if (symbol.section == Sections::SectionType::shtSsh) {
                auto dispatchId = std::stoi(symbolName.substr(Symbols::SymbolNames::sshPrefix.length()).str());
                UNRECOVERABLE_IF(static_cast<size_t>(dispatchId) >= dispatchInfos.size());
                auto &dispatch = dispatchInfos[dispatchId];
                dispatch.sshSize = symbol.size;
            }
        } break;
        case SymType::kernel: {
            Symbols::KernelSymbol kernelSymbol(symbol.value);
            auto &kernelData = kernelDataVec[kernelSymbol.kernelDataId];
            kernelData.simdSize = kernelSymbol.simdSize;
            kernelData.skipPerThreadDataLoad = kernelSymbol.skipPerThreadDataLoad;
            kernelData.passInlineData = kernelSymbol.passInlineData;
            kernelData.indirectOffset = (kernelSymbol.indirectOffset << Symbols::KernelSymbol::indirectOffsetBitShift);
            kernelData.grfCount = GrfConfig::defaultGrfNumber;
        } break;
        }
    }
}

void MclDecoder::relocateSymbol(Symbols::Symbol &symbol) {
    using SecType = Sections::SectionType;

    switch (symbol.section) {
    default:
        break;
    case SecType::shtIh:
        symbol.value += program.segments.ih.address;
        break;
    case SecType::shtIoh:
        symbol.value += program.segments.ioh.address;
        break;
    case SecType::shtSsh:
        symbol.value += program.segments.ssh.address;
        break;
    case SecType::shtDataConst:
        symbol.value += program.segments.consts.address;
        break;
    case SecType::shtDataVar:
        symbol.value += program.segments.vars.address;
        break;
    case SecType::shtDataStrings:
        symbol.value += program.segments.strings.address;
        break;
    }
}

void MclDecoder::parseBaseSymbol(NEO::ConstStringRef symbolName, Symbols::Symbol &symbol) {
    using namespace Symbols::SymbolNames;

    if (symbolName == gpuBaseSymbol) {
        symbol.value = baseAddresses.generalState;
    } else if (symbolName == ihBaseSymbol) {
        symbol.value = baseAddresses.instruction;
    } else if (symbolName == iohBaseSymbol) {
        symbol.value = baseAddresses.indirectObject;
    } else if (symbolName == sshBaseSymbol) {
        symbol.value = baseAddresses.surfaceState;
    } else {
        DEBUG_BREAK_IF(true);
    }
}

void MclDecoder::parseVarSymbol(NEO::ConstStringRef symbolName, Symbols::Symbol &symbol) {
    using namespace Symbols::SymbolNames;
    Symbols::VariableSymbolValue varSym(symbol.value);

    VarInfo varInfo = {};
    varInfo.size = symbol.size;
    varInfo.type = static_cast<VariableType>(varSym.variableType);
    varInfo.scalable = varSym.flags.isScalable;
    varInfo.tmp = varSym.flags.isTmp;
    varInfo.name = varSym.flags.isTmp
                       ? symbolName.substr(varPrefix.length() + tmpPrefix.length()).str()
                       : symbolName.substr(varPrefix.length()).str();

    if (varSym.id >= varInfos.size()) {
        varInfos.resize(varSym.id + 1);
    }
    varInfos[varSym.id] = std::move(varInfo);
}

void MclDecoder::parseDispatchSymbol(NEO::ConstStringRef symbolName, Symbols::Symbol &symbol) {
    Symbols::DispatchSymbolValue dSymVal(symbol.value);

    if (dSymVal.dispatchId >= dispatchInfos.size()) {
        dispatchInfos.resize(dSymVal.dispatchId + 1);
    }
    auto &di = dispatchInfos[dSymVal.dispatchId];
    di.groupSizeVarId = dSymVal.groupCountVarId;
    di.groupCountVarId = dSymVal.groupSizeVarId;
    di.kernelDataId = dSymVal.kernelDataId;
}

void MclDecoder::parseRelocations() {
    for (auto &[targetSecType, relocs] : program.elfRelocs) {
        for (auto &reloc : relocs) {
            auto symbolIdx = reloc.getSymbolTableIndex();
            auto &symbol = program.symbols[symbolIdx];
            switch (symbol.type) {
            default:
                parseDefaultRelocation(targetSecType, symbol.value, reloc);
                break;
            case Symbols::SymbolType::base: {
                UNRECOVERABLE_IF(targetSecType != Sections::SectionType::shtCs);
                parseBaseRelocation(symbol, reloc);
            } break;
            case Symbols::SymbolType::dispatch: {
                Symbols::DispatchSymbolValue diSym(symbol.value);
                parseDispatchRelocation(targetSecType, dispatchInfos[diSym.dispatchId], reloc);
            } break;
            case Symbols::SymbolType::var: {
                Symbols::VariableSymbolValue varSym(symbol.value);
                parseVarRelocation(targetSecType, varInfos[varSym.id], reloc);
            } break;
            }
        }
    }
}

void MclDecoder::parseDefaultRelocation(Sections::SectionType targetSecType, uint64_t value, const ElfRela &reloc) {
    const auto &segments = program.segments;
    auto relocType = static_cast<Relocations::RelocType>(reloc.getRelocationType());
    switch (targetSecType) {
    default:
        DEBUG_BREAK_IF(true);
        return;
    case Sections::SectionType::shtCs:
        return patch(segments.cs.cpuPtr, reloc.offset, value, reloc.addend, relocType);
    case Sections::SectionType::shtIh:
        return patch(segments.ih.cpuPtr, reloc.offset, value, reloc.addend, relocType);
    case Sections::SectionType::shtDataConst:
        return patchIncrement(segments.consts.cpuPtr, reloc.offset, value, relocType);
    case Sections::SectionType::shtDataVar:
        return patchIncrement(segments.vars.cpuPtr, reloc.offset, value, relocType);
    }
}

void MclDecoder::parseBaseRelocation(const Symbols::Symbol &symbol, const ElfRela &reloc) {
    auto address = program.segments.cs.cpuPtr + reloc.offset;
    patch<uint64_t>(address, symbol.value, 0xc);
    if (NEO::ConstStringRef(symbol.name) == Symbols::SymbolNames::gpuBaseSymbol) {
        sba.gsba = reloc.offset;
    } else if (NEO::ConstStringRef(symbol.name) == Symbols::SymbolNames::ihBaseSymbol) {
        sba.isba = reloc.offset;
    } else if (NEO::ConstStringRef(symbol.name) == Symbols::SymbolNames::sshBaseSymbol) {
        sba.ssba = reloc.offset;
    }
}

void MclDecoder::parseDispatchRelocation(Sections::SectionType targetSecType, KernelDispatchInfo &di, const ElfRela &reloc) {
    using RelocType = Relocations::RelocType;
    using SecType = Sections::SectionType;

    switch (targetSecType) {
    default:
        DEBUG_BREAK_IF(true);
        break;
    case SecType::shtCs: {
        switch (reloc.getRelocationType()) {
        default:
            DEBUG_BREAK_IF(true);
            break;
        case RelocType::walkerCommand:
            di.dispatchOffsets.walkerCmdOffset = reloc.offset;
            break;
        }
    } break;
    case SecType::shtSsh: {
        switch (reloc.getRelocationType()) {
        default:
            DEBUG_BREAK_IF(true);
            break;
        case RelocType::surfaceStateHeapStart:
            di.dispatchOffsets.sshOffset = reloc.offset;
            break;
        case RelocType::bindingTable:
            DEBUG_BREAK_IF(di.dispatchOffsets.sshOffset == undefined<SurfaceStateHeapOffset>);
            di.dispatchOffsets.btOffset = static_cast<LocalSshOffset>(reloc.offset - di.dispatchOffsets.sshOffset);
            break;
        }
    } break;
    case SecType::shtIh: {
        switch (reloc.getRelocationType()) {
        default:
            DEBUG_BREAK_IF(true);
            break;
        case RelocType::kernelStart:
            di.dispatchOffsets.kernelStartOffset = reloc.offset;
            break;
        }
    } break;
    case SecType::shtIoh: {
        auto getCtdOffset = [&di](auto &trait, auto offset) {
            UNRECOVERABLE_IF(di.dispatchOffsets.crossThreadOffset == undefined<IndirectObjectHeapOffset>);
            trait = static_cast<CrossThreadDataOffset>(offset - di.dispatchOffsets.crossThreadOffset);
        };
        switch (reloc.getRelocationType()) {
        default:
            DEBUG_BREAK_IF(true);
            break;

        case RelocType::crossThreadDataStart:
            di.dispatchOffsets.crossThreadOffset = reloc.offset;
            break;
        case RelocType::perThreadDataStart:
            di.dispatchOffsets.perThreadOffset = reloc.offset;
            break;
        case RelocType::numWorkGroups:
            getCtdOffset(di.indirectDataOffsets.numWorkGroups, reloc.offset);
            break;
        case RelocType::localWorkSize:
            getCtdOffset(di.indirectDataOffsets.localWorkSize, reloc.offset);
            break;
        case RelocType::localWorkSize2:
            getCtdOffset(di.indirectDataOffsets.localWorkSize2, reloc.offset);
            break;
        case RelocType::enqLocalWorkSize:
            getCtdOffset(di.indirectDataOffsets.enqLocalWorkSize, reloc.offset);
            break;
        case RelocType::globalWorkSize:
            getCtdOffset(di.indirectDataOffsets.globalWorkSize, reloc.offset);
            break;
        case RelocType::workDimensions:
            getCtdOffset(di.indirectDataOffsets.workDimensions, reloc.offset);
            break;
        }
    } break;
    }
}

void MclDecoder::parseVarRelocation(Sections::SectionType targetSecType, VarInfo &varInfo, const ElfRela &reloc) {
    using RelocType = Relocations::RelocType;
    using SecType = Sections::SectionType;

    OffsetT offset = static_cast<OffsetT>(reloc.offset);
    switch (varInfo.type) {
    default:
        DEBUG_BREAK_IF(true);
        break;
    case VariableType::buffer: {
        auto &bufferUsages = varInfo.bufferUsages;
        switch (targetSecType) {
        default:
            DEBUG_BREAK_IF(true);
            break;
        case SecType::shtCs: {
            switch (reloc.getRelocationType()) {
            default:
                DEBUG_BREAK_IF(true);
                break;
            case RelocType::bufferAddress:
                bufferUsages.commandBufferOffsets.push_back(offset);
                break;
            }
        } break;
        case SecType::shtIoh: {
            switch (reloc.getRelocationType()) {
            default:
                DEBUG_BREAK_IF(true);
                break;
            case RelocType::bufferStateless:
                bufferUsages.statelessIndirect.push_back(offset);
                break;
            case RelocType::bufferBindless:
                bufferUsages.bindless.push_back(offset);
                break;
            case RelocType::bufferBindlessWithoutOffset:
                bufferUsages.bindlessWithoutOffset.push_back(offset);
                break;
            case RelocType::bufferOffset:
                bufferUsages.bufferOffset.push_back(offset);
                break;
            }
        } break;
        case SecType::shtSsh: {
            switch (reloc.getRelocationType()) {
            default:
                DEBUG_BREAK_IF(true);
                break;
            case RelocType::bufferBindful:
                bufferUsages.bindful.push_back(offset);
                break;
            case RelocType::bufferBindfulWithoutOffset:
                bufferUsages.bindfulWithoutOffset.push_back(offset);
                break;
            }
        } break;
        }
    } break;
    case VariableType::value: {
        auto &valueUsages = varInfo.valueUsages;
        switch (targetSecType) {
        default:
            DEBUG_BREAK_IF(true);
            break;
        case SecType::shtCs: {
            switch (reloc.getRelocationType()) {
            default:
                DEBUG_BREAK_IF(true);
                break;
            case RelocType::valueStateless:
                valueUsages.statelessIndirect.push_back(offset);
                break;
            case RelocType::valueStatelessSize:
                valueUsages.statelessIndirectPatchSize.push_back(offset);
                break;
            case RelocType::valueCmdBuffer:
                valueUsages.commandBufferOffsets.push_back(offset);
                break;
            case RelocType::valueCmdBufferSize:
                valueUsages.commandBufferPatchSize.push_back(offset);
                break;
            }
        } break;
        case SecType::shtIoh: {
            switch (reloc.getRelocationType()) {
            default:
                DEBUG_BREAK_IF(true);
                break;
            case RelocType::valueStateless:
                valueUsages.statelessIndirect.push_back(offset);
                break;
            case RelocType::valueStatelessSize:
                valueUsages.statelessIndirectPatchSize.push_back(offset);
                break;
            case RelocType::valueCmdBuffer:
                valueUsages.commandBufferOffsets.push_back(offset);
                break;
            case RelocType::valueCmdBufferSize:
                valueUsages.commandBufferPatchSize.push_back(offset);
                break;
            }
        } break;
        }
    };
    }
}

MclDecoder::Segment *MclDecoder::getSegmentBySecType(Sections::SectionType type) {
    using SectionType = Sections::SectionType;
    auto &segments = program.segments;
    switch (type) {
    default:
        DEBUG_BREAK_IF(true);
        return nullptr;
    case SectionType::shtCs:
        return &segments.cs;
    case SectionType::shtIh:
        return &segments.ih;
    case SectionType::shtSsh:
        return &segments.ssh;
    case SectionType::shtIoh:
        return &segments.ioh;
    case SectionType::shtDataConst:
        return &segments.consts;
    case SectionType::shtDataVar:
        return &segments.vars;
    case SectionType::shtDataStrings:
        return &segments.strings;
    }
}
} // namespace L0::MCL::Program::Decoder
