/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/utilities/stackvec.h"

#include <string>
#include <vector>

namespace AOT {
enum PRODUCT_CONFIG : uint32_t;
}

namespace NEO {
namespace Elf {
template <NEO::Elf::ELF_IDENTIFIER_CLASS numBits>
struct Elf;
}
struct ProgramInfo;

namespace Zebin {

template <Elf::ELF_IDENTIFIER_CLASS numBits = Elf::EI_CLASS_64>
struct ZebinSections {
    using SectionHeaderData = typename NEO::Elf::Elf<numBits>::SectionHeaderAndData;
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

template <Elf::ELF_IDENTIFIER_CLASS numBits>
bool isZebin(ArrayRef<const uint8_t> binary);

bool validateTargetDevice(const TargetDevice &targetDevice, Elf::ELF_IDENTIFIER_CLASS numBits, PRODUCT_FAMILY productFamily, GFXCORE_FAMILY gfxCore, AOT::PRODUCT_CONFIG productConfig, Zebin::Elf::ZebinTargetFlags targetMetadata);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
bool validateTargetDevice(const Elf::Elf<numBits> &elf, const TargetDevice &targetDevice, std::string &outErrReason, std::string &outWarning, GeneratorType &generatorType);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError decodeIntelGTNoteSection(ArrayRef<const uint8_t> intelGTNotesSection, std::vector<Elf::IntelGTNote> &intelGTNotes, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError getIntelGTNotes(const Elf::Elf<numBits> &elf, std::vector<Elf::IntelGTNote> &intelGTNotes, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError extractZebinSections(Elf::Elf<numBits> &elf, ZebinSections<numBits> &out, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError validateZebinSectionsCount(const ZebinSections<numBits> &sections, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError decodeZebin(ProgramInfo &dst, Elf::Elf<numBits> &elf, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ArrayRef<const uint8_t> getKernelHeap(ConstStringRef &kernelName, Elf::Elf<numBits> &elf, const ZebinSections<numBits> &zebinSections);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ArrayRef<const uint8_t> getKernelGtpinInfo(ConstStringRef &kernelName, Elf::Elf<numBits> &elf, const ZebinSections<numBits> &zebinSections);

void setKernelMiscInfoPosition(ConstStringRef metadata, ProgramInfo &dst);

ConstStringRef getZeInfoFromZebin(const ArrayRef<const uint8_t> zebin, std::string &outErrReason, std::string &outWarning);

} // namespace Zebin
} // namespace NEO
