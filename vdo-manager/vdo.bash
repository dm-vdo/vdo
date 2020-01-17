# bash completion for vdostats
#
# Copyright (c) 2020 Red Hat, Inc.
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

# If this needs to be modified where the location of confFile would be
# manually provided, we should be able to read from that file

CONF_FILE=/etc/vdoconf.yml

# This function will tokenize the INPUT that is given by the function
#  _parse_vdo_options(). It will output all the options that have
# two "--" in the beginning.

__parse_vdo_options()
{
  local option option2 i IFS=',/|';
  option=;
  local -a array;
  if [[ $1 =~ --[A-Za-z0-9]+ ]] ; then
    read -a array <<< "${BASH_REMATCH[0]}"
  fi
  for i in "${array[@]}";
  do
    case "$i" in
      ---*)
      break
      ;;
      --?*)
      option=$i;
      break
      ;;
      -?*)
      [[ -n $option ]] || option=$i
      ;;
      *)
      break
      ;;
    esac;
  done;
  [[ -n $option ]] || return;
  IFS='
  ';
  if [[ $option =~ (\[((no|dont)-?)\]). ]]; then
    option2=${option/"${BASH_REMATCH[1]}"/};
    option2=${option2%%[<{().[]*};
    printf '%s\n' "${option2/=*/=}";
    option=${option/"${BASH_REMATCH[1]}"/"${BASH_REMATCH[2]}"};
  fi;
  option=${option%%[<{().[]*};
  printf '%s\n' "${option/=*/=}"
}

# This function does the hardwork of doing the following: 
# let us assume you're running vdo activate 
# It executes "vdo activate --help" and gets a list of possible 
# options and then tokenizes them.

_parse_vdo_options()
{
  eval local cmd=$( quote "$1" );
  local line;
  {
    case $cmd in
      -)
      cat
      ;;
      *)
      LC_ALL=C "$( dequote "$cmd" )" $2 --help 2>&1
      ;;
    esac
  }| while read -r line; do
    [[ $line == *([[:blank:]])-* ]] || continue;
    while [[ $line =~ \
((^|[^-])-[A-Za-z0-9?][[:space:]]+)\[?[A-Z0-9]+\]? ]]; do
      line=${line/"${BASH_REMATCH[0]}"/"${BASH_REMATCH[1]}"};
    done;
    __parse_vdo_options "${line// or /, }";
  done
}

# _vdo_devdir will give the list of devices that can be used
# for vdo 

_vdo_devdir()
{
  local cur prev words cword options
  _init_completion || return
  COMPREPLY=( $( compgen -W "$(lsblk -pnro name)" -- "$cur" ))
}

# We parse the configuration file to understand the names of vdo
# which are present. This may generate false positive if the 
# conf file has names of vdo devices but they're not actually
# present in the system 

_vdo_names()
{
  local cur prev words cword options
  _init_completion || return
  if [ ! -f $CONF_FILE ]; then
    return
  fi
  names=()
  while IFS= read -r line
  do
    if [[ $line =~ \!VDOService ]]; then
      names+=( $(echo $line | cut -d: -f1) )
    fi
  done < $CONF_FILE
  COMPREPLY=( $( compgen -W  " ${names[*]}"  -- "$cur"))
}


_generic_function()
{

  local cur prev words cword options
  _init_completion || return

# We check for filename completion before after the first compreply.
# Otherwise the options of COMPREPLY and _filedir will be mixed.
  case "${prev}" in
	-f|--confFile)
# since the configuration file has the extention yml, we only display
# the files with the suffix yml.
		_filedir yml
		return
	;;
	--logfile)
		_filedir
		return
	;;
  esac

  COMPREPLY=( $( compgen -W '$( _parse_vdo_options vdo $1 )' -- "$cur" ) )
  case "${prev}" in
    -n|--name)
        if [[ "$1" == "create" ]]
        then
          return
        else
          _vdo_names
        fi
    ;;
    --writePolicy)
    COMPREPLY=( $( compgen -W 'sync async auto' -- "$cur" ) )
    ;;
    --activate|--compression|--deduplication|--emulate512|--sparseIndex)
    COMPREPLY=( $( compgen -W 'disabled enabled' -- "$cur" ) )
    ;;
    --vdoLogLevel)
    COMPREPLY=( $( compgen -W \
'critical error warning notice info debug' -- "$cur" ) )
    ;;
    --device)
      _vdo_devdir
    ;;

  esac
  return
}

_vdo()
{
  local cur prev words cword
  _init_completion || return

  if [[ $cword -eq 1 ]]; then
    COMPREPLY=( $( compgen -W '
    activate changeWritePolicy create deactivate disableCompression
    disableDeduplication enableCompression enableDeduplication growLogical
    growPhysical list modify printConfigFile remove start status
	stop' -- "$cur" ) )
  else
    case "${words[1]}" in
      activate|changeWritePolicy|create|deactivate|disableCompression|\
      disableDeduplication|enableCompression|enableDeduplication|\
      growLogical|growPhysical|list|modify|printConfigFile|remove|\
      start|status|stop)
	   _generic_function ${words[1]}
      ;;
    esac
  fi
} &&

complete -F _vdo vdo
