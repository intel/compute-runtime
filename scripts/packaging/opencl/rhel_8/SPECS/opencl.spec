#it's changed by external script
%global rel i1
%global ver xxx
%global NEO_OCL_VERSION_MAJOR xxx
%global NEO_OCL_VERSION_MINOR xxx
%global NEO_OCL_VERSION_BUILD xxx
%global NEO_RELEASE_WITH_REGKEYS FALSE
%global I915_HEADERS_DIR %{nil}

%define _source_payload w5T16.xzdio
%define _binary_payload w5T16.xzdio

Name:    intel-opencl
Epoch:   1
Version: %{ver}
Release: %{rel}%{?dist}
Summary: Intel(R) Graphics Compute Runtime for OpenCL(TM)

Group: System Environment/Libraries
License: MIT
URL: https://github.com/intel/compute-runtime
Source0: %{url}/archive/%{version}/compute-runtime.tar.xz
Source1: copyright
%if "%{I915_HEADERS_DIR}" != ""
Source2: uapi.tar.xz
%endif

Requires:      intel-gmmlib
Requires:      intel-igc-opencl

BuildRequires: libva-devel gcc-c++ cmake ninja-build make
BuildRequires: intel-gmmlib-devel
BuildRequires: intel-igc-opencl-devel

%description
Intel(R) Graphics Compute Runtime for OpenCL(TM) is a open source project to converge Intel's development efforts on OpenCL(TM) compute stacks supporting the GEN graphics hardware architecture.

%package       -n intel-ocloc
Summary:       ocloc package for opencl
Requires:      intel-igc-opencl
%description   -n intel-ocloc
Intel(R) Graphics Compute Runtime for OpenCL(TM) is a open source project to converge Intel's development efforts on OpenCL(TM) compute stacks supporting the GEN graphics hardware architecture.

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
   -DNEO_OCL_VERSION_MAJOR=%{NEO_OCL_VERSION_MAJOR} \
   -DNEO_OCL_VERSION_MINOR=%{NEO_OCL_VERSION_MINOR} \
   -DNEO_VERSION_BUILD=%{NEO_OCL_VERSION_BUILD} \
   -DCMAKE_BUILD_TYPE=Release \
   -DBUILD_WITH_L0=FALSE \
   -DNEO_SKIP_UNIT_TESTS=TRUE \
   -DNEO_ENABLE_i915_PRELIM_DETECTION=TRUE \
   -DNEO_ENABLE_XE_DRM_DETECTION=TRUE \
   -DRELEASE_WITH_REGKEYS=%{NEO_RELEASE_WITH_REGKEYS} \
   -DCMAKE_VERBOSE_MAKEFILE=FALSE \
   -DI915_HEADERS_DIR=$(realpath %{I915_HEADERS_DIR})
%ninja_build

%install
cd build
%ninja_install

chmod +x %{buildroot}/%{_libdir}/intel-opencl/libigdrcl.so
chmod +x %{buildroot}/%{_libdir}/libocloc.so
rm -vf %{buildroot}/%{_libdir}/intel-opencl/libigdrcl.so.debug
rm -vf %{buildroot}/%{_libdir}/libocloc.so.debug
rm -rvf %{buildroot}/usr/lib/debug/
#insert license into package
mkdir -p %{buildroot}/usr/share/doc/intel-opencl/
cp -pvR %{_sourcedir}/copyright %{buildroot}/usr/share/doc/intel-opencl/.
mkdir -p %{buildroot}/usr/share/doc/intel-ocloc/
cp -pvR %{_sourcedir}/copyright %{buildroot}/usr/share/doc/intel-ocloc/.

%files
%defattr(-,root,root)
%{_libdir}/intel-opencl/libigdrcl.so
%config(noreplace)
/etc/OpenCL/vendors/intel.icd
/usr/share/doc/intel-opencl/copyright

%files -n intel-ocloc
%defattr(-,root,root)
%{_bindir}/ocloc
%{_libdir}/libocloc.so
%{_includedir}/ocloc_api.h
%config(noreplace)
/usr/share/doc/intel-ocloc/copyright

%doc

%changelog
* Wed May 6 2020 Pavel Androniychuk <pavel.androniychuk@intel.com> - 20.17.16650
- Update spec files to pull version automatically.

* Tue Apr 28 2020 Jacek Danecki <jacek.danecki@intel.com> - 20.16.16582-1
- Update to 20.16.16582

* Tue Mar 03 2020 Jacek Danecki <jacek.danecki@intel.com> - 20.08.15750-1
- Update to 20.08.15750

* Tue Jan 14 2020 Jacek Danecki <jacek.danecki@intel.com> - 20.01.15264-1
- Update to 20.01.15264
- Updated IGC
- Updated gmmlib
