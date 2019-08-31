%global igc_commit_id 8b0b9d18e0bc602b732fd50a920b203c23ef630f
%global major_version 1
%global minor_version 0
%global patch_version 2456
%global package_release 1

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
Intel(R) Graphics Compiler for OpenCL(TM).

%package       core
Summary:       Intel(R) Graphics Compiler Core

%description   core

%package       opencl
Summary:       Intel(R) Graphics Compiler Frontend
Requires:      %{name}-core = %{version}-%{release}

%description   opencl

%package       opencl-devel
Summary:       Intel(R) Graphics Compiler development package
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
/usr/lib64/libiga64.so.%{major_version}.%{minor_version}.10
/usr/lib64/libigc.so.%{major_version}
/usr/lib64/libigc.so.%{major_version}.%{minor_version}.10

%files opencl
%defattr(-,root,root)
/usr/lib64/libigdfcl.so.%{major_version}
/usr/lib64/libigdfcl.so.%{major_version}.%{minor_version}.10

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

