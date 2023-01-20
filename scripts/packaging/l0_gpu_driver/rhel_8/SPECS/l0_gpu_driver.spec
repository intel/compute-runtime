#it's changed by external script
%global ver xxx
%global rel xxx
%global build_id xxx
%global NEO_RELEASE_WITH_REGKEYS FALSE
%global I915_HEADERS_DIR %{nil}

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
%if "%{I915_HEADERS_DIR}" != ""
Source2: uapi.tar.xz
%endif

BuildRequires: libva-devel gcc-c++ cmake ninja-build make
BuildRequires: intel-gmmlib-devel
BuildRequires: intel-igc-opencl-devel

Requires: intel-gmmlib
Requires: intel-igc-opencl

%description
Runtime library providing the ability to use Intel GPUs with the oneAPI Level
Zero programming interface.
Level Zero is the primary low-level interface for language and runtime
libraries. Level Zero offers fine-grain control over accelerators capabilities,
delivering a simplified and low-latency interface to hardware, and efficiently
exposing hardware capabilities to applications.

%define debug_package %{nil}

%prep
%if "%{I915_HEADERS_DIR}" == ""
%autosetup -p1 -n compute-runtime
%else
%autosetup -p1 -n compute-runtime -b 2
%endif

%build
mkdir build
cd build
%cmake .. \
   -GNinja ${NEO_BUILD_EXTRA_OPTS} \
   -DNEO_VERSION_BUILD=%{build_id} \
   -DCMAKE_BUILD_TYPE=Release \
   -DNEO_BUILD_WITH_OCL=FALSE \
   -DNEO_SKIP_UNIT_TESTS=TRUE \
   -DNEO_ENABLE_i915_PRELIM_DETECTION=TRUE \
   -DNEO_ENABLE_XE_DRM_DETECTION=TRUE \
   -DRELEASE_WITH_REGKEYS=%{NEO_RELEASE_WITH_REGKEYS} \
   -DL0_INSTALL_UDEV_RULES=1 \
   -DUDEV_RULES_DIR=/etc/udev/rules.d/ \
   -DCMAKE_VERBOSE_MAKEFILE=FALSE \
   -DI915_HEADERS_DIR=$(realpath %{I915_HEADERS_DIR})
%ninja_build

%install
cd build
%ninja_install

#Remove OpenCL files
rm -rvf %{buildroot}%{_libdir}/intel-opencl/
rm -rvf %{buildroot}%{_sysconfdir}/OpenCL/
rm -rvf %{buildroot}%{_bindir}/ocloc
rm -rvf %{buildroot}%{_libdir}/libocloc.so
rm -rvf %{buildroot}%{_includedir}/ocloc_api.h
#Remove debug files
rm -vf %{buildroot}/%{_libdir}/intel-opencl/libigdrcl.so.debug
rm -vf %{buildroot}/%{_libdir}/libocloc.so.debug
rm -rvf %{buildroot}/usr/lib/debug/
#insert license into package
mkdir -p %{buildroot}/usr/share/doc/intel-level-zero-gpu/
cp -pvR %{_sourcedir}/copyright %{buildroot}/usr/share/doc/intel-level-zero-gpu/.

%files
%defattr(-,root,root)
%{_libdir}/libze_intel_gpu.so.*
/usr/share/doc/intel-level-zero-gpu/copyright
%config(noreplace)

%doc

%changelog
* Mon Aug 10 2020 Spruit, Neil R <neil.r.spruit@intel.com> - 1.0.17625
- Update to 1.0.17625

* Tue Apr 28 2020 Jacek Danecki <jacek.danecki@intel.com> - 0.8.16582-1
- Update to 0.8.16582

* Tue Mar 24 2020 Spruit, Neil R <neil.r.spruit@intel.com> - 0.8.0
* Update to 0.8.0

* Fri Mar 13 2020 Pavel Androniychuk <pavel.androniychuk@intel.com> - 0.4.1
- Spec file init
