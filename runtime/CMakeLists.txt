# Copyright (c) 2017, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
cmake_minimum_required (VERSION 3.0)

if (POLICY CMP0042)
  cmake_policy (SET CMP0042 NEW)
endif (POLICY CMP0042)

if (POLICY CMP0063)
  cmake_policy (SET CMP0063 NEW)
endif (POLICY CMP0063)

project (neo)

#set (CMAKE_CXX_VISIBILITY_PRESET default)
#set (CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Support for Windows Universal Drivers
ENABLE_WUD()

if(WIN32)
  set(GEN_OS_SRC
    windows/command_stream_receiver.cpp
    windows/wddm_engine_mapper.cpp
    windows/wddm.cpp
  )
else(WIN32)
  set(GEN_OS_SRC
    linux/command_stream_receiver.cpp
    linux/drm_engine_mapper.cpp
  )
endif(WIN32)

set(RUNTIME_SRCS_GENX
  aub_command_stream_receiver.cpp
  aub_mapper.h
  aub_mem_dump.cpp
  command_queue.cpp
  device_enqueue.h
  device_queue.cpp
  command_stream_receiver_hw.cpp
  hw_cmds.h
  hw_cmds_generated.h
  hw_helper.cpp
  hw_info.cpp
  hw_info.h
  buffer.cpp
  image.cpp
  kernel_commands.cpp
  preamble.cpp
  preemption.cpp
  reg_configs.h
  sampler.cpp
  scheduler_definitions.h
  scheduler_igdrcl_built_in.inl
  state_base_address.cpp
  tbx_command_stream_receiver.cpp
  ${GEN_OS_SRC}
)
if(NOT (TARGET ${BIKSIM_LIB_NAME}))
  add_subdirectory(builtin_kernels_simulation)
endif(NOT (TARGET ${BIKSIM_LIB_NAME}))

if(NOT (TARGET ${SCHEDULER_BINARY_LIB_NAME}))
  add_subdirectory("scheduler")
endif(NOT (TARGET ${SCHEDULER_BINARY_LIB_NAME}))

if(NOT (TARGET ${BUILTINS_BINARIES_LIB_NAME}))
  add_subdirectory("built_ins")
endif(NOT (TARGET ${BUILTINS_BINARIES_LIB_NAME}))

add_subdirectory(api)
add_subdirectory(accelerators)

set (RUNTIME_SRCS_AUB_MEM_DUMP
  aub_mem_dump/aub_mem_dump.cpp
  aub_mem_dump/aub_mem_dump.h
  aub_mem_dump/aub_mem_dump.inl
  aub_mem_dump/aub_header.h
  aub_mem_dump/aub_services.h
)


set (RUNTIME_SRCS_BUILT_INS
  built_ins/built_ins_storage.cpp
  built_ins/built_ins.cpp
  built_ins/built_ins.h
  built_ins/sip.cpp
  built_ins/sip.h
  built_ins/vme_dispatch_builder.h
)

set (RUNTIME_SRCS_BUILT_IN_KERNELS
  built_ins/kernels/copy_buffer_rect.igdrcl_built_in
  built_ins/kernels/copy_buffer_to_buffer.igdrcl_built_in
  built_ins/kernels/copy_buffer_to_image3d.igdrcl_built_in
  built_ins/kernels/copy_image3d_to_buffer.igdrcl_built_in
  built_ins/kernels/copy_image_to_image1d.igdrcl_built_in
  built_ins/kernels/copy_image_to_image2d.igdrcl_built_in
  built_ins/kernels/copy_image_to_image3d.igdrcl_built_in
  built_ins/kernels/fill_buffer.igdrcl_built_in
  built_ins/kernels/fill_image1d.igdrcl_built_in
  built_ins/kernels/fill_image2d.igdrcl_built_in
  built_ins/kernels/fill_image3d.igdrcl_built_in
  built_ins/kernels/vme_block_motion_estimate_intel.igdrcl_built_in
  built_ins/kernels/vme_block_advanced_motion_estimate_check_intel.igdrcl_built_in
  built_ins/kernels/vme_block_advanced_motion_estimate_bidirectional_check_intel.igdrcl_built_in
  built_ins/kernels/vebox_ve_enhance_intel.igdrcl_built_in
  built_ins/kernels/vebox_ve_dn_enhance_intel.igdrcl_built_in
  built_ins/kernels/vebox_ve_dn_di_enhance_intel.igdrcl_built_in
)

set (RUNTIME_SRCS_COMMANDS
  commands/bxml_generator_glue.h
)

