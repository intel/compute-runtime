%global neo_commit_id ab74b60

Name: intel-opencl
Version: 19.28.13502
Release: 1%{?dist}
Summary: Intel(R) Graphics Compute Runtime for OpenCL(TM)

Group: System Environment/Libraries
License: MIT
URL: https://github.com/intel/compute-runtime
Source0: https://github.com/intel/compute-runtime/archive/%{neo_commit_id}.tar.gz

BuildRequires: cmake gcc-c++ ninja-build make procps python2 sed libva-devel

BuildRequires: intel-gmmlib-devel >= 19.2.3
Requires: intel-gmmlib >= 19.2.3

BuildRequires: intel-igc-opencl-devel >= 1.0.10
Requires: intel-igc-opencl >= 1.0.10

%description


%prep


%build
echo "==== BUILD ===="
rm -rf *

mkdir neo
tar xzf $RPM_SOURCE_DIR/%{neo_commit_id}.tar.gz -C neo --strip-components=1

mkdir build
cd build

cmake ../neo -DCMAKE_BUILD_TYPE=Release -DNEO_DRIVER_VERSION=%{version}
make -j`nproc` igdrcl_dll

echo "==== DONE ===="


%install
echo "==== INSTALL ===="
mkdir -p $RPM_BUILD_ROOT/usr/lib64/intel-opencl
mkdir -p $RPM_BUILD_ROOT/etc/OpenCL/vendors
cp $RPM_BUILD_DIR/build/bin/libigdrcl.so $RPM_BUILD_ROOT/usr/lib64/intel-opencl
strip $RPM_BUILD_ROOT/usr/lib64/intel-opencl/libigdrcl.so
echo "/usr/lib64/intel-opencl/libigdrcl.so" > $RPM_BUILD_ROOT/etc/OpenCL/vendors/intel.icd
echo "==== DONE ===="


%files
%defattr(-,root,root)
/usr/lib64/intel-opencl/libigdrcl.so

%config(noreplace)
/etc/OpenCL/vendors/intel.icd

# %doc


# %changelog
