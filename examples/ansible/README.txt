This directory contains an Ansible module that can create and remove VDO
volumes, modify configuration options, and start and stop VDO volumes.

General information on Ansible can be found at http://docs.ansible.com/

To install this module:

1. Install Ansible (via a package manager).

2. Install the "vdo.py" module file by copying it into the site-packages
   subdirectory for Ansible.

   Example for RHEL 7.3:
   /usr/lib/python2.7/site-packages/ansible/modules/extras/system/vdo.py

   Example for RHEL 7.4:
   /usr/lib/python2.7/site-packages/ansible/modules/system/vdo.py

   Install the Ansible module on a "control system" to send commands, and
   each "managed node" system to receive commands (these systems will have
   VDO installed).


To run a playbook:

1. On the control system, set up a passwordless SSH session via the
   "ssh-agent bash" and "ssh-add" commands.

2. Specify the managed hosts and groups by adding them to the
   /etc/ansible/hosts file on the control system.

3. Using the "hosts" value, construct a playbook detailing the desired state
   of the VDO volume.

4. When ready, execute "ansible-playbook <playbook.yml>".  For additional
   diagnostic output, run "ansible-playbook <playbook.yml> -vvvv".
