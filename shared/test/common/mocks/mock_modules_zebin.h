/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/const_stringref.h"

#include "neo_igfxfmid.h"

#include <string>
#include <vector>

extern PRODUCT_FAMILY productFamily;

inline std::string versionToString(NEO::Zebin::ZeInfo::Types::Version version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor);
}

namespace ZebinTestData {

enum class AppendElfAdditionalSection {
    none,
    spirv,
    global,
    constant,
    constantString
};

template <NEO::Elf::ElfIdentifierClass numBits = NEO::Elf::EI_CLASS_64>
struct ValidEmptyProgram {
    static constexpr char kernelName[19] = "valid_empty_kernel";

    ValidEmptyProgram();
    void recalcPtr();
    NEO::Elf::ElfSectionHeader<numBits> &appendSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData);
    void removeSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel);

    NEO::Elf::ElfFileHeader<numBits> *elfHeader;
    std::vector<uint8_t> storage;
};

struct ZebinWithExternalFunctionsInfo {
    ZebinWithExternalFunctionsInfo();

    void recalcPtr();
    void setProductFamily(uint16_t productFamily);
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> getElf();

    std::unordered_map<std::string, uint32_t> nameToSegId;
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *elfHeader;
    NEO::ConstStringRef fun0Name = "fun0";
    NEO::ConstStringRef fun1Name = "fun1";
    const uint8_t barrierCount = 2;
    std::vector<uint8_t> storage;
    std::string zeInfo = std::string("version :\'") + versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
    - name: kernel
      execution_env:
        simd_size: 8
    - name: Intel_Symbol_Table_Void_Program
      execution_env:
        simd_size: 8
        indirect_stateless_count: 1
functions:
    - name: fun0
      execution_env:
        grf_count: 128
        simd_size: 8
        barrier_count: 0
    - name: fun1
      execution_env:
        grf_count: 128
        simd_size: 8
        barrier_count: 2
)===";
};

struct ZebinWithL0TestCommonModule {
    ZebinWithL0TestCommonModule(const NEO::HardwareInfo &hwInfo) : ZebinWithL0TestCommonModule(hwInfo, std::initializer_list<AppendElfAdditionalSection>{}) {}
    ZebinWithL0TestCommonModule(const NEO::HardwareInfo &hwInfo, std::initializer_list<AppendElfAdditionalSection> additionalSections, bool forceRecompilation = false);

    void recalcPtr();

    const uint8_t numOfKernels = 2;
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *elfHeader;
    std::vector<uint8_t> storage;
    std::string zeInfo = std::string("version :\'") + versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
  - name:            test
    execution_env:
      barrier_count:   1
      grf_count:       128
      has_global_atomics: true
      inline_data_payload_size: 32
      offset_to_skip_per_thread_data_load: 192
      simd_size:       32
      slm_size:        64
      subgroup_independent_forward_progress: true
    payload_arguments:
      - arg_type:        global_id_offset
        offset:          0
        size:            12
      - arg_type:        local_size
        offset:          12
        size:            12
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       0
        addrmode:        stateful
        addrspace:       global
        access_type:     readonly
      - arg_type:        arg_bypointer
        offset:          32
        size:            8
        arg_index:       0
        addrmode:        stateless
        addrspace:       global
        access_type:     readonly
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       1
        addrmode:        stateful
        addrspace:       global
        access_type:     readonly
      - arg_type:        arg_bypointer
        offset:          40
        size:            8
        arg_index:       1
        addrmode:        stateless
        addrspace:       global
        access_type:     readonly
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       2
        addrmode:        stateful
        addrspace:       global
        access_type:     readwrite
      - arg_type:        arg_bypointer
        offset:          48
        size:            8
        arg_index:       2
        addrmode:        stateless
        addrspace:       global
        access_type:     readwrite
      - arg_type:        printf_buffer
        offset:          56
        size:            8
      - arg_type:        buffer_offset
        offset:          64
        size:            4
        arg_index:       2
      - arg_type:        local_size
        offset:          68
        size:            12
      - arg_type:        enqueued_local_size
        offset:          80
        size:            12
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       5
        addrmode:        stateful
        addrspace:       sampler
        access_type:     readwrite
        sampler_index:   0
      - arg_type:        arg_bypointer
        offset:          0
        size:            4
        arg_index:       3
        addrmode:        stateful
        addrspace:       image
        access_type:     readonly
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       4
        addrmode:        stateful
        addrspace:       image
        access_type:     writeonly
    per_thread_payload_arguments:
      - arg_type:        local_id
        offset:          0
        size:            192
    binding_table_indices:
      - bti_value:       1
        arg_index:       0
      - bti_value:       2
        arg_index:       1
      - bti_value:       3
        arg_index:       2
      - bti_value:       0
        arg_index:       3
      - bti_value:       5
        arg_index:       4
  - name:            memcpy_bytes_attr
    user_attributes:
      work_group_size_hint: [1,1,1]
    execution_env:
      grf_count:       128
      has_no_stateless_write: true
      inline_data_payload_size: 32
      offset_to_skip_per_thread_data_load: 192
      simd_size:       32
      subgroup_independent_forward_progress: true
    payload_arguments:
      - arg_type:        global_id_offset
        offset:          0
        size:            12
      - arg_type:        local_size
        offset:          12
        size:            12
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       0
        addrmode:        stateful
        addrspace:       global
        access_type:     readwrite
      - arg_type:        arg_bypointer
        offset:          32
        size:            8
        arg_index:       0
        addrmode:        stateless
        addrspace:       global
        access_type:     readwrite
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       1
        addrmode:        stateful
        addrspace:       global
        access_type:     readonly
      - arg_type:        arg_bypointer
        offset:          40
        size:            8
        arg_index:       1
        addrmode:        stateless
        addrspace:       global
        access_type:     readonly
      - arg_type:        enqueued_local_size
        offset:          48
        size:            12
    per_thread_payload_arguments:
      - arg_type:        local_id
        offset:          0
        size:            192
    binding_table_indices:
      - bti_value:       0
        arg_index:       0
      - bti_value:       1
        arg_index:       1
)===";
};

