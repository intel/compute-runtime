#
# Copyright (C) 2018-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(builtins_vme_sources)
set_target_properties(builtins_vme_sources PROPERTIES FOLDER "${OPENCL_RUNTIME_PROJECTS_FOLDER}/${OPENCL_BUILTINS_PROJECTS_FOLDER}")
set(BUILTINS_OUTDIR_WITH_ARCH "${TargetDir}/built_ins/${NEO_ARCH}")
add_dependencies(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} builtins_vme_sources)
add_subdirectories()
