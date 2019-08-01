%global igc_commit_id c7dec76146e3a18b9ed9f489d033e65ff224e869
%global major_version 1
%global minor_version 0
%global patch_version 10
%global package_release 2

Name: intel-igc
Version: %{major_version}.%{minor_version}.%{patch_version}
Release: %{package_release}%{?dist}
Summary: Intel(R) Graphics Compiler for OpenCL(TM)

Group: System Environment/Libraries
License: MIT
URL: https://github.com/intel/intel-graphics-compiler
Source0: https://github.com/intel/intel-graphics-compiler/archive/%{igc_commit_id}/igc.tar.gz

BuildRequires: cmake clang gcc-c++ make procps flex bison python2 llvm-devel clang-devel pkg-config
BuildRequires: intel-opencl-clang-devel

%description

%package       core
Summary:       Intel(R) Graphics Compiler Core

%description   core

%package       opencl
Summary:       Intel(R) Graphics Compiler Frontend for OpenCL(TM)
Requires:      %{name}-core = %{version}-%{release}

%description   opencl

%package       opencl-devel
Summary:       Intel(R) Graphics Compiler development package for OpenCL(TM)
Requires:      %{name}-opencl = %{version}-%{release}

%description   opencl-devel


%prep

%build
echo "==== BUILD ===="
rm -rf *

mkdir igc
tar xzf $RPM_SOURCE_DIR/igc.tar.gz -C igc --strip-components=1

mkdir build
cd build

cmake ../igc -Wno-dev -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr  -DCOMMON_CLANG_LIBRARY_NAME=opencl-clang -DIGC_PREFERRED_LLVM_VERSION=8.0.0
make -j 1

%install
echo "==== INSTALL ===="
cd build
make -j`nproc` install DESTDIR=$RPM_BUILD_ROOT

rm -fv $RPM_BUILD_ROOT/usr/bin/GenX_IR
rm -fv $RPM_BUILD_ROOT/usr/bin/iga64
echo "==== DONE ===="

%files core
%defattr(-,root,root)
/usr/lib64/libiga64.so.%{major_version}
/usr/lib64/libiga64.so.%{major_version}.%{minor_version}.%{patch_version}
/usr/lib64/libigc.so.%{major_version}
/usr/lib64/libigc.so.%{major_version}.%{minor_version}.%{patch_version}

%files opencl
%defattr(-,root,root)
/usr/lib64/libigdfcl.so.%{major_version}
/usr/lib64/libigdfcl.so.%{major_version}.%{minor_version}.%{patch_version}

%files opencl-devel
%defattr(-,root,root)
/usr/include/igc/*
/usr/include/iga/*
/usr/include/visa/*
/usr/lib64/libiga64.so
/usr/lib64/libigc.so
/usr/lib64/libigdfcl.so
/usr/lib64/pkgconfig/*

# %doc


# %changelog

