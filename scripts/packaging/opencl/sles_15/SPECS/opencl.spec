# spec file for package intel-opencl

#it's changed by external script
%global rel i1
%global ver xxx
%global NEO_OCL_VERSION_MAJOR xxx
%global NEO_OCL_VERSION_MINOR xxx
%global NEO_OCL_VERSION_BUILD xxx
%global NEO_RELEASE_WITH_REGKEYS FALSE
%global NEO_ENABLE_I915_PRELIM_DETECTION FALSE
%global NEO_ENABLE_XE_PRELIM_DETECTION FALSE
%global NEO_ENABLE_XE_EU_DEBUG_SUPPORT FALSE
%global NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM FALSE
%global NEO_I915_PRELIM_HEADERS_DIR %{nil}
%global NEO_OCLOC_VERSION_MODE 1

%define gmmlib_sover 12
%define igc_sover 2

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
%if "%{NEO_I915_PRELIM_HEADERS_DIR}" != ""
Source2:        uapi.tar.xz
%endif

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
%if "%{NEO_I915_PRELIM_HEADERS_DIR}" == ""
%autosetup -p1 -n compute-runtime
%else
%autosetup -p1 -n compute-runtime -b 2
%endif

%build
%cmake .. \
   -GNinja ${NEO_BUILD_EXTRA_OPTS} \
   -DNEO_OCL_VERSION_MAJOR=%{NEO_OCL_VERSION_MAJOR} \
   -DNEO_OCL_VERSION_MINOR=%{NEO_OCL_VERSION_MINOR} \
   -DNEO_VERSION_BUILD=%{NEO_OCL_VERSION_BUILD} \
   -DCMAKE_BUILD_TYPE=%{build_type} \
   -DBUILD_WITH_L0=FALSE \
   -DNEO_SKIP_UNIT_TESTS=TRUE \
   -DNEO_ENABLE_I915_PRELIM_DETECTION=%{NEO_ENABLE_I915_PRELIM_DETECTION} \
   -DNEO_ENABLE_XE_PRELIM_DETECTION=%{NEO_ENABLE_XE_PRELIM_DETECTION} \
   -DNEO_ENABLE_XE_EU_DEBUG_SUPPORT=%{NEO_ENABLE_XE_EU_DEBUG_SUPPORT} \
   -DNEO_USE_XE_EU_DEBUG_EXP_UPSTREAM=%{NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM} \
   -DRELEASE_WITH_REGKEYS=%{NEO_RELEASE_WITH_REGKEYS} \
   -DCMAKE_VERBOSE_MAKEFILE=FALSE \
   -DNEO_I915_PRELIM_HEADERS_DIR=$(realpath %{NEO_I915_PRELIM_HEADERS_DIR})

%if 0%{?neo_rpm__post_cmake:0}
%{neo_rpm__post_cmake_command}
%endif

%ninja_build

%install
cd build
%ninja_install

rm -vf %{buildroot}/%{_libdir}/intel-opencl/libigdrcl*.so.debug
rm -vf %{buildroot}/%{_libdir}/libocloc*.so.debug
rm -rvf %{buildroot}/usr/lib/debug/

#insert license into package
mkdir -p %{buildroot}/usr/share/doc/intel-opencl%{?name_suffix}/
cp -pvR %{_sourcedir}/copyright %{buildroot}/usr/share/doc/intel-opencl%{?name_suffix}/.
mkdir -p %{buildroot}/usr/share/doc/intel-ocloc%{?name_suffix}/
cp -pvR %{_sourcedir}/copyright %{buildroot}/usr/share/doc/intel-ocloc%{?name_suffix}/.

if [ -d "${NEO_NINJA_DUMP}" ]; then
    cp -v .ninja* ${NEO_NINJA_DUMP}/
fi

%files -n intel-opencl%{?name_suffix}
%defattr(-,root,root)
%{_sysconfdir}/OpenCL
%{_libdir}/intel-opencl/libigdrcl*.so
/usr/share/doc/intel-opencl%{?name_suffix}/copyright

%files -n intel-ocloc%{?name_suffix}
%defattr(-,root,root)
%{_bindir}/ocloc*
%{_libdir}/libocloc*.so
%{_includedir}/ocloc_api.h
/usr/share/doc/intel-ocloc%{?name_suffix}/copyright

%post -n intel-ocloc%{?name_suffix}
update-alternatives --quiet --install /usr/bin/ocloc ocloc /usr/bin/ocloc-%{NEO_OCL_VERSION_MAJOR}.%{NEO_OCL_VERSION_MINOR}.%{NEO_OCLOC_VERSION_MODE} %{NEO_OCL_VERSION_MAJOR}%{NEO_OCL_VERSION_MINOR}%{NEO_OCLOC_VERSION_MODE}

%preun -n intel-ocloc%{?name_suffix}
if [ $1 == "0" ]; then
    # uninstall
    update-alternatives --quiet --remove ocloc /usr/bin/ocloc-%{NEO_OCL_VERSION_MAJOR}.%{NEO_OCL_VERSION_MINOR}.%{NEO_OCLOC_VERSION_MODE}
fi

%changelog
* Mon Sep 13 2021 Compute-Runtime-Automation <compute-runtime@intel.com>
- Initial spec file for SLES 15.3
