/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>

namespace NEO {

// Real IGC YAML, used to test the parser against IGC's actual YAML shape.
inline constexpr const char *spirvExtensionsYamlIgcSample = R"(---
- name:            SPV_EXT_optnone
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_optnone.html'
  supported_capabilities:
    - id:              6094
      name:            OptNoneEXT
- name:            SPV_EXT_shader_atomic_float_add
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_atomic_float_add.html'
  supported_capabilities:
    - id:              6033
      name:            AtomicFloat32AddEXT
- name:            SPV_EXT_shader_atomic_float_min_max
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_atomic_float_min_max.html'
  supported_capabilities:
    - id:              5612
      name:            AtomicFloat32MinMaxEXT
- name:            SPV_INTEL_cache_controls
  spec_url:        'https://github.com/KhronosGroup/SPIRV-Registry/blob/main/extensions/INTEL/SPV_INTEL_cache_controls.asciidoc'
  supported_capabilities:
    - id:              6441
      name:            CacheControlsINTEL
- name:            SPV_INTEL_global_variable_host_access
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_global_variable_host_access.html'
  supported_capabilities:
    - id:              6187
      name:            GlobalVariableHostAccessINTEL
- name:            SPV_INTEL_long_composites
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_long_composites.html'
  supported_capabilities:
    - id:              6089
      name:            LongCompositesINTEL
- name:            SPV_INTEL_split_barrier
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_split_barrier.html'
  supported_capabilities:
    - id:              6141
      name:            SplitBarrierINTEL
- name:            SPV_INTEL_subgroup_buffer_prefetch
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_subgroup_buffer_prefetch.html'
  supported_capabilities:
    - id:              6220
      name:            SubgroupBufferPrefetchINTEL
- name:            SPV_INTEL_subgroups
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_subgroups.html'
  supported_capabilities:
    - id:              5568
      name:            SubgroupShuffleINTEL
    - id:              5569
      name:            SubgroupBufferBlockIOINTEL
    - id:              5570
      name:            SubgroupImageBlockIOINTEL
- name:            SPV_INTEL_unstructured_loop_controls
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_unstructured_loop_controls.html'
  supported_capabilities:
    - id:              5886
      name:            UnstructuredLoopControlsINTEL
- name:            SPV_KHR_bit_instructions
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_bit_instructions.html'
  supported_capabilities:
    - id:              6025
      name:            BitInstructions
- name:            SPV_KHR_expect_assume
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_expect_assume.html'
  supported_capabilities:
    - id:              5629
      name:            ExpectAssumeKHR
- name:            SPV_KHR_integer_dot_product
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_integer_dot_product.html'
  supported_capabilities:
    - id:              6019
      name:            DotProductKHR
    - id:              6017
      name:            DotProductInput4x8BitKHR
    - id:              6018
      name:            DotProductInput4x8BitPackedKHR
- name:            SPV_KHR_linkonce_odr
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_linkonce_odr.html'
  supported_capabilities:
    - id:              5
      name:            Linkage
- name:            SPV_KHR_no_integer_wrap_decoration
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_no_integer_wrap_decoration.html'
  supported_capabilities: []
- name:            SPV_KHR_shader_clock
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_shader_clock.html'
  supported_capabilities:
    - id:              5055
      name:            ShaderClockKHR
...
)";

// Expected results of parsing spirvExtensionsYamlIgcSample.
inline constexpr size_t spirvExtensionsYamlIgcSampleExtensionCount = 16u;
inline constexpr size_t spirvExtensionsYamlIgcSampleCapabilityCount = 19u;

} // namespace NEO
