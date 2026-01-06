/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/utilities/stackvec.h"

#include <string>
#include <vector>

namespace AOT {
enum PRODUCT_CONFIG : uint32_t; // NOLINT(readability-identifier-naming)
}

namespace NEO {
namespace Elf {
template <NEO::Elf::ElfIdentifierClass numBits>
struct Elf;
}
struct ProgramInfo;

namespace Zebin {

template <Elf::ElfIdentifierClass numBits = Elf::EI_CLASS_64>
struct ZebinSections {
    using SectionHeaderData = NEO::Elf::SectionHeaderAndData<numBits>;
    StackVec<SectionHeaderData *, 1> textSections;
    StackVec<SectionHeaderData *, 32> textKernelSections;
    StackVec<SectionHeaderData *, 32> gtpinInfoSections;
    StackVec<SectionHeaderData *, 1> zeInfoSections;
    StackVec<SectionHeaderData *, 1> globalDataSections;
    StackVec<SectionHeaderData *, 1> globalZeroInitDataSections;
    StackVec<SectionHeaderData *, 1> constDataSections;
    StackVec<SectionHeaderData *, 1> constZeroInitDataSections;
    StackVec<SectionHeaderData *, 1> constDataStringSections;
    StackVec<SectionHeaderData *, 1> symtabSections;
    StackVec<SectionHeaderData *, 1> spirvSections;
    StackVec<SectionHeaderData *, 1> noteIntelGTSections;
    StackVec<SectionHeaderData *, 1> buildOptionsSection;
};

template <Elf::ElfIdentifierClass numBits>
bool isZebin(ArrayRef<const uint8_t> binary);

bool validateTargetDevice(const TargetDevice &targetDevice, Elf::ElfIdentifierClass numBits, PRODUCT_FAMILY productFamily, GFXCORE_FAMILY gfxCore, AOT::PRODUCT_CONFIG productConfig, Zebin::Elf::ZebinTargetFlags targetMetadata);

template <Elf::ElfIdentifierClass numBits>
bool validateTargetDevice(const Elf::Elf<numBits> &elf, const TargetDevice &targetDevice, std::string &outErrReason, std::string &outWarning, GeneratorFeatureVersions &generatorFeatures, GeneratorType &generator);

template <Elf::ElfIdentifierClass numBits>
DecodeError decodeIntelGTNoteSection(ArrayRef<const uint8_t> intelGTNotesSection, std::vector<Elf::IntelGTNote> &intelGTNotes, std::string &outErrReason, std::string &outWarning);

template <Elf::ElfIdentifierClass numBits>
DecodeError getIntelGTNotes(const Elf::Elf<numBits> &elf, std::vector<Elf::IntelGTNote> &intelGTNotes, std::string &outErrReason, std::string &outWarning);

template <Elf::ElfIdentifierClass numBits>
DecodeError extractZebinSections(Elf::Elf<numBits> &elf, ZebinSections<numBits> &out, std::string &outErrReason, std::string &outWarning);

template <Elf::ElfIdentifierClass numBits>
DecodeError validateZebinSectionsCount(const ZebinSections<numBits> &sections, std::string &outErrReason, std::string &outWarning);

template <Elf::ElfIdentifierClass numBits>
DecodeError decodeZebin(ProgramInfo &dst, Elf::Elf<numBits> &elf, std::string &outErrReason, std::string &outWarning);

template <Elf::ElfIdentifierClass numBits>
ArrayRef<const uint8_t> getKernelHeap(ConstStringRef &kernelName, Elf::Elf<numBits> &elf, const ZebinSections<numBits> &zebinSections);

template <Elf::ElfIdentifierClass numBits>
ArrayRef<const uint8_t> getKernelGtpinInfo(ConstStringRef &kernelName, Elf::Elf<numBits> &elf, const ZebinSections<numBits> &zebinSections);

void setKernelMiscInfoPosition(ConstStringRef metadata, ProgramInfo &dst);

ConstStringRef getZeInfoFromZebin(const ArrayRef<const uint8_t> zebin, std::string &outErrReason, std::string &outWarning);

ConstStringRef getKernelNameFromSectionName(ConstStringRef sectionName);

} // namespace Zebin
} // namespace NEO
