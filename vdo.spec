%define spec_release 1
%define bash_completions_dir %{_datadir}/bash-completion/completions

Summary: Management tools for Virtual Data Optimizer
Name: vdo
Version: 8.3.0.72
Release: %{spec_release}%{?dist}

License: GPL-2.0-only
URL: http://github.com/dm-vdo/vdo
Source0: %{name}-%{version}.tgz

Requires: kmod-kvdo >= 8.2
ExcludeArch: s390
ExcludeArch: ppc
ExcludeArch: ppc64
ExcludeArch: i686
BuildRequires: device-mapper-devel
BuildRequires: device-mapper-event-devel
BuildRequires: gcc
BuildRequires: libblkid-devel
BuildRequires: libuuid-devel
BuildRequires: make
%ifarch %{valgrind_arches}
BuildRequires: valgrind-devel
%endif
BuildRequires: zlib-devel

# Disable an automatic dependency due to a file in examples/monitor.
%define __requires_exclude perl

%description
Virtual Data Optimizer (VDO) is a device mapper target that delivers
block-level deduplication, compression, and thin provisioning.

This package provides the user-space management tools for VDO.

%package support
Summary: Support tools for Virtual Data Optimizer
License: GPL-2.0-only

Requires: libuuid >= 2.23
ExcludeArch: s390
ExcludeArch: ppc
ExcludeArch: ppc64
ExcludeArch: i686

%description support
Virtual Data Optimizer (VDO) is a device mapper target that delivers
block-level deduplication, compression, and thin provisioning.

This package provides the user-space support tools for VDO.

%prep
%setup -q

%build
make

%install
make install DESTDIR=$RPM_BUILD_ROOT INSTALLOWNER= name=%{name} bindir=%{_bindir} \
  mandir=%{_mandir} defaultdocdir=%{_defaultdocdir} libexecdir=%{_libexecdir} \
  presetdir=%{_presetdir} python3_sitelib=/%{python3_sitelib} \
  sysconfdir=%{_sysconfdir} unitdir=%{_unitdir}

%files
%license COPYING
%{_bindir}/vdoforcerebuild
%{_bindir}/vdoformat
%{_bindir}/vdostats
%{bash_completions_dir}/vdostats
%dir %{_defaultdocdir}/%{name}
%dir %{_defaultdocdir}/%{name}/examples
%dir %{_defaultdocdir}/%{name}/examples/monitor
%doc %{_defaultdocdir}/%{name}/examples/monitor/monitor_check_vdostats_logicalSpace.pl
%doc %{_defaultdocdir}/%{name}/examples/monitor/monitor_check_vdostats_physicalSpace.pl
%doc %{_defaultdocdir}/%{name}/examples/monitor/monitor_check_vdostats_savingPercent.pl
%{_mandir}/man8/vdoforcerebuild.8*
%{_mandir}/man8/vdoformat.8*
%{_mandir}/man8/vdostats.8*

%files support
%{_bindir}/adaptlvm
%{_bindir}/vdoaudit
%{_bindir}/vdodebugmetadata
%{_bindir}/vdodumpblockmap
%{_bindir}/vdodumpmetadata
%{_bindir}/vdolistmetadata
%{_bindir}/vdoreadonly
%{_bindir}/vdorecover
%{_mandir}/man8/adaptlvm.8*
%{_mandir}/man8/vdoaudit.8*
%{_mandir}/man8/vdodebugmetadata.8*
%{_mandir}/man8/vdodumpblockmap.8*
%{_mandir}/man8/vdodumpmetadata.8*
%{_mandir}/man8/vdolistmetadata.8*
%{_mandir}/man8/vdoreadonly.8*
%{_mandir}/man8/vdorecover.8*

%changelog
* Tue Oct 15 2024 - Red Hat VDO Team <dm-devel@lists.linux.dev> - 8.3.0.72-1
- See https://github.com/dm-vdo/vdo.git
