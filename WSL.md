<!---

Copyright (C) 2021 Intel Corporation

SPDX-License-Identifier: MIT

-->

# WDDM GPU Paravirtualization support for WSL2

## Introduction

This document describes ingredients required in order to use Intel&reg; Graphics Compute Runtime for oneAPI Level Zero and OpenCL&trade; Driver
in WSL2 environment.

## Host OS

Windows 11 Insider Preview Build [22000.71](https://blogs.windows.com/windows-insider/2021/07/15/announcing-windows-11-insider-preview-build-22000-71/)

## Host Graphics Driver

Required driver package is available [here](https://downloadcenter.intel.com/download/30579/Intel-Graphics-Windows-DCH-Drivers)

## Guest (WSL2) Intel&reg; Graphics Compute Runtime for oneAPI Level Zero and OpenCL&trade; Driver

Support was enabled at https://github.com/intel/compute-runtime/commit/fad4ee7e246839c36c3f6b0e14ea0c79d9e4758a
and it is included in [21.30.20482](https://github.com/intel/compute-runtime/releases/tag/21.30.20482)
