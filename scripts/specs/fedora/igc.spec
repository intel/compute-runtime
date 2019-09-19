%global igc_commit_id 6dd3bde8f93ee61f798f84a34a9c9e66d0aedd49
%global major_version 1
%global minor_version 0
%global patch_version 2500
%global package_release 2

Name: intel-igc
Version: %{major_version}.%{minor_version}.%{patch_version}
Release: %{package_release}%{?dist}
Summary: Intel(R) Graphics Compiler for OpenCL(TM)

Group: System Environment/Libraries
License: MIT
URL: https://github.com/intel/intel-graphics-compiler
Source0: https://github.com/intel/intel-graphics-compiler/archive/%{igc_commit_id}/igc.tar.gz
Patch0: https://github.com/intel/intel-graphics-compiler/commit/8621841947517aaf4b4278f9f6e94d74cc126747.patch
Patch1: https://github.com/intel/intel-graphics-compiler/commit/0a69375bf58c5d100cf5d77b3d903720b4af966e.patch

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
cd igc
patch -p1 < $RPM_SOURCE_DIR/8621841947517aaf4b4278f9f6e94d74cc126747.patch
patch -p1 < $RPM_SOURCE_DIR/0a69375bf58c5d100cf5d77b3d903720b4af966e.patch
cd ..
mkdir build
cd build

cmake ../igc -Wno-dev -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr  \
 -DCOMMON_CLANG_LIBRARY_NAME=opencl-clang -DIGC_PREFERRED_LLVM_VERSION=8.0.0 -DIGC_PACKAGE_RELEASE=%{patch_version}
make -j `nproc`

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

%doc


%changelog

