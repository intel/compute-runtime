# spec file for package intel-opencl

#it's changed by external script
%global rel i1
%global ver xxx
%global NEO_OCL_VERSION_MAJOR xxx
%global NEO_OCL_VERSION_MINOR xxx
%global NEO_OCL_VERSION_BUILD xxx
%global NEO_RELEASE_WITH_REGKEYS FALSE

%define gmmlib_sover 12
%define igc_sover 1

%if !0%{?build_type:1}
%define build_type  Release
%endif

%define _source_payload w5T16.xzdio
%define _binary_payload w5T16.xzdio

Name:           intel-opencl
Epoch:          1
Version:        %{ver}
Release:        %{rel}%{?dist}
Summary:        Intel(R) Graphics Compute Runtime for OpenCL(TM)
License:        MIT
Group:          System Environment/Libraries
Url:            https://github.com/intel/compute-runtime
Source0:        %{url}/archive/%{version}/compute-runtime.tar.xz
Source1:        copyright

ExclusiveArch:  x86_64
BuildRequires:  cmake gcc-c++ ninja make
# BuildRequires:  libva-devel
BuildRequires:  libigdgmm%{?name_suffix}-devel
BuildRequires:  libigdfcl%{?name_suffix}-devel
Requires:       libigc%{igc_sover}%{?name_suffix}
Requires:       libigdfcl%{igc_sover}%{?name_suffix}
Requires:       libigdgmm%{gmmlib_sover}%{?name_suffix}

%description
Intel(R) Graphics Compute Runtime for OpenCL(TM) is a open source project to
converge Intel's development efforts on OpenCL(TM) compute stacks supporting
the GEN graphics hardware architecture.

%package     -n intel-ocloc%{?name_suffix}
Summary:        ocloc package for opencl
%description -n intel-ocloc%{?name_suffix}

%debug_package %{nil}

%prep
%autosetup -p1 -n compute-runtime

%build
%cmake .. \
   -GNinja ${NEO_BUILD_EXTRA_OPTS} \
   -DNEO_OCL_VERSION_MAJOR=%{NEO_OCL_VERSION_MAJOR} \
   -DNEO_OCL_VERSION_MINOR=%{NEO_OCL_VERSION_MINOR} \
   -DNEO_VERSION_BUILD=%{NEO_OCL_VERSION_BUILD} \
   -DCMAKE_BUILD_TYPE=%{build_type} \
   -DBUILD_WITH_L0=FALSE \
   -DNEO_SKIP_UNIT_TESTS=TRUE \
   -DNEO_ENABLE_i915_PRELIM_DETECTION=TRUE \
   -DRELEASE_WITH_REGKEYS=%{NEO_RELEASE_WITH_REGKEYS} \
   -DCMAKE_VERBOSE_MAKEFILE=FALSE
%ninja_build

%install
cd build
%ninja_install

chmod +x %{buildroot}/%{_libdir}/intel-opencl/libigdrcl.so
rm -vf %{buildroot}/%{_libdir}/intel-opencl/libigdrcl.so.debug
rm -vf %{buildroot}/%{_libdir}/libocloc.so.debug
rm -rvf %{buildroot}/usr/lib/debug/

#insert license into package
mkdir -p %{buildroot}/usr/share/doc/intel-opencl%{?name_suffix}/
cp -pvR %{_sourcedir}/copyright %{buildroot}/usr/share/doc/intel-opencl%{?name_suffix}/.
mkdir -p %{buildroot}/usr/share/doc/intel-ocloc%{?name_suffix}/
cp -pvR %{_sourcedir}/copyright %{buildroot}/usr/share/doc/intel-ocloc%{?name_suffix}/.

%files -n intel-opencl%{?name_suffix}
%defattr(-,root,root)
%{_sysconfdir}/OpenCL
%{_libdir}/intel-opencl/libigdrcl.so
/usr/share/doc/intel-opencl%{?name_suffix}/copyright

%files -n intel-ocloc%{?name_suffix}
%defattr(-,root,root)
%{_bindir}/ocloc
%{_libdir}/libocloc.so
%{_includedir}/ocloc_api.h
/usr/share/doc/intel-ocloc%{?name_suffix}/copyright

%changelog
* Mon Sep 13 2021 Compute-Runtime-Automation <compute-runtime@intel.com>
- Initial spec file for SLES 15.3
