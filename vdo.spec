%define spec_release 1
#
#
#
Summary: Management tools for Virtual Data Optimizer
Name: vdo
Version: 6.2.4.14
Release: %{spec_release}%{?dist}
License: GPLv2
Source0: %{name}-%{version}.tgz
URL: http://github.com/dm-vdo/vdo
Requires: python3 >= 3.6
Requires: python3-PyYAML >= 3.10
Requires: libuuid >= 2.23
Requires: lvm2
Requires: kmod-kvdo >= 6.2
ExclusiveArch: x86_64
ExcludeArch: s390
ExcludeArch: s390x
ExcludeArch: ppc
ExcludeArch: ppc64
ExcludeArch: ppc64le
ExcludeArch: aarch64
ExcludeArch: i686
BuildRequires: device-mapper-event-devel
BuildRequires: device-mapper-devel
BuildRequires: libblkid-devel
BuildRequires: gcc
BuildRequires: libuuid-devel
BuildRequires: make
BuildRequires: python3
BuildRequires: python3-devel
BuildRequires: systemd
BuildRequires: valgrind-devel
BuildRequires: zlib-devel
%{?systemd_requires}

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
  defaultdocdir=%{_defaultdocdir} name=%{name} \
  python3_sitelib=/%{python3_sitelib} mandir=%{_mandir} \
  unitdir=%{_unitdir} presetdir=%{_presetdir} sysconfdir=%{_sysconfdir}

%post
%systemd_post vdo.service

%preun
%systemd_preun vdo.service

%postun
%systemd_postun_with_restart vdo.service

%files
#defattr(-,root,root)
%{_bindir}/vdo
%{_bindir}/vdostats
%{_bindir}/vdodmeventd
%{_bindir}/vdodumpconfig
%{_bindir}/vdoforcerebuild
%{_bindir}/vdoformat
%{_bindir}/vdosetuuid
%{_bindir}/vdo-by-dev
%dir %{python3_sitelib}/%{name}
%{python3_sitelib}/%{name}/__pycache__/*
%{python3_sitelib}/%{name}/__init__.py
%dir %{python3_sitelib}/%{name}/vdomgmnt/
%{python3_sitelib}/%{name}/vdomgmnt/__pycache__/*
%{python3_sitelib}/%{name}/vdomgmnt/CommandLock.py
%{python3_sitelib}/%{name}/vdomgmnt/Configuration.py
%{python3_sitelib}/%{name}/vdomgmnt/Constants.py
%{python3_sitelib}/%{name}/vdomgmnt/Defaults.py
%{python3_sitelib}/%{name}/vdomgmnt/ExitStatusMixins.py
%{python3_sitelib}/%{name}/vdomgmnt/KernelModuleService.py
%{python3_sitelib}/%{name}/vdomgmnt/MgmntUtils.py
%{python3_sitelib}/%{name}/vdomgmnt/Service.py
%{python3_sitelib}/%{name}/vdomgmnt/SizeString.py
%{python3_sitelib}/%{name}/vdomgmnt/Utils.py
%{python3_sitelib}/%{name}/vdomgmnt/VDOArgumentParser.py
%{python3_sitelib}/%{name}/vdomgmnt/VDOService.py
%{python3_sitelib}/%{name}/vdomgmnt/VDOKernelModuleService.py
%{python3_sitelib}/%{name}/vdomgmnt/VDOOperation.py
%{python3_sitelib}/%{name}/vdomgmnt/__init__.py
%dir %{python3_sitelib}/%{name}/statistics/
%{python3_sitelib}/%{name}/statistics/Command.py
%{python3_sitelib}/%{name}/statistics/Field.py
%{python3_sitelib}/%{name}/statistics/KernelStatistics.py
%{python3_sitelib}/%{name}/statistics/LabeledValue.py
%{python3_sitelib}/%{name}/statistics/StatFormatter.py
%{python3_sitelib}/%{name}/statistics/StatStruct.py
%{python3_sitelib}/%{name}/statistics/VDOReleaseVersions.py
%{python3_sitelib}/%{name}/statistics/VDOStatistics.py
%{python3_sitelib}/%{name}/statistics/__init__.py
%{python3_sitelib}/%{name}/statistics/__pycache__/*
%dir %{python3_sitelib}/%{name}/utils/
%{python3_sitelib}/%{name}/utils/Command.py
%{python3_sitelib}/%{name}/utils/FileUtils.py
%{python3_sitelib}/%{name}/utils/Timeout.py
%{python3_sitelib}/%{name}/utils/Transaction.py
%{python3_sitelib}/%{name}/utils/YAMLObject.py
%{python3_sitelib}/%{name}/utils/__init__.py
%{python3_sitelib}/%{name}/utils/__pycache__/*
%{_unitdir}/vdo.service
%{_unitdir}/vdo-start-by-dev@.service
%{_presetdir}/97-vdo.preset
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
%dir %{_defaultdocdir}/%{name}/examples/systemd
%doc %{_defaultdocdir}/%{name}/examples/systemd/VDO.mount.example
%{_mandir}/man8/vdo.8.gz
%{_mandir}/man8/vdostats.8.gz
%{_mandir}/man8/vdodmeventd.8.gz
%{_mandir}/man8/vdodumpconfig.8.gz
%{_mandir}/man8/vdoforcerebuild.8.gz
%{_mandir}/man8/vdoformat.8.gz
%{_mandir}/man8/vdosetuuid.8.gz
%dir %{_sysconfdir}/bash_completion.d
%{_sysconfdir}/bash_completion.d/vdo
%{_sysconfdir}/bash_completion.d/vdostats
%{_sysconfdir}/udev/rules.d/69-vdo-start-by-dev.rules

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
* Thu Oct 01 2020 - Red Hat VDO Group <vdo-devel@redhat.com> - 6.2.4.14-1
HASH(0x55c9459fe3b8)