set (RUNTIME_SRCS_COMMAND_QUEUE
  command_queue/cpu_data_transfer_handler.h
  command_queue/command_queue.cpp
  command_queue/command_queue.h
  command_queue/command_queue_hw.h
  command_queue/command_queue_hw.inl
  command_queue/dispatch_walker.h
  command_queue/dispatch_walker_helper.h
  command_queue/dispatch_walker_helper.inl
  command_queue/enqueue_barrier.h
  command_queue/enqueue_common.h
  command_queue/enqueue_copy_buffer.h
  command_queue/enqueue_copy_buffer_rect.h
  command_queue/enqueue_copy_buffer_to_image.h
  command_queue/enqueue_copy_image.h
  command_queue/enqueue_copy_image_to_buffer.h
  command_queue/enqueue_fill_buffer.h
  command_queue/enqueue_fill_image.h
  command_queue/enqueue_kernel.h
  command_queue/enqueue_map_buffer.h
  command_queue/enqueue_map_image.h
  command_queue/enqueue_svm.h
  command_queue/enqueue_marker.h
  command_queue/enqueue_migrate_mem_objects.h
  command_queue/enqueue_read_buffer.h
  command_queue/enqueue_read_buffer_rect.h
  command_queue/enqueue_read_image.h
  command_queue/enqueue_write_buffer.h
  command_queue/enqueue_write_buffer_rect.h
  command_queue/enqueue_write_image.h
  command_queue/finish.h
  command_queue/flush.h
  command_queue/local_id_gen.cpp
  command_queue/local_id_gen_avx2.cpp
  command_queue/local_id_gen_sse4.cpp
  command_queue/local_id_gen.h
  command_queue/local_id_gen.inl
  command_queue/local_work_size.cpp
)

set (RUNTIME_SRCS_COMMAND_STREAM
  command_stream/aub_command_stream_receiver.cpp
  command_stream/aub_command_stream_receiver.h
  command_stream/aub_command_stream_receiver_hw.h
  command_stream/aub_command_stream_receiver_hw.inl
  command_stream/command_stream_receiver.cpp
  command_stream/command_stream_receiver.h
  command_stream/command_stream_receiver_hw.h
  command_stream/command_stream_receiver_hw.inl
  command_stream/csr_definitions.h
  command_stream/device_command_stream.h
  command_stream/linear_stream.cpp
  command_stream/linear_stream.h
  command_stream/submissions_aggregator.cpp
  command_stream/submissions_aggregator.h
  command_stream/tbx_command_stream_receiver.cpp
  command_stream/tbx_command_stream_receiver.h
  command_stream/tbx_command_stream_receiver_hw.h
  command_stream/tbx_command_stream_receiver_hw.inl
  command_stream/tbx_stream.cpp
  command_stream/thread_arbitration_policy.h
  command_stream/preemption.h
  command_stream/preemption.cpp
  command_stream/preemption.inl
)

set (RUNTIME_SRCS_COMPILER_INTERFACE
  compiler_interface/binary_cache.cpp
  compiler_interface/compiler_interface.cpp
  compiler_interface/compiler_interface.h
  compiler_interface/compiler_interface.inl
  compiler_interface/create_main.cpp
)

set (RUNTIME_SRCS_CONTEXT
  context/context.cpp
  context/context.h
  context/context.inl
  context/driver_diagnostics.cpp
  context/driver_diagnostics.h
)

set (RUNTIME_SRCS_DEVICE
  device/device.cpp
  device/device.h
  device/device_caps.cpp
  device/device_info.cpp
  device/device_info.h
  device/device_info_map.h
  device/device_vector.h
)

set (RUNTIME_SRCS_DEVICE_QUEUE
  device_queue/device_queue.cpp
  device_queue/device_queue.h
  device_queue/device_queue_hw.h
  device_queue/device_queue_hw.inl
  device_queue/device_queue_hw_profiling.inl
)

set (RUNTIME_SRCS_EVENT
  event/async_events_handler.h
  event/async_events_handler.cpp
  event/event.cpp
  event/event.h
  event/event_builder.cpp
  event/event_builder.h
  event/user_event.cpp
  event/user_event.h
  event/hw_timestamps.h
  event/perf_counter.h
)

set (RUNTIME_SRCS_EXECUTION_MODEL
  execution_model/device_enqueue.h
)

if(GTPIN_HEADERS_DIR)
  set (RUNTIME_SRCS_GTPIN
    gtpin/gtpin_callbacks.cpp
    gtpin/gtpin_helpers.cpp
    gtpin/gtpin_helpers.h
    gtpin/gtpin_hw_helper.cpp
    gtpin/gtpin_hw_helper.h
    gtpin/gtpin_init.cpp
    gtpin/gtpin_init.h
    gtpin/gtpin_notify.h
  )
else(GTPIN_HEADERS_DIR)
  set (RUNTIME_SRCS_GTPIN
    gtpin/gtpin_callback_stubs.cpp
    gtpin/gtpin_notify.h
  )
endif(GTPIN_HEADERS_DIR)

