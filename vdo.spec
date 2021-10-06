%define spec_release 1
#
#
#
Summary: Management tools for Virtual Data Optimizer
Name: vdo
Version: 8.2.0.0
Release: %{spec_release}%{?dist}
License: GPLv2
Source0: %{name}-%{version}.tgz
URL: http://github.com/dm-vdo/vdo
Requires: libuuid >= 2.23
Requires: kmod-kvdo >= 6.2
ExclusiveArch: x86_64
ExcludeArch: s390
ExcludeArch: s390x
ExcludeArch: ppc
ExcludeArch: ppc64
ExcludeArch: ppc64le
ExcludeArch: aarch64
ExcludeArch: i686
BuildRequires: device-mapper-devel
BuildRequires: device-mapper-event-devel
BuildRequires: gcc
BuildRequires: libblkid-devel
BuildRequires: libuuid-devel
BuildRequires: make
BuildRequires: valgrind-devel
BuildRequires: zlib-devel

# Disable an automatic dependency due to a file in examples/monitor.
%define __requires_exclude perl

%description
Virtual Data Optimizer (VDO) is a device mapper target that delivers
block-level deduplication, compression, and thin provisioning.

This package provides the user-space management tools for VDO.

%prep
%setup -q

%build
make

%install
make install DESTDIR=$RPM_BUILD_ROOT INSTALLOWNER= bindir=%{_bindir} \
  defaultdocdir=%{_defaultdocdir} name=%{name} mandir=%{_mandir} \
  unitdir=%{_unitdir} presetdir=%{_presetdir} sysconfdir=%{_sysconfdir}

%files
#defattr(-,root,root)
%{_bindir}/vdodmeventd
%{_bindir}/vdodumpconfig
%{_bindir}/vdoforcerebuild
%{_bindir}/vdoformat
%{_bindir}/vdosetuuid
%{_bindir}/vdostats
%dir %{_defaultdocdir}/%{name}
%license %{_defaultdocdir}/%{name}/COPYING
%dir %{_defaultdocdir}/%{name}/examples
%dir %{_defaultdocdir}/%{name}/examples/ansible
%doc %{_defaultdocdir}/%{name}/examples/ansible/README.txt
%doc %{_defaultdocdir}/%{name}/examples/ansible/test_vdocreate.yml
%doc %{_defaultdocdir}/%{name}/examples/ansible/test_vdocreate_alloptions.yml
%doc %{_defaultdocdir}/%{name}/examples/ansible/test_vdoremove.yml
%dir %{_defaultdocdir}/%{name}/examples/monitor
%doc %{_defaultdocdir}/%{name}/examples/monitor/monitor_check_vdostats_logicalSpace.pl
%doc %{_defaultdocdir}/%{name}/examples/monitor/monitor_check_vdostats_physicalSpace.pl
%doc %{_defaultdocdir}/%{name}/examples/monitor/monitor_check_vdostats_savingPercent.pl
%{_mandir}/man8/vdodmeventd.8.gz
%{_mandir}/man8/vdodumpconfig.8.gz
%{_mandir}/man8/vdoforcerebuild.8.gz
%{_mandir}/man8/vdoformat.8.gz
%{_mandir}/man8/vdosetuuid.8.gz
%{_mandir}/man8/vdostats.8.gz
%dir %{_sysconfdir}/bash_completion.d
%{_sysconfdir}/bash_completion.d/vdostats

%package support
Summary: Support tools for Virtual Data Optimizer
License: GPLv2
Requires: libuuid >= 2.23
ExclusiveArch: x86_64
ExcludeArch: s390
ExcludeArch: s390x
ExcludeArch: ppc
ExcludeArch: ppc64
ExcludeArch: ppc64le
ExcludeArch: aarch64
ExcludeArch: i686

%description support
Virtual Data Optimizer (VDO) is a device mapper target that delivers
block-level deduplication, compression, and thin provisioning.

This package provides the user-space support tools for VDO.

%files support
%{_bindir}/vdoaudit
%{_bindir}/vdodebugmetadata
%{_bindir}/vdodumpblockmap
%{_bindir}/vdodumpmetadata
%{_bindir}/vdolistmetadata
%{_bindir}/vdoreadonly
%{_bindir}/vdoregenerategeometry
%{_mandir}/man8/vdoaudit.8.gz
%{_mandir}/man8/vdodebugmetadata.8.gz
%{_mandir}/man8/vdodumpblockmap.8.gz
%{_mandir}/man8/vdodumpmetadata.8.gz
%{_mandir}/man8/vdolistmetadata.8.gz
%{_mandir}/man8/vdoreadonly.8.gz
%{_mandir}/man8/vdoregenerategeometry.8.gz

%changelog
* Wed Oct 06 2021 - Red Hat VDO Team <vdo-devel@redhat.com> - 8.2.0.0-1
- See https://github.com/dm-vdo/vdo.git