template <NEO::Elf::ElfIdentifierClass numBits>
struct ZebinCopyBufferSimdModule {
    ZebinCopyBufferSimdModule(const NEO::HardwareInfo &hwInfo, uint8_t simdSize);
    inline size_t getLocalIdSize(const NEO::HardwareInfo &hwInfo, uint8_t simdSize) {
        return alignUp(simdSize * sizeof(uint16_t), hwInfo.capabilityTable.grfSize) * 3;
    }
    NEO::Elf::ElfFileHeader<numBits> *elfHeader;
    std::vector<uint8_t> storage;
    std::string zeInfoCopyBuffer;
    size_t zeInfoSize;
    std::string zeInfoCopyBufferSimdPlaceholder = std::string("version :\'") + versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
  - name:            CopyBuffer
    execution_env:
      disable_mid_thread_preemption: true
      grf_count:       128
      has_no_stateless_write: true
      inline_data_payload_size: 32
      offset_to_skip_per_thread_data_load: 192
      required_sub_group_size: %d
      simd_size:       %d
      subgroup_independent_forward_progress: true
    payload_arguments:
      - arg_type:        global_id_offset
        offset:          0
        size:            12
      - arg_type:        local_size
        offset:          12
        size:            12
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       0
        addrmode:        stateful
        addrspace:       global
        access_type:     readwrite
      - arg_type:        buffer_address
        offset:          32
        size:            8
        arg_index:       0
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       1
        addrmode:        stateful
        addrspace:       global
        access_type:     readwrite
      - arg_type:        buffer_address
        offset:          40
        size:            8
        arg_index:       1
      - arg_type:        buffer_offset
        offset:          48
        size:            4
        arg_index:       0
      - arg_type:        buffer_offset
        offset:          52
        size:            4
        arg_index:       1
    per_thread_payload_arguments:
      - arg_type:        local_id
        offset:          0
        size:            %d
    binding_table_indices:
      - bti_value:       0
        arg_index:       0
      - bti_value:       1
        arg_index:       1
)===";
};

size_t writeElfNote(ArrayRef<uint8_t> dst, ArrayRef<const uint8_t> desc, NEO::ConstStringRef name, uint32_t type);
size_t writeIntelGTNote(ArrayRef<uint8_t> dst, NEO::Zebin::Elf::IntelGTSectionType sectionType, ArrayRef<const uint8_t> desc);
std::vector<uint8_t> createIntelGTNoteSection(NEO::ConstStringRef version, AOT::PRODUCT_CONFIG productConfig);
std::vector<uint8_t> createIntelGTNoteSection(PRODUCT_FAMILY productFamily, GFXCORE_FAMILY coreFamily, NEO::Zebin::Elf::ZebinTargetFlags flags, NEO::ConstStringRef version);
} // namespace ZebinTestData
