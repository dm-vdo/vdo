This directory contains example playbooks for the "vdo" module in Ansible,
which can be used to create and remove VDO volumes, modify configuration
options, and start and stop VDO volumes.

The Ansible "vdo" module can be found in Ansible, version 2.5 or greater.
General information on Ansible can be found at http://docs.ansible.com/

To run a playbook:

1. On the control system, set up a passwordless SSH session via the
   "ssh-agent bash" and "ssh-add" commands.

2. Specify the managed hosts and groups by adding them to the
   /etc/ansible/hosts file on the control system.

3. Using the "hosts" value, construct a playbook detailing the desired state
   of the VDO volume.

4. When ready, execute "ansible-playbook <playbook.yml>".  For additional
   diagnostic output, run "ansible-playbook <playbook.yml> -vvvv".