set (RUNTIME_SRCS_HELPERS
  helpers/abort.h
  helpers/aligned_memory.h
  helpers/array_count.h
  helpers/base_object.cpp
  helpers/base_object.h
  helpers/base_object_allocator.cpp
  helpers/basic_math.h
  helpers/cache_policy.cpp
  helpers/cache_policy.h
  helpers/dirty_state_helpers.h
  helpers/dirty_state_helpers.cpp
  helpers/dispatch_info.h
  helpers/dispatch_info.cpp
  helpers/dispatch_info_builder.h
  helpers/completion_stamp.h
  helpers/debug_helpers.h
  helpers/engine_node.h
  helpers/error_mappers.h
  helpers/file_io.cpp
  helpers/file_io.h
  helpers/flush_stamp.h
  helpers/flush_stamp.cpp
  helpers/get_info.h
  helpers/hash.h
  helpers/hw_helper.cpp
  helpers/hw_helper.h
  helpers/hw_helper.inl
  helpers/hw_info.cpp
  helpers/hw_info.h
  helpers/kernel_commands.h
  helpers/kernel_commands.inl
  helpers/options.cpp
  helpers/options.h
  helpers/per_thread_data.cpp
  helpers/per_thread_data.h
  helpers/preamble.h
  helpers/preamble.inl
  helpers/ptr_math.h
  helpers/queue_helpers.h
  helpers/sampler_helpers.h
  helpers/selectors.h
  helpers/state_base_address.h
  helpers/state_base_address.inl
  helpers/stdio.h
  helpers/string.h
  helpers/string_helpers.h
  helpers/surface_formats.cpp
  helpers/surface_formats.h
  helpers/task_information.cpp
  helpers/task_information.h
  helpers/uint16_avx2.h
  helpers/uint16_sse4.h
  helpers/wddm_helper.h
  helpers/validators.cpp
  helpers/validators.h
)

if (WIN32)
  list (APPEND RUNTIME_SRCS_HELPERS
    helpers/translationtable_callbacks.h
  )
endif(WIN32)

set (RUNTIME_SRCS_INDIRECT_HEAP
  indirect_heap/indirect_heap.cpp
  indirect_heap/indirect_heap.h
)

set (RUNTIME_SRCS_INSTRUMENTATION
  instrumentation${BRANCH_DIR_SUFFIX}/instrumentation.cpp
  instrumentation${BRANCH_DIR_SUFFIX}/instrumentation.h
)

set (RUNTIME_SRCS_KERNEL
  kernel/dynamic_kernel_info.h
  kernel/kernel.cpp
  kernel/kernel.h
  kernel/kernel.inl
)

set (RUNTIME_SRCS_MEMORY_MANAGER
  memory_manager/deferrable_deletion.h
  memory_manager/deferred_deleter.cpp
  memory_manager/deferred_deleter.h
  memory_manager/graphics_allocation.h
  memory_manager/graphics_allocation.cpp
  memory_manager/host_ptr_defines.h
  memory_manager/host_ptr_manager.h
  memory_manager/host_ptr_manager.cpp
  memory_manager/memory_manager.cpp
  memory_manager/memory_manager.h
  memory_manager/svm_memory_manager.cpp
  memory_manager/svm_memory_manager.h
  memory_manager/os_agnostic_memory_manager.cpp
  memory_manager/os_agnostic_memory_manager.h
  memory_manager/page_table.cpp
  memory_manager/page_table.h
  memory_manager/address_mapper.cpp
  memory_manager/address_mapper.h
  memory_manager/surface.h
)

set (RUNTIME_SRCS_GMM_HELPER
  gmm_helper/gmm_helper.cpp
  gmm_helper/gmm_helper.h
  gmm_helper/gmm_lib.h
  gmm_helper/resource_info.h
  gmm_helper${BRANCH_DIR_SUFFIX}/gmm_utils.cpp
 )

if (WIN32)
  list (APPEND RUNTIME_SRCS_GMM_HELPER
    gmm_helper/page_table_mngr.h
    gmm_helper/gmm_memory.h
  )
endif(WIN32)

set (RUNTIME_SRCS_MEM_OBJ
  mem_obj/buffer.cpp
  mem_obj/buffer.h
  mem_obj/buffer.inl
  mem_obj/image.cpp
  mem_obj/image.h
  mem_obj/image.inl
  mem_obj/mem_obj.cpp
  mem_obj/mem_obj.h
  mem_obj/buffer_factory_init.inl
  mem_obj/image_factory_init.inl
  mem_obj/pipe.h
  mem_obj/pipe.cpp
)

