/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/device_binary_format/zebin_decoder.h"
#include "shared/source/utilities/const_stringref.h"
#include "shared/test/common/mocks/mock_elf.h"

#include "igfxfmid.h"

#include <string>
#include <vector>

extern PRODUCT_FAMILY productFamily;

inline std::string versionToString(NEO::Elf::ZebinKernelMetadata::Types::Version version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor);
}

namespace ZebinTestData {

enum class appendElfAdditionalSection {
    NONE,
    SPIRV,
    GLOBAL,
    CONSTANT,
    CONSTANT_STRING
};

struct ValidEmptyProgram {
    static constexpr char kernelName[19] = "valid_empty_kernel";

    ValidEmptyProgram();
    void recalcPtr();
    NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64> &appendSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData);
    void removeSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel);

    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *elfHeader;
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
    std::string zeInfo = std::string("version :\'") + versionToString(NEO::zeInfoDecoderVersion) + R"===('
kernels:
    - name: kernel
      execution_env:
        simd_size: 8
    - name: Intel_Symbol_Table_Void_Program
      execution_env:
        simd_size: 8
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
    ZebinWithL0TestCommonModule(const NEO::HardwareInfo &hwInfo) : ZebinWithL0TestCommonModule(hwInfo, std::initializer_list<appendElfAdditionalSection>{}) {}
    ZebinWithL0TestCommonModule(const NEO::HardwareInfo &hwInfo, std::initializer_list<appendElfAdditionalSection> additionalSections, bool forceRecompilation = false);

    void recalcPtr();

    const uint8_t numOfKernels = 2;
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *elfHeader;
    std::vector<uint8_t> storage;
    std::string zeInfo = std::string("version :\'") + versionToString(NEO::zeInfoDecoderVersion) + R"===('
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
        size:            0
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
  - name:            memcpy_bytes
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
-)===";
};

} // namespace ZebinTestData