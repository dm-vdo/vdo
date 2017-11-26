#!/usr/bin/python

#
# Copyright (c) 2017 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

from __future__ import absolute_import, division, print_function
__metaclass__ = type


ANSIBLE_METADATA = {
    'metadata_version': '1.1',
    'status': ['preview'],
    'supported_by': 'community'
}

DOCUMENTATION = '''
---
author:
    - "Bryan Gurney (@bgurney-rh)"

module: vdo

short_description: Module to control VDO

version_added: "2.5"

description:
    - "This module controls the VDO dedupe and compression device."

options:
    name:
        description:
            - The name of the VDO volume.
        required: true
    state:
        choices: [ "present", "absent" ]
        description:
            - Whether this VDO volume should be "present" or "absent".
              If a "present" VDO volume already exists, it will not be
              created. If an "absent" VDO volume does not exist, it
              will not be removed.
        required: true
    activated:
        choices: [ "yes", "no" ]
        description:
            - The "activate" status for a VDO volume.  If this is set
              to "no", the VDO volume cannot be started, and it will
              not start on system startup.
        required: false
    running:
        choices: [ "yes", "no" ]
        description:
            - Whether this VDO volume is running.  A VDO volume must
              be activated in order to be started.
        required: false
    device:
        description:
            - The full path of the device to use for VDO storage.
              This is required if "state" is "present".
        required: false
    logicalsize:
        description:
            - The logical size of the VDO volume (in megabytes, or
              LVM suffix format).  If not specified, this defaults to
              the same size as the underlying storage device, which
              is specified in the 'device' parameter.
        required: false
    compression:
        choices: [ "enabled", "disabled" ]
        description:
            - Configures whether compression is enabled.  The default
              for a created volume is 'enabled'.
        required: false
    blockmapcachesize:
        description:
            - The amount of memory allocated for caching block map
              pages, in megabytes (or may be issued with an LVM-style
              suffix of K, M, G, or T).  The default (and minimum)
              value is 128M.  The value specifies the size of the
              cache; there is a 15% memory usage overhead. Each 1.25G
              of block map covers 1T of logical blocks, therefore a
              small amount of block map cache memory can cache a
              significantly large amount of block map data.
        required: false
    readcache:
        choices: [ "enabled", "disabled" ]
        description:
            - Enables or disables the read cache.  The default is
              'disabled'.  Choosing 'enabled' enables a read cache
              which may improve performance for workloads of high
              deduplication, read workloads with a high level of
              compression, or on hard disk storage.
        required: false
    readcachesize:
        description:
            - Specifies the extra VDO device read cache size in
              megabytes.  This is in addition to a system-defined
              minimum.  Using a value with a suffix of K, M, G, or T
              is optional.  The default value is 0.  1.125 MB of
              memory per bio thread will be used per 1 MB of read
              cache specified (for example, a VDO volume configured
              with 4 bio threads will have a read cache memory usage
              overhead of 4.5 MB per 1 MB of read cache specified).
        required: false
    emulate512:
        description:
            - Enables 512-byte emulation mode, allowing drivers or
              filesystems to access the VDO volume at 512-byte
              granularity, instead of the default 4096-byte granularity.
              Default is 'disabled'; only recommended when a driver
              or filesystem requires 512-byte sector level access to
              a device.
        required: false
    slabsize:
        description:
            - The size of the increment by which the physical size of
              a VDO volume is grown, in megabytes (or may be issued
              with an LVM-style suffix of K, M, G, or T).  Must be a
              power of two between 128M and 32G.  The default is 2G,
              which supports volumes having a physical size up to 16T.
              The maximum, 32G, supports a physical size of up to 256T.
        required: false
    writepolicy:
        description:
            - Specifies the write policy of the VDO volume.  The
              default 'sync' mode acknowledges writes only after data
              is on stable storage.  The 'async' mode acknowledges
              writes when data has been cached for writing to stable
              storage.
        required: false
    indexmem:
        description:
            - Specifies the amount of index memory in gigabytes.  The
              default is 0.25.  The special decimal values 0.25, 0.5,
              and 0.75 can be used, as can any positive integer.
        required: false
    indexmode:
        description:
            - Specifies the index mode of the Albireo index.  The
              default is 'dense', which has a deduplication window of
              1 GB of index memory per 1 TB of incoming data,
              requiring 10 GB of index data on persistent storage.
              The 'sparse' mode has a deduplication window of 1 GB of
              index memory per 10 TB of incoming data, but requires
              100 GB of index data on persistent storage.
        required: false
    ackthreads:
        description:
            - Specifies the number of threads to use for
              acknowledging completion of requested VDO I/O operations.
              Valid values are integer values from 1 to 100 (lower
              numbers are preferable due to overhead).  The default is
              1.
        required: false
    biothreads:
        description:
            - Specifies the number of threads to use for submitting I/O
              operations to the storage device.  Valid values are
              integer values from 1 to 100 (lower numbers are
              preferable due to overhead).  The default is 4.
        required: false
    cputhreads:
        description:
            - Specifies the number of threads to use for CPU-intensive
              work such as hashing or compression.  Valid values are
              integer values from 1 to 100 (lower numbers are
              preferable due to overhead).  The default is 2.
        required: false
    logicalthreads:
        description:
            - Specifies the number of threads across which to
              subdivide parts of the VDO processing based on logical
              block addresses.  Valid values are integer values from
              1 to 100 (lower numbers are preferable due to overhead).
              The default is 1.
        required: false
    physicalthreads:
        description:
            - Specifies the number of threads across which to
              subdivide parts of the VDO processing based on physical
              block addresses.  Valid values are integer values from
              1 to 16 (lower numbers are preferable due to overhead).
              The physical space used by the VDO volume must be
              larger than (slabsize * physicalthreads).  The default
              is 1.
        required: false
notes:
  - In general, the default thread configuration should be used.
'''

EXAMPLES = '''
# Create a VDO volume
- name: Create 2 TB VDO volume vdo1 on device /dev/md0
  vdo:
    name: vdo1
    state: present
    device: /dev/md0
    logicalsize: 2T

# Remove a VDO volume
- name: Remove VDO volume vdo1
  vdo:
    name: vdo1
    state: absent
'''

RETURN = '''# '''

from ansible.module_utils.basic import AnsibleModule
import re

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


# Generate a list of VDO volumes, whether they are running or stopped.
#
# @param module  The AnsibleModule object.
# @param vdo_cmd  The path of the 'vdo' command.
#
# @return vdolist  A list of currently created VDO volumes.
def inventory_vdos(module, vdo_cmd):
    rc, vdostatusout, err = module.run_command("%s status" % (vdo_cmd))

    # The VDO volume name is in a standalone line with a leading ' - '
    # and a trailing ':'.  This could potentially be collected via
    # PyYaml, but a VDO volume with a name that collides # with a
    # statistic field key (e.g.: 'Server') risks the inability to
    # return the volume name.  For now, don't create a VDO volume named
    # 'Server' or 'Enabled'

    # if rc != 0:
    #   module.fail_json(msg="Inventorying VDOs failed: %s"
    #                        % vdostatusout, rc=rc, err=err)

    vdolist = []

    if (rc == 2 and
            re.findall(r"vdoconf.yml does not exist", err, re.MULTILINE)):
        # If there is no /etc/vdoconf.yml file, assume there are no
        # VDO volumes. Return an empty list of VDO volumes.
        return vdolist

    if rc != 0:
        module.fail_json(msg="Inventorying VDOs failed: %s"
                             % vdostatusout, rc=rc, err=err)

    vdostatusyaml = yaml.load(vdostatusout)
    if vdostatusyaml is None:
        return vdolist

    vdoyamls = vdostatusyaml['VDOs']

    if vdoyamls is not None:
        vdolist = vdoyamls.keys()

    return vdolist


def list_running_vdos(module, vdo_cmd):
    rc, vdolistout, err = module.run_command("%s list" % (vdo_cmd))
    runningvdolist = filter(None, vdolistout.split('\n'))
    return runningvdolist


# Generate a string containing options to pass to the 'VDO' command.
# Note that a 'create' operation will pass more options than a
# 'modify' operation.
#
# @param params  A dictionary of parameters, and their values
#                (values of 'None' and/or nonexistent values are ignored).
#
# @return vdocmdoptions  A string to be used in a 'vdo <action>' command.
def start_vdo(module, vdoname, vdo_cmd):
    rc, _, err = module.run_command("%s start --name=%s" % (vdo_cmd, vdoname))
    return rc


def stop_vdo(module, vdoname, vdo_cmd):
    rc, _, err = module.run_command("%s stop --name=%s" % (vdo_cmd, vdoname))
    return rc


def activate_vdo(module, vdoname, vdo_cmd):
    rc, _, err = module.run_command("%s activate --name=%s" %
                                    (vdo_cmd, vdoname))
    return rc


def deactivate_vdo(module, vdoname, vdo_cmd):
    rc, _, err = module.run_command("%s deactivate --name=%s" %
                                    (vdo_cmd, vdoname))
    return rc


def add_vdooptions(params):
    vdocmdoptions = ""
    options = []

    if ('logicalsize' in params) and (params['logicalsize'] is not None):
        options.append("--vdoLogicalSize=" + params['logicalsize'])

    if (('blockmapcachesize' in params) and
            (params['blockmapcachesize'] is not None)):
        options.append("--blockMapCacheSize=" + params['blockmapcachesize'])

    if ('readcache' in params) and (params['readcache'] == 'enabled'):
        options.append("--readCache=enabled")

    if ('readcachesize' in params) and (params['readcachesize'] is not None):
        options.append("--readCacheSize=" + params['readcachesize'])

    if ('writepolicy' in params) and (params['writepolicy'] == 'async'):
        options.append("--writePolicy=async")

    if ('slabsize' in params) and (params['slabsize'] is not None):
        options.append("--vdoSlabSize=" + params['slabsize'])

    if ('emulate512' in params) and (params['emulate512'] == 'enabled'):
        options.append("--emulate512=enabled")

    if ('indexmem' in params) and (params['indexmem'] is not None):
        options.append("--indexMem=" + params['indexmem'])

    if ('indexmode' in params) and (params['indexmode'] == 'sparse'):
        options.append("--sparseIndex=enabled")

    # Entering an invalid thread config results in a cryptic
    # 'Could not set up device mapper for %s' error from the 'vdo'
    # command execution.  The dmsetup module on the system will
    # output a more helpful message, but one would have to log
    # onto that system to read the error.  For now, heed the thread
    # limit warnings in the DOCUMENTATION section above.
    if ('ackthreads' in params) and (params['ackthreads'] is not None):
        options.append("--vdoAckThreads=" + params['ackthreads'])

    if ('biothreads' in params) and (params['biothreads'] is not None):
        options.append("--vdoBioThreads=" + params['biothreads'])

    if ('cputhreads' in params) and (params['cputhreads'] is not None):
        options.append("--vdoCpuThreads=" + params['cputhreads'])

    if ('logicalthreads' in params) and (params['logicalthreads'] is not None):
        options.append("--vdoLogicalThreads=" + params['logicalthreads'])

    if (('physicalthreads' in params) and
            (params['physicalthreads'] is not None)):
        options.append("--vdoPhysicalThreads=" + params['physicalthreads'])

    vdocmdoptions = ' '.join(options)
    return vdocmdoptions


def run_module():

    # Debugging note:
    # Don't use "print" in ansible modules.  To print debug messages,
    # add it to the "result" dictionary, the contents of which is printed
    # when executing "ansible-playbook foo.yml -vvvv".

    # Define the available arguments/parameters that a user can pass to
    # the module.
    # Defaults for VDO parameters are None, in order to facilitate
    # the detection of parameters passed from the playbook.
    # Creation param defaults are determined by the creation section.

    module_args = dict(
        name=dict(type='str', required=True),
        state=dict(choices=['absent', 'present'],
                   required=False, default='present'),
        activated=dict(choices=['yes', 'no'],
                       required=False, default=None),
        running=dict(choices=['yes', 'no'],
                     required=False, default=None),
        growphysical=dict(choices=['yes', 'no'],
                          required=False, default='no'),
        device=dict(type='str', required=False),
        logicalsize=dict(type='str', required=False),
        deduplication=dict(choices=['enabled', 'disabled'],
                           required=False, default=None),
        compression=dict(choices=['enabled', 'disabled'],
                         required=False, default=None),
        blockmapcachesize=dict(type='str', required=False),
        readcache=dict(choices=['enabled', 'disabled'],
                       required=False, default=None),
        readcachesize=dict(type='str', required=False),
        emulate512=dict(choices=['enabled', 'disabled'],
                        required=False, default=None),
        slabsize=dict(type='str', required=False),
        writepolicy=dict(choices=['sync', 'async'],
                         required=False, default=None),
        indexmem=dict(type='str', required=False),
        indexmode=dict(choices=['dense', 'sparse'],
                       required=False, default=None),
        ackthreads=dict(type='str', required=False),
        biothreads=dict(type='str', required=False),
        cputhreads=dict(type='str', required=False),
        logicalthreads=dict(type='str', required=False),
        physicalthreads=dict(type='str', required=False)
    )

    # Seed the result dictionary in the object.  There will be an
    # 'invocation' dictionary added with 'module_args' (arguments
    # given) and 'module_name'.  This module will add various other
    # dictionaries and/or lists that may be helpful for diagnosis
    # when 'ansible-playbook' is executed with the '-vvvv' switch.
    result = dict(
        changed=False,
        vdobinary=''
    )

    # the AnsibleModule object will be our abstraction working with Ansible
    # this includes instantiation, a couple of common attr would be the
    # args/params passed to the execution, as well as if the module
    # supports check mode
    module = AnsibleModule(
        argument_spec=module_args,
        supports_check_mode=False
    )

    if not HAS_YAML:
        module.fail_json(msg='PyYAML is required for this module.')

    vdo_cmd = module.get_bin_path("vdo", required=True)
    if not vdo_cmd:
        module.fail_json(msg='VDO is not installed.', **result)

    result['vdobinary'] = vdo_cmd

    # Print a pre-run list of VDO volumes in the result object.
    vdolist = inventory_vdos(module, vdo_cmd)
    result['pre_vdolist'] = vdolist

    runningvdolist = list_running_vdos(module, vdo_cmd)
    result['runningvdos'] = runningvdolist

    # Collect the name of the desired VDO volume, and its state.  These will
    # determine what to do.
    desiredvdo = module.params['name']
    state = module.params['state']
    result['desiredvdos'] = desiredvdo

    # Create a desired VDO volume that doesn't exist yet.
    if (desiredvdo not in vdolist) and (state == 'present'):
        device = module.params['device']
        if device is None:
            module.fail_json(msg="Creating a VDO volume requires specifying "
                                 "a 'device' in the playbook.")

        # Create a dictionary of the options from the AnsibleModule
        # parameters, compile the vdo command options, and run "vdo create"
        # with those options.
        # Since this is a creation of a new VDO volume, it will contain all
        # all of the parameters given by the playbook; the rest will
        # assume default values.
        options = module.params
        vdocmdoptions = add_vdooptions(options)
        result['vdocmdoptions'] = vdocmdoptions
        rc, _, err = module.run_command("%s create --name=%s --device=%s %s"
                                        % (vdo_cmd, desiredvdo, device,
                                           vdocmdoptions))
        if rc == 0:
            result['changed'] = True
        else:
            module.fail_json(msg="Creating VDO %s failed."
                             % desiredvdo, rc=rc, err=err)

        if (module.params['compression'] == 'disabled'):
            rc, _, err = module.run_command("%s disableCompression --name=%s"
                                            % (vdo_cmd, desiredvdo))

        if ((module.params['deduplication'] is not None) and
                module.params['deduplication'] == 'disabled'):
            rc, _, err = module.run_command("%s disableDeduplication --name=%s"
                                            % (vdo_cmd, desiredvdo))

        if module.params['activated'] == 'no':
            deactivate_vdo(module, desiredvdo, vdo_cmd)

        if module.params['running'] == 'no':
            stop_vdo(module, desiredvdo, vdo_cmd)

        # Print a post-run list of VDO volumes in the result object.
        vdolist = inventory_vdos(module, vdo_cmd)
        result['post_vdolist'] = vdolist
        module.exit_json(**result)

    # Modify the current parameters of a VDO that exists.
    if (desiredvdo in vdolist) and (state == 'present'):
        rc, vdostatusoutput, err = module.run_command("%s status" % (vdo_cmd))
        vdostatusyaml = yaml.load(vdostatusoutput)

        # An empty dictionary to contain dictionaries of VDO statistics
        processedvdos = {}

        vdoyamls = vdostatusyaml['VDOs']
        if vdoyamls is not None:
            processedvdos = vdoyamls

        # The 'vdo status' keys that are currently modifiable.
        statusparamkeys = ['Acknowledgement threads',
                           'Bio submission threads',
                           'CPU-work threads',
                           'Logical threads',
                           'Physical threads',
                           'Read cache',
                           'Read cache size',
                           'Write policy',
                           'Compression',
                           'Deduplication']

        # A key translation table from 'vdo status' output to Ansible
        # module parameters.  This covers all of the 'vdo status'
        # parameter keys that could be modified with the 'vdo'
        # command.
        vdokeytrans = {
            'Logical size': 'logicalsize',
            'Compression': 'compression',
            'Deduplication': 'deduplication',
            'Block map cache size': 'blockmapcachesize',
            'Read cache': 'readcache',
            'Read cache size': 'readcachesize',
            'Write policy': 'writepolicy',
            'Acknowledgement threads': 'ackthreads',
            'Bio submission threads': 'biothreads',
            'CPU-work threads': 'cputhreads',
            'Logical threads': 'logicalthreads',
            'Physical threads': 'physicalthreads'
        }

        # Build a dictionary of the current VDO status parameters, with
        # the keys used by VDO.  (These keys will be converted later.)
        currentvdoparams = {}

        # Build a "lookup table" dictionary containing a translation table
        # of the parameters that can be modified
        modtrans = {}

        for statfield in statusparamkeys:
            currentvdoparams[statfield] = processedvdos[desiredvdo][statfield]
            modtrans[statfield] = vdokeytrans[statfield]

        # Build a dictionary of current parameters formatted with the
        # same keys as the AnsibleModule parameters.
        currentparams = {}
        for paramkey in currentvdoparams.keys():
            currentparams[modtrans[paramkey]] = currentvdoparams[paramkey]

        diffparams = {}

        # Check for differences between the playbook parameters and the
        # current parameters.  This will need a comparison function;
        # since AnsibleModule params are all strings, compare them as
        # strings (but if it's None; skip).
        for key in currentparams.keys():
            if module.params[key] is not None:
                if str(currentparams[key]) != module.params[key]:
                    diffparams[key] = module.params[key]

        result['currentparams'] = currentparams

        if diffparams:
            result['changed'] = True
            result['diffparams'] = diffparams
            vdocmdoptions = add_vdooptions(diffparams)
            result['vdocmdoptions'] = vdocmdoptions
            rc, _, err = module.run_command("%s modify --name=%s %s"
                                            % (vdo_cmd,
                                               desiredvdo,
                                               vdocmdoptions))
            if rc == 0:
                result['changed'] = True
            else:
                module.fail_json(msg="Modifying VDO %s failed."
                                 % desiredvdo, rc=rc, err=err)

            if 'deduplication' in diffparams.keys():
                dedupemod = diffparams['deduplication']
                if dedupemod == 'disabled':
                    result['dedupeopt'] = dedupemod
                    rc, _, err = module.run_command("%s disableDeduplication "
                                                    "--name=%s"
                                                    % (vdo_cmd, desiredvdo))

                if dedupemod == 'enabled':
                    result['dedupeopt'] = dedupemod
                    rc, _, err = module.run_command("%s enableDeduplication "
                                                    "--name=%s"
                                                    % (vdo_cmd, desiredvdo))

            if 'compression' in diffparams.keys():
                compressmod = diffparams['compression']
                if compressmod == 'disabled':
                    result['compressopt'] = compressmod
                    rc, _, err = module.run_command("%s disableCompression "
                                                    "--name=%s"
                                                    % (vdo_cmd, desiredvdo))

                if compressmod == 'enabled':
                    result['compressopt'] = compressmod
                    rc, _, err = module.run_command("%s enableCompression "
                                                    "--name=%s"
                                                    % (vdo_cmd, desiredvdo))

        # Process the size parameters, to determine of a growPhysical or
        # growLogical operation needs to occur.
        sizeparamkeys = ['Logical size', ]

        currentsizeparams = {}
        sizetrans = {}
        for statfield in sizeparamkeys:
            currentsizeparams[statfield] = processedvdos[desiredvdo][statfield]
            sizetrans[statfield] = vdokeytrans[statfield]

        sizeparams = {}
        for paramkey in currentsizeparams.keys():
            sizeparams[sizetrans[paramkey]] = currentsizeparams[paramkey]

        diffsizeparams = {}
        for key in sizeparams.keys():
            if module.params[key] is not None:
                if str(sizeparams[key]) != module.params[key]:
                    diffsizeparams[key] = module.params[key]

        result['currentsizeparams'] = currentsizeparams

        if module.params['growphysical'] == 'yes':
            result['diffsizeparams'] = diffsizeparams

            physdevice = module.params['device']
            rc, devsectors, err = module.run_command("blockdev --getsz %s"
                                                     % (physdevice))
            devblocks = (int(devsectors) / 8)
            result['devsize'] = devblocks

            dmvdoname = ('/dev/mapper/' + desiredvdo)
            currentvdostats = (processedvdos[desiredvdo]
                                            ['VDO statistics']
                                            [dmvdoname])
            currentphysblocks = currentvdostats['physical blocks']
            result['currentphysblocks'] = currentphysblocks

            # Set a growPhysical threshold to grow only when there is
            # guaranteed to be more than 2 slabs worth of unallocated
            # space on the device to use.  For now, set to device
            # size + 64 GB, since 32 GB is the largest possible
            # slab size.
            growthresh = devblocks + 16777216

            if currentphysblocks > growthresh:
                result['changed'] = True
                rc, _, err = module.run_command("%s growPhysical --name=%s"
                                                % (vdo_cmd, desiredvdo))
                if rc != 0:
                    result['growphysicalerr'] = err

        if 'logicalsize' in diffsizeparams.keys():
            result['changed'] = True
            result['diffsizeparams'] = diffsizeparams
            vdocmdoptions = ("--vdoLogicalSize=" +
                             diffsizeparams['logicalsize'])
            result['growlogicaloptions'] = vdocmdoptions
            rc, _, err = module.run_command("%s growLogical --name=%s %s"
                                            % (vdo_cmd,
                                               desiredvdo,
                                               vdocmdoptions))
            if rc != 0:
                result['growlogicalerr'] = err

        result['currentactivatestatus'] = processedvdos[desiredvdo]['Activate']
        vdoactivatestatus = processedvdos[desiredvdo]['Activate']

        if ((module.params['activated'] == 'no') and
                (vdoactivatestatus == 'enabled')):
            result['action_enable'] = 'disable'
            deactivate_vdo(module, desiredvdo, vdo_cmd)
            if not result['changed']:
                result['changed'] = True

        if ((module.params['activated'] == 'yes') and
                (vdoactivatestatus == 'disabled')):
            result['action_enable'] = 'enable'
            activate_vdo(module, desiredvdo, vdo_cmd)
            if not result['changed']:
                result['changed'] = True

        if ((module.params['running'] == 'no') and
                (desiredvdo in runningvdolist)):
            result['action_running'] = 'stop'
            stop_vdo(module, desiredvdo, vdo_cmd)
            if not result['changed']:
                result['changed'] = True

        # Note that a disabled VDO volume cannot be started by the
        # 'vdo start' command, by design.  To accurately track changed
        # status, don't try to start a disabled VDO volume.
        # If the playbook contains 'activated: yes', assume that
        # the activate_vdo() operation succeeded, as 'vdoactivatestatus'
        # will have the activated status prior to the activate_vdo()
        # call.
        if (((vdoactivatestatus == 'enabled') or
                (module.params['activated'] == 'yes')) and
                (module.params['running'] == 'yes') and
                (desiredvdo not in runningvdolist)):
            result['action_running'] = 'start'
            start_vdo(module, desiredvdo, vdo_cmd)
            if not result['changed']:
                result['changed'] = True

        # Print a post-run list of VDO volumes in the result object.
        vdolist = inventory_vdos(module, vdo_cmd)
        result['post_vdolist'] = vdolist
        module.exit_json(**result)

    # Remove a desired VDO that currently exists.
    if (desiredvdo in vdolist) and (state == 'absent'):
        rc, _, err = module.run_command("%s remove --name=%s"
                                        % (vdo_cmd, desiredvdo))
        if rc == 0:
            result['changed'] = True
        else:
            module.fail_json(msg="Removing VDO %s failed."
                             % desiredvdo, rc=rc, err=err)

        # Print a post-run list of VDO volumes in the result object.
        vdolist = inventory_vdos(module, vdo_cmd)
        result['post_vdolist'] = vdolist
        module.exit_json(**result)

    # The state for the desired VDO volume was absent, and it does not exist.
    # Print a post-run list of VDO volumes in the result object.
    vdolist = inventory_vdos(module, vdo_cmd)
    result['post_vdolist'] = vdolist
    result['desiredvdo_alreadyabsent'] = True

    module.exit_json(**result)


def main():
    run_module()

if __name__ == '__main__':
    main()