set (RUNTIME_SRCS_OS_INTERFACE
  os_interface/32bit_memory.h
  os_interface/os_library.h
  os_interface/linux/linux_inc.h
  os_interface/windows/windows_inc.h
  os_interface/device_factory.h
  os_interface/os_inc.h
  os_interface/os_interface.h
  os_interface/os_time.h
  os_interface/os_time.cpp
  os_interface/debug_settings_manager.cpp
  os_interface/debug_settings_manager.h
  os_interface/performance_counters.cpp
  os_interface/performance_counters.h
  os_interface/print.h
)

set (RUNTIME_SRCS_PLATFORM
  platform/platform.cpp
  platform/platform.h
  platform/platform_info.h
)

set (RUNTIME_SRCS_PROGRAM
  program/block_kernel_manager.cpp
  program/block_kernel_manager.h
  program/build.cpp
  program/compile.cpp
  program/create.cpp
  program/get_info.cpp
  program/heap_info.h
  program/kernel_arg_info.h
  program/kernel_info.cpp
  program/kernel_info.h
  program/link.cpp
  program/patch_info.h
  program/process_elf_binary.cpp
  program/process_spir_binary.cpp
  program/process_gen_binary.cpp
  program/program.cpp
  program/program.h
  program/printf_handler.h
  program/printf_handler.cpp
  program/print_formatter.h
  program/print_formatter.cpp
)

set (RUNTIME_SRCS_SAMPLER
  sampler/sampler.cpp
  sampler/sampler.h
  sampler/sampler.inl
  sampler/sampler_factory_init.inl
)

list (APPEND RUNTIME_SRCS_SCHEDULER
  scheduler/scheduler_kernel.cpp
  scheduler/scheduler_kernel.h
  scheduler/CMakeLists.txt
)

set (RUNTIME_SRCS_SKU_INFO
  sku_info/sku_info_base.h
  sku_info/operations/sku_info_transfer.h
  sku_info/definitions${BRANCH_DIR_SUFFIX}/sku_info.h
  sku_info/operations${BRANCH_DIR_SUFFIX}/sku_info_transfer.cpp
)

if (WIN32)
  list (APPEND RUNTIME_SRCS_SKU_INFO
      sku_info/operations/sku_info_receiver.h
      sku_info/operations${BRANCH_DIR_SUFFIX}/sku_info_receiver.cpp
  )
endif(WIN32)

set (RUNTIME_SRCS_SHARINGS
  sharings/sharing.h
  sharings/sharing.cpp
)

set (RUNTIME_SRCS_TBX
  tbx/tbx_proto.h
  tbx/tbx_sockets.cpp
  tbx/tbx_sockets.h
)

set (RUNTIME_SRCS_UTILITIES
  utilities/api_intercept.h
  utilities/arrayref.h
  utilities/cpu_info.h
  utilities/debug_file_reader.cpp
  utilities/debug_file_reader.h
  utilities/debug_settings_reader.cpp
  utilities/debug_settings_reader.h
  utilities/directory.h
  utilities/heap_allocator.cpp
  utilities/heap_allocator.h
  utilities/iflist.h
  utilities/idlist.h
  utilities/stackvec.h
  utilities/perf_profiler.cpp
  utilities/perf_profiler.h
  utilities/reference_tracked_object.h
  utilities/tag_allocator.h
  utilities/timer_util.h
  utilities/vec.h
)

set (RUNTIME_SRCS_GEN_COMMON
  gen_common/aub_mapper.h
  gen_common/aub_mapper_base.h
  gen_common/hw_cmds.h
  gen_common/reg_configs.h
)

if (WIN32)
  list (APPEND RUNTIME_SRCS_UTILITIES
    utilities/windows/directory.cpp
    utilities/windows/timer_util.cpp
    utilities/windows/cpu_info.cpp
  )
else(WIN32)
  list (APPEND RUNTIME_SRCS_UTILITIES
    utilities/linux/directory.cpp
    utilities/linux/timer_util.cpp
    utilities/linux/cpu_info.cpp
  )
endif (WIN32)

if (WIN32)
  list (APPEND RUNTIME_SRCS_OS_INTERFACE
    os_interface/windows/api.cpp
    os_interface/windows/d3d_sharing_functions.h
    os_interface/windows/d3d9_sharing_functions.cpp
    os_interface/windows/d3d10_11_sharing_functions.cpp
    os_interface/windows/debug_registry_reader.cpp
    os_interface/windows/deferrable_deletion_win.cpp
    os_interface/windows/deferrable_deletion_win.h
    os_interface/windows/device_command_stream.inl
    os_interface/windows/device_factory.cpp
    os_interface/windows/gdi_interface.cpp
    os_interface/windows/gdi_interface.h
    os_interface/windows/options.cpp
    os_interface/windows/os_interface.cpp
    os_interface/windows/os_interface.h
    os_interface/windows/os_library.cpp
    os_interface/windows/os_library.h
    os_interface/windows/os_time.cpp
    os_interface/windows/os_time.h
    os_interface/windows/registry_reader.h
    os_interface/windows/thk_wrapper.h
    os_interface/windows/wddm.cpp
    os_interface/windows/wddm.h
    os_interface/windows/wddm.inl
    os_interface/windows/wddm_32bit_memory.cpp
    os_interface/windows/wddm_allocation.h
    os_interface/windows/wddm_device_command_stream.inl
    os_interface/windows/wddm_device_command_stream.h
    os_interface/windows/wddm_engine_mapper.h
    os_interface/windows/wddm_memory_manager.cpp
    os_interface/windows/wddm_memory_manager.h
    os_interface/windows/windows_inc.cpp
    os_interface/windows/windows_wrapper.h
    os_interface/windows/performance_counters_win.cpp
    os_interface/windows/performance_counters_win.h
    os_interface/windows/print.cpp
    os_interface/windows/driver_info.h
    os_interface/windows/driver_info.cpp
  )

  if ("${IGDRCL_OPTION__BITS}" STREQUAL "32" )
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
  endif ("${IGDRCL_OPTION__BITS}" STREQUAL "32")
endif (WIN32)

if (UNIX)
  list (APPEND RUNTIME_SRCS_OS_INTERFACE
    os_interface/linux/api.cpp
    os_interface/linux/d3d_sharing_functions.h
    os_interface/linux/debug_env_reader.cpp
    os_interface/linux/device_command_stream.inl
    os_interface/linux/device_factory.cpp
    os_interface/linux/drm_32bit_memory.cpp
    os_interface/linux/drm_allocation.h
    os_interface/linux/drm_buffer_object.cpp
    os_interface/linux/drm_buffer_object.h
    os_interface/linux/drm_command_stream.inl
    os_interface/linux/drm_command_stream.h
    os_interface/linux/drm_engine_mapper.h
    os_interface/linux/drm_null_device.h
    os_interface/linux/drm_gem_close_worker.cpp
    os_interface/linux/drm_gem_close_worker.h
    os_interface/linux/drm_memory_manager.cpp
    os_interface/linux/drm_memory_manager.h
    os_interface/linux/drm_neo.cpp
    os_interface/linux/drm_neo.h
    os_interface/linux/drm_neo_create.cpp
    os_interface/linux/hw_info_config.cpp
    os_interface/linux/hw_info_config.h
    os_interface/linux/linux_inc.cpp
    os_interface/linux/options.cpp
    os_interface/linux/os_interface.cpp
    os_interface/linux/os_interface.h
    os_interface/linux/os_library.cpp
    os_interface/linux/os_library.h
    os_interface/linux/os_time.cpp
    os_interface/linux/os_time.h
    os_interface/linux/performance_counters_linux.cpp
    os_interface/linux/performance_counters_linux.h
    os_interface/linux/print.cpp
    os_interface/linux/driver_info.cpp
  )
endif (UNIX)

add_subdirectory("sharings")

set (RUNTIME_SRCS
  ${RUNTIME_SRCS_API}
  ${RUNTIME_SRCS_ACCELERATORS}
  ${RUNTIME_SRCS_AUB_MEM_DUMP}
  ${RUNTIME_SRCS_BUILT_INS}
  ${RUNTIME_SRCS_BUILT_IN_KERNELS}
  ${RUNTIME_SRCS_COMMANDS}
  ${RUNTIME_SRCS_COMMAND_QUEUE}
  ${RUNTIME_SRCS_COMMAND_STREAM}
  ${RUNTIME_SRCS_COMPILER_INTERFACE}
  ${RUNTIME_SRCS_CONTEXT}
  ${RUNTIME_SRCS_DEVICE}
  ${RUNTIME_SRCS_DEVICE_QUEUE}
  ${RUNTIME_SRCS_EVENT}
  ${RUNTIME_SRCS_EXECUTION_MODEL}
  ${RUNTIME_SRCS_GEN_COMMON}
  ${RUNTIME_SRCS_GTPIN}
  ${RUNTIME_SRCS_HELPERS}
  ${RUNTIME_SRCS_INDIRECT_HEAP}
  ${RUNTIME_SRCS_INSTRUMENTATION}
  ${RUNTIME_SRCS_KERNEL}
  ${RUNTIME_SRCS_MEMORY_MANAGER}
  ${RUNTIME_SRCS_GMM_HELPER}
  ${RUNTIME_SRCS_MEM_OBJ}
  ${RUNTIME_SRCS_OS_INTERFACE}
  ${RUNTIME_SRCS_PLATFORM}
  ${RUNTIME_SRCS_PROGRAM}
  ${RUNTIME_SRCS_SAMPLER}
  ${RUNTIME_SRCS_SCHEDULER}
  ${RUNTIME_SRCS_SKU_INFO}
  ${RUNTIME_SRCS_SHARINGS}
  ${RUNTIME_SRCS_TBX}
  ${RUNTIME_SRCS_UTILITIES}
  CMakeLists.txt
)

# Enable SSE4/AVX2 options for files that need them
if(MSVC)
	set_source_files_properties(command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
else()
	set_source_files_properties(command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
	set_source_files_properties(command_queue/local_id_gen_sse4.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
endif (MSVC)

# Put Driver version into define
if(NEO_DRIVER_VERSION)
	set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/device/device_caps.cpp PROPERTIES COMPILE_DEFINITIONS NEO_DRIVER_VERSION="${NEO_DRIVER_VERSION}")
endif(NEO_DRIVER_VERSION)

list (APPEND HW_SRC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/gen_common)

# Include/enable each GEN
# Reverse order so that GEN N+1 includes GEN N
foreach(GEN_NUM RANGE ${MAX_GEN} 0 -1)
  GEN_CONTAINS_PLATFORMS("SUPPORTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    # Add GEN-specific files
    set(RUNTIME_SRCS_GEN${GEN_NUM} ${RUNTIME_SRCS_GEN${GEN_NUM}_SPECIFIC})

    # Add default GEN files
    foreach(SRC_IT ${RUNTIME_SRCS_GENX})
      list (APPEND RUNTIME_SRCS_GEN${GEN_NUM} gen${GEN_NUM}/${SRC_IT})
    endforeach(SRC_IT)

    # Get all supported platforms for this GEN
    GET_PLATFORMS_FOR_GEN("SUPPORTED" ${GEN_NUM} SUPPORTED_GENX_PLATFORMS)

    # Add platform-specific files
    foreach(PLATFORM_IT ${SUPPORTED_GENX_PLATFORMS})
      string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
      list(APPEND RUNTIME_SRCS_GEN${GEN_NUM} gen${GEN_NUM}/hw_cmds_${PLATFORM_IT_LOWER}.h)
      list(APPEND RUNTIME_SRCS_GEN${GEN_NUM} gen${GEN_NUM}/hw_info_${PLATFORM_IT_LOWER}.cpp)
      list(APPEND RUNTIME_SRCS_GEN${GEN_NUM} ${RUNTIME_SRCS_GEN${GEN_NUM}_${PLATFORM_IT}})
      if(UNIX)
        list(APPEND RUNTIME_SRCS_GEN${GEN_NUM} gen${GEN_NUM}/linux/hw_info_config_${PLATFORM_IT_LOWER}.cpp)
      endif(UNIX)

      # Enable platform
      list(APPEND GEN${GEN_NUM}_SRC_LINK gen${GEN_NUM}/enable_${PLATFORM_IT_LOWER}.cpp)
      if(UNIX)
        list(APPEND GEN${GEN_NUM}_SRC_LINK gen${GEN_NUM}/linux/enable_${PLATFORM_IT_LOWER}.cpp)
      endif(UNIX)
    endforeach(PLATFORM_IT)

    list(APPEND GEN${GEN_NUM}_SRC_LINK gen${GEN_NUM}/enable_family_full.cpp)

    # Append this GEN's sources to the list of all sources
    foreach(SRC_IT ${RUNTIME_SRCS_GEN${GEN_NUM}})
      if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SRC_IT}")
        list(APPEND RUNTIME_SRCS ${SRC_IT})
      endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SRC_IT}")
    endforeach(SRC_IT)

    # Set-up gen sources for the dll
    list(APPEND HW_SRC_LINK ${GEN${GEN_NUM}_SRC_LINK})

    source_group("gen${GEN_NUM}" FILES ${RUNTIME_SRCS_GEN${GEN_NUM}} ${GEN${GEN_NUM}_SRC_LINK} )
  endif(${GENX_HAS_PLATFORMS})
endforeach(GEN_NUM)

add_library(${NEO_STATIC_LIB_NAME} STATIC $<TARGET_OBJECTS:${BIKSIM_LIB_NAME}>
  ${RUNTIME_SRCS}
)

target_link_libraries(${NEO_STATIC_LIB_NAME} elflib)

target_include_directories(${NEO_STATIC_LIB_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${GMM_INCLUDE_PATHS}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
)

target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
	${KHRONOS_HEADERS_DIR}
	${IGDRCL__IGC_INCLUDE_DIR}
	${THIRD_PARTY_DIR}
)

if(GTPIN_HEADERS_DIR)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
	${GTPIN_HEADERS_DIR}
  )
endif(GTPIN_HEADERS_DIR)

if (WIN32)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    ${WDK_INCLUDE_PATHS}
    os_interface/windows
  )
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC OGL=1)
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC INSTR_WIN_UMD=1)

endif (WIN32)

if (UNIX)
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC OGL_GEM=1)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    os_interface/linux
    "${LIBDRM_DIR}/include"
  )
endif (UNIX)

