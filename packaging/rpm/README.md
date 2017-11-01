# Overview
A spec file for VDO to create an RPM with.

# TODO
- The files now install to the location that `make install` has configured. The locations do not use a logical or POSIX-compliant location.
- Permissions of files and directoies are now undetermined, likely incorrect.
- The service (/vdo.service) may need to be registered.
- The configuration file (/etc/vdoconf.yml) may need to be listed as a ghost file.
- A dependency on kvdo exists, but that package is not ready.
