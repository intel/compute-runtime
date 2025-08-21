# spec file for package intel-level-zero-gpu

#it's changed by external script
%global ver xxx
%global rel xxx
%global build_id xxx
%global NEO_RELEASE_WITH_REGKEYS FALSE
%global NEO_ENABLE_XE_EU_DEBUG_SUPPORT FALSE
%global NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM FALSE
%global NEO_ENABLE_I915_PRELIM_DETECTION FALSE
%global NEO_ENABLE_XE_PRELIM_DETECTION FALSE
%global NEO_I915_PRELIM_HEADERS_DIR %{nil}

%define gmmlib_sover 12
%define igc_sover 2

%if !0%{?build_type:1}
%define build_type Release
%endif

%define _source_payload w5T16.xzdio
%define _binary_payload w5T16.xzdio

Name: intel-level-zero-gpu
Version: %{ver}
Release: %{rel}%{?dist}
Summary: Intel(R) GPU Driver for oneAPI Level Zero.

Group: System Environment/Libraries
License: MIT
URL: https://github.com/intel/compute-runtime
Source0: %{url}/archive/%{version}/compute-runtime.tar.xz
Source1: copyright
%if "%{NEO_I915_PRELIM_HEADERS_DIR}" != ""
Source2: uapi.tar.xz
%endif

ExclusiveArch:  x86_64
BuildRequires:  cmake gcc-c++ ninja make
#BuildRequires:  libva-devel
BuildRequires:  libigdgmm%{?name_suffix}-devel
BuildRequires:  libigdfcl%{?name_suffix}-devel

Requires:       libigc%{igc_sover}%{?name_suffix}
Requires:       libigdfcl%{igc_sover}%{?name_suffix}
Requires:       libigdgmm%{gmmlib_sover}%{?name_suffix}

%description
Runtime library providing the ability to use Intel GPUs with the oneAPI Level
Zero programming interface.
Level Zero is the primary low-level interface for language and runtime
libraries. Level Zero offers fine-grain control over accelerators capabilities,
delivering a simplified and low-latency interface to hardware, and efficiently
exposing hardware capabilities to applications.

%package     -n intel-level-zero-gpu%{?name_suffix}-devel
Summary:        Development files for Intel(R) GPU Driver for oneAPI Level Zero.
Group:          Development/Libraries/C and C++
Requires:       intel-level-zero-gpu%{?name_suffix} = %{version}
Provides:       intel-level-zero-gpu-devel

%description     -n intel-level-zero-gpu%{?name_suffix}-devel
Intel(R) Graphics Compute Runtime for oneAPI Level Zero - development headers

%debug_package %{nil}

%prep
%if "%{NEO_I915_PRELIM_HEADERS_DIR}" == ""
%autosetup -p1 -n compute-runtime
%else
%autosetup -p1 -n compute-runtime -b 2
%endif

%build
%cmake .. \
   -GNinja ${NEO_BUILD_EXTRA_OPTS} \
   -DNEO_VERSION_BUILD=%{build_id} \
   -DCMAKE_BUILD_TYPE=%{build_type} \
   -DNEO_BUILD_WITH_OCL=FALSE \
   -DNEO_SKIP_UNIT_TESTS=TRUE \
   -DNEO_ENABLE_I915_PRELIM_DETECTION=%{NEO_ENABLE_I915_PRELIM_DETECTION} \
   -DNEO_ENABLE_XE_PRELIM_DETECTION=%{NEO_ENABLE_XE_PRELIM_DETECTION} \
   -DNEO_ENABLE_XE_EU_DEBUG_SUPPORT=%{NEO_ENABLE_XE_EU_DEBUG_SUPPORT} \
   -DNEO_USE_XE_EU_DEBUG_EXP_UPSTREAM=%{NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM} \
   -DRELEASE_WITH_REGKEYS=%{NEO_RELEASE_WITH_REGKEYS} \
   -DL0_INSTALL_UDEV_RULES=1 \
   -DUDEV_RULES_DIR=/etc/udev/rules.d/ \
   -DCMAKE_VERBOSE_MAKEFILE=FALSE \
   -DNEO_I915_PRELIM_HEADERS_DIR=$(realpath %{NEO_I915_PRELIM_HEADERS_DIR})

%if 0%{?neo_rpm__post_cmake:0}
%{neo_rpm__post_cmake_command}
%endif

%ninja_build

%install
cd build
%ninja_install

#Remove OpenCL files
rm -rvf %{buildroot}%{_libdir}/intel-opencl/
rm -rvf %{buildroot}%{_sysconfdir}/OpenCL/
rm -rvf %{buildroot}%{_bindir}/ocloc*
rm -rvf %{buildroot}%{_libdir}/libocloc*.so
rm -rvf %{buildroot}%{_includedir}/ocloc_api.h
#Remove debug files
rm -vf %{buildroot}/%{_libdir}/intel-opencl/libigdrcl*.so.debug
rm -vf %{buildroot}/%{_libdir}/libocloc*.so.debug
rm -rvf %{buildroot}/usr/lib/debug/
#insert license into package
mkdir -p %{buildroot}/usr/share/doc/intel-level-zero-gpu%{?name_suffix}/
cp -pvR %{_sourcedir}/copyright %{buildroot}/usr/share/doc/intel-level-zero-gpu%{?name_suffix}/.

if [ -d "${NEO_NINJA_DUMP}" ]; then
    cp -v .ninja* ${NEO_NINJA_DUMP}/
fi

%files -n intel-level-zero-gpu%{?name_suffix}
%defattr(-,root,root)
%{_libdir}/libze_intel_gpu*.so.*
/usr/share/doc/intel-level-zero-gpu%{?name_suffix}/copyright
%config(noreplace)

%files -n intel-level-zero-gpu%{?name_suffix}-devel
%{_includedir}/level_zero/zet_intel_gpu_debug.h
%{_includedir}/level_zero/ze_intel_gpu.h
%{_includedir}/level_zero/ze_intel_results.h
%{_includedir}/level_zero/ze_stypes.h
%{_includedir}/level_zero/driver_experimental/ze_bindless_image_exp.h
%{_includedir}/level_zero/driver_experimental/zex_api.h
%{_includedir}/level_zero/driver_experimental/zex_cmdlist.h
%{_includedir}/level_zero/driver_experimental/zex_context.h
%{_includedir}/level_zero/driver_experimental/zex_common.h
%{_includedir}/level_zero/driver_experimental/zex_driver.h
%{_includedir}/level_zero/driver_experimental/zex_event.h
%{_includedir}/level_zero/driver_experimental/zex_graph.h
%{_includedir}/level_zero/driver_experimental/zex_memory.h
%{_includedir}/level_zero/driver_experimental/zex_module.h
%{_includedir}/level_zero/zes_intel_gpu_sysman.h
%{_includedir}/level_zero/zet_intel_gpu_metric.h

%doc

%changelog
* Mon Sep 13 2021 Compute-Runtime-Automation <compute-runtime@intel.com>
- Initial spec file for SLES 15.3