#cl_khr_priority support
if(NOT MSVC)
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC -DSUPPORT_PRIORITY_HINTS)
  message(STATUS "Supporting priority hints")
endif()

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM})

link_directories(${GMM_LIB_PATHS})

if(NOT GMMUMD_LIB_NAME)
  set(GMMUMD_LIB_NAME "gmm_umd" CACHE STRING "name of gmm static library")
endif()
target_link_libraries(${NEO_STATIC_LIB_NAME} ${GMMUMD_LIB_NAME})

if(HAVE_INSTRUMENTATION)
target_link_libraries(${NEO_STATIC_LIB_NAME} instrumentation_umd)
endif()

if(WIN32)
  if(GTPIN_HEADERS_DIR)
    set( DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/GTPinExports${IGDRCL_OPTION__BITS}.def" )
  else(GTPIN_HEADERS_DIR)
    set( DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/OpenCLExports${IGDRCL_OPTION__BITS}.def" )
  endif(GTPIN_HEADERS_DIR)
endif(WIN32)

list(APPEND LIB_FLAGS_DEFINITIONS -DCIF_HEADERS_ONLY_BUILD ${SUPPORTED_GEN_FLAGS_DEFINITONS})

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC ${LIB_FLAGS_DEFINITIONS})
if(IGC_OCL_ADAPTOR_DIR) # IGC/AdaptorOCL
  target_include_directories("${NEO_STATIC_LIB_NAME}" PUBLIC "${IGC_OCL_ADAPTOR_DIR}")
endif(IGC_OCL_ADAPTOR_DIR)

if(CIF_BASE_DIR)
  target_include_directories("${NEO_STATIC_LIB_NAME}" PUBLIC "${CIF_BASE_DIR}")
endif(CIF_BASE_DIR)

set(IGDRCL_LIB_FLAGS_DEFINITIONS ${LIB_FLAGS_DEFINITIONS} PARENT_SCOPE)
set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_property(TARGET ${NEO_STATIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})

set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")

target_include_directories(${NEO_STATIC_LIB_NAME} BEFORE PRIVATE ${HW_SRC_INCLUDES})

if(${GENERATE_EXECUTABLE})
	set (RUNTIME_SRCS_DLL
	  dll/options.cpp
	  dll/create_command_stream.cpp
	  dll/create_deferred_deleter.cpp
	  helpers/abort.cpp
	  helpers/debug_helpers.cpp
	  gmm_helper/resource_info.cpp
	  program/evaluate_unhandled_token.cpp
	  "${DEF_FILE}"
	)

	list (APPEND RUNTIME_SRCS_DLL ${HW_SRC_LINK})

	if (UNIX)
		list (APPEND RUNTIME_SRCS_DLL dll/linux/drm_neo_create.cpp)
	endif (UNIX)

	if (WIN32)
	  list (APPEND RUNTIME_SRCS_DLL os_interface/windows/wddm_create.cpp)
	  list (APPEND RUNTIME_SRCS_DLL gmm_helper/page_table_mngr.cpp)
	  list (APPEND RUNTIME_SRCS_DLL gmm_helper/gmm_memory.cpp)
	endif (WIN32)

	list (APPEND RUNTIME_SRCS_DLL api/api.cpp)

	if(GTPIN_HEADERS_DIR)
		list (APPEND RUNTIME_SRCS_DLL gtpin/gtpin_init.cpp)
		foreach(GEN_NUM RANGE ${MAX_GEN} 0 -1)
		  GEN_CONTAINS_PLATFORMS("SUPPORTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
		  if(${GENX_HAS_PLATFORMS})
			list(APPEND RUNTIME_SRCS_GTPIN gen${GEN_NUM}/gtpin_setup_gen${GEN_NUM}.cpp)
		  endif(${GENX_HAS_PLATFORMS})
		endforeach(GEN_NUM)
	endif(GTPIN_HEADERS_DIR)

	add_library(${NEO_DYNAMIC_LIB_NAME} SHARED
	  ${RUNTIME_SRCS_DLL}
	  $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
	  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
	  $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
	  $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
	)

	target_include_directories(${NEO_DYNAMIC_LIB_NAME} BEFORE PRIVATE
	  ${CMAKE_CURRENT_BINARY_DIR}
	  ${HW_SRC_INCLUDES}
	)

	target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${NEO_STATIC_LIB_NAME})

	if (WIN32)
	  target_include_directories(${NEO_DYNAMIC_LIB_NAME} PUBLIC
		${WDK_INCLUDE_PATHS}
		${GMM_INCLUDE_PATHS}
		${UMKM_SHAREDDATA_INCLUDE_PATHS}
		${INSTRUMENTATION_INCLUDE_PATH}
	  )
	  target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${NEO_STATIC_LIB_NAME} dxgi Ws2_32.lib)
	else(WIN32)
	  target_include_directories(${NEO_DYNAMIC_LIB_NAME} PUBLIC
		${GMM_INCLUDE_PATHS}
		${UMKM_SHAREDDATA_INCLUDE_PATHS}
		${INSTRUMENTATION_INCLUDE_PATH}
	  )
	endif (WIN32)

	if (UNIX)
	  target_link_libraries(${NEO_DYNAMIC_LIB_NAME} dl pthread)
	  set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
		APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/ocl.exports"
	  )
	endif (UNIX)

	set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES
	  DEBUG_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
	  RELEASE_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
	  RELEASE-INTERNAL_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
	  OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
	)

	set_property(TARGET ${NEO_DYNAMIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
	set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")
endif(${GENERATE_EXECUTABLE})

if (WIN32)
  source_group("source files" FILES ${IGDRCL_SRCS_DLL})
  source_group("source files\\api" FILES ${RUNTIME_SRCS_API})
  source_group("source files\\accelerators" FILES ${RUNTIME_SRCS_ACCELERATORS})
  source_group("source files\\aub_mem_dump" FILES ${RUNTIME_SRCS_AUB_MEM_DUMP})
  source_group("source files\\built_ins" FILES ${RUNTIME_SRCS_BUILT_INS})
  source_group("source files\\built_ins\\kernels" FILES ${RUNTIME_SRCS_BUILT_IN_KERNELS})
  source_group("source files\\commands" FILES ${RUNTIME_SRCS_COMMANDS})
  source_group("source files\\command_queue" FILES ${RUNTIME_SRCS_COMMAND_QUEUE})
  source_group("source files\\command_stream" FILES ${RUNTIME_SRCS_COMMAND_STREAM})
  source_group("source files\\compiler_interface" FILES ${RUNTIME_SRCS_COMPILER_INTERFACE})
  source_group("source files\\context" FILES ${RUNTIME_SRCS_CONTEXT})
  source_group("source files\\device" FILES ${RUNTIME_SRCS_DEVICE})
  source_group("source files\\device_queue" FILES ${RUNTIME_SRCS_DEVICE_QUEUE})
  source_group("source files\\event" FILES ${RUNTIME_SRCS_EVENT})
  source_group("source files\\execution_model" FILES ${RUNTIME_SRCS_EXECUTION_MODEL})
  source_group("source files\\gen_common" FILES ${RUNTIME_SRCS_GEN_COMMON})
  source_group("source files\\helpers" FILES ${RUNTIME_SRCS_HELPERS})
  source_group("source files\\indirect_heap" FILES ${RUNTIME_SRCS_INDIRECT_HEAP})
  source_group("source files\\instrumentation" FILES ${RUNTIME_SRCS_INSTRUMENTATION})
  source_group("source files\\kernel" FILES ${RUNTIME_SRCS_KERNEL})
  source_group("source files\\memory_manager" FILES ${RUNTIME_SRCS_MEMORY_MANAGER})
  source_group("source files\\gmm_helper" FILES ${RUNTIME_SRCS_GMM_HELPER})
  if(GTPIN_HEADERS_DIR)
    source_group("source files\\gtpin" FILES ${RUNTIME_SRCS_GTPIN})
  endif(GTPIN_HEADERS_DIR)
  source_group("source files\\mem_obj" FILES ${RUNTIME_SRCS_MEM_OBJ})
  source_group("source files\\os_interface" FILES ${RUNTIME_SRCS_OS_INTERFACE})
  source_group("source files\\platform" FILES ${RUNTIME_SRCS_PLATFORM})
  source_group("source files\\program" FILES ${RUNTIME_SRCS_PROGRAM})
  source_group("source files\\sampler" FILES ${RUNTIME_SRCS_SAMPLER})
  source_group("source files\\scheduler" FILES ${RUNTIME_SRCS_SCHEDULER})
  source_group("source files\\sku_info" FILES ${RUNTIME_SRCS_SKU_INFO})
  source_group("source files\\sharings" FILES ${RUNTIME_SRCS_SHARINGS})
  source_group("source files\\tbx" FILES ${RUNTIME_SRCS_TBX})
  source_group("source files\\utilities" FILES ${RUNTIME_SRCS_UTILITIES})
endif (WIN32)

if (UNIX)
  if(NOT (TARGET clang-tidy))
    add_custom_target(clang-tidy
      DEPENDS scheduler
    )

    add_custom_command(
      TARGET clang-tidy
      POST_BUILD
      COMMAND echo clang-tidy...
      COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name *.cpp -print0 | xargs -0 -I{} -P`nproc` clang-tidy -p ${IGDRCL_BINARY_DIR} {} | tee ${IGDRCL_BINARY_DIR}/clang-tidy.log
      WORKING_DIRECTORY ${IGDRCL_SOURCE_DIR}
    )
  endif(NOT (TARGET clang-tidy))
endif(UNIX)

