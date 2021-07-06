#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(macro_for_each_gen)
  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    foreach(BRANCH ${BRANCH_DIR_LIST})
      set(SRC_FILE ${NEO_SHARED_TEST_DIRECTORY}/common/${BRANCH}${GEN_TYPE_LOWER}/cmd_parse_${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND igdrcl_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_TEST_DIRECTORY}/common/libult/${BRANCH}${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND igdrcl_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      string(REGEX REPLACE "/$" "" _BRANCH_FILENAME_SUFFIX "${BRANCH_DIR}")
      string(REGEX REPLACE "^/" "_" _BRANCH_FILENAME_SUFFIX "${_BRANCH_FILENAME_SUFFIX}")
      foreach(SRC_IT "enable_hw_info_config_" "enable_" "enable_family_full_core_")
        set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH}${GEN_TYPE_LOWER}${BRANCH_DIR}${SRC_IT}${GEN_TYPE_LOWER}${_BRANCH_FILENAME_SUFFIX}.cpp)
        if(EXISTS ${SRC_FILE})
          list(APPEND igdrcl_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
        endif()
        set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH_DIR}${GEN_TYPE_LOWER}/${SRC_IT}${GEN_TYPE_LOWER}.cpp)
        if(EXISTS ${SRC_FILE})
          list(APPEND igdrcl_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
        endif()
      endforeach()
    endforeach()
  endforeach()
endmacro()

apply_macro_for_each_gen("TESTED")

set(igdrcl_libult_common_SRCS_LIB_ULT
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/device_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/device_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/mock_execution_environment_gmm_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/mock_execution_environment_gmm_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_leak_listener.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_leak_listener.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_management.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_management.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/sip_init.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/test_files.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/test_files.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/create_tbx_sockets.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/source_level_debugger_library.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/source_level_debugger_library.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/source_level_debugger_ult.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_excludes.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_excludes.h
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/base_ult_config_listener.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/base_ult_config_listener.h
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/helpers/debug_helpers.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/utilities/cpuintrinsics.cpp
)

get_property(NEO_SHARED_TESTS_CMD_PARSE GLOBAL PROPERTY NEO_SHARED_TESTS_CMD_PARSE)

list(APPEND igdrcl_libult_common_SRCS_LIB_ULT
     ${NEO_SHARED_TESTS_CMD_PARSE}
)

add_library(igdrcl_libult_common OBJECT EXCLUDE_FROM_ALL
            ${igdrcl_libult_common_SRCS_LIB_ULT}
            ${igdrcl_libult_common_SRCS_ENABLE_TESTED_HW}
)

target_include_directories(igdrcl_libult_common PRIVATE
                           $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
                           ${SOURCE_LEVEL_DEBUGGER_HEADERS_DIR}
)

set_target_properties(igdrcl_libult_common PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(igdrcl_libult_common PROPERTIES FOLDER ${SHARED_TEST_PROJECTS_FOLDER})
set_property(TARGET igdrcl_libult_common APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
target_include_directories(igdrcl_libult_common PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(igdrcl_libult_common PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)

create_project_source_tree(igdrcl_libult_common)
add_subdirectories()
