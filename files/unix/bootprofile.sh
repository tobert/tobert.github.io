#!/bin/sh
####################################################################
#
# bootprofile: Allow users to select distinct bootprofile based on
#             desired networked environment  on Linux  at boot time.
#
####################################################################
#    Copyright (C) 2002  Rajeev Kumar
#    Shell Version Copyright (C) 2003  Al Tobey
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330,
#    Boston, MA  02111-1307  USA
#
# Rajeev Kumar (rajeev@rajeevnet.com)
# Albert P Tobey (tobeya@tobert.org)
###################################################################
#
#DISCLAIMER:
#   THIS SOFTWARE RUNS AS A ROOT AT A VERY EARLY STAGE OF LINUX BOOT.
#   IMPROPER USE OF THIS SOFTWARE MAY CAUSE BOOT PROBLEMS OR  OTHER
#   POSSIBLE DAMAGES TO YOUR SYSTEM. USE THIS SOFTWARE AT YOUR OWN
#   RISK. AUTHOR ASSUMES NO RESPONSIBILITY FOR ANY DAMAGE(S) CAUSED
#   BY THE USE OF THIS SOFTWARE.
###################################################################

# ---------------------------------------------------------------------------- #
# FUNCTIONS
# ---------------------------------------------------------------------------- #

function doprofile {
	prof=$1
	exec 5</etc/bootprofile/$prof/$prof.profile

	while read this_line <&5
	do
		# remove comments
		this_line=`echo $this_line |sed 's/#.*$//'`

		# skip blank lines
		if [ -z "$this_line" ] ; then
			continue
		fi

		# find sections
		if (echo $this_line |grep -qE '^\[') ; then
			section=`echo $this_line |sed 's/^\[//' |sed 's/]$//'`
			continue
		fi

		case $section in
			chkconfig)
				stt=`echo $this_line |awk '{print $3}'`
				svc=`echo $this_line |awk '{print $2}'`
				lvl=`echo $this_line |awk '{print $1}'`
				if [ -z "$stt" -o -z "$svc" -o -z "$lvl" ] ; then
					echo "invalid entry for chkconfig '$this_line'"
					exit 1
				fi
				if [ -n "$VERBOSE" ] ; then
					echo "/sbin/chkconfig --level $lvl $svc $stt"
				fi
				if [ -n "$EXECUTE" ] ; then 
					/sbin/chkconfig --level $lvl $svc $stt
				fi
				;;
			files)
				file=`echo $this_line |awk '{print $1}'`
				dest=`echo $this_line |awk '{print $2}'`
				if [ -n "$VERBOSE" ] ; then
					echo "cp -f /etc/bootprofile/$prof/$file $dest"
				fi
				if [ -n "$EXECUTE" ] ; then 
					cp -f /etc/bootprofile/$prof/$file $dest
				fi
				;;
			links)
				file=`echo $this_line |awk '{print $1}'`
				dest=`echo $this_line |awk '{print $2}'`
				if [ -n "$VERBOSE" ] ; then
					echo "rm -f $dest"
					echo "ln -s $file $dest"
				fi
				if [ -n "$EXECUTE" ] ; then 
					rm -f $dest
					ln -s $file $dest
				fi
				;;
			scripts)
				if [ -n "$VERBOSE" ] ; then
					echo "sh -c \"$this_line\""
				fi
				if [ -n "$EXECUTE" ] ; then 
					sh -c "$this_line"
				fi
				;;
			remove)
				if [ -n "$VERBOSE" ] ; then
					echo "rm -f $this_line"
				fi
				if [ -n "$EXECUTE" ] ; then 
					rm -f $this_line
				fi
				;;
			*)
				echo "no section ..."
				break;
				;;
		esac
		unset lvl svc stt file dest
	done
}

function timeout {
	sh -s <<EOCMD
		counter=0
		while (( $TIMEOUT>\$counter ))
		do
			sleep 1
			counter=\$(( \$counter + 1 ))
			(ps -e |grep -qE '^\\ *$$.*bootprofile.sh\$') || exit 0
		done
		echo "Time is up ... booting with previous profile."
		if ( ps -e |grep -qE '^\\ *$$.*bootprofile.sh\$' ) ; then
			kill $$
		fi
EOCMD
}

function mkline {
	echo "--------------------------------------------------------------------------------"
}

# ---------------------------------------------------------------------------- #
# MAIN
#  --------------------------------------------------------------------------- #

[ ! -f /etc/sysconfig/bootprofile ] && exit 1
. /etc/sysconfig/bootprofile

# parse options
while [ $# -ne 0 ]
do
	case $1 in
		--execute)
			EXECUTE=1
			shift
			;;
		--test)
			unset EXECUTE
			VERBOSE=1
			shift
			;;
		--verbose)
			VERBOSE=1
			shift
			;;
		--nocmdline)
			NOCMDLINE=1
			shift
			;;
		*)
			;;
	esac
done

# check to make sure bootprofile is enabled
[ -z "$BOOTPROFILE" -o "$BOOTPROFILE" != "ON" ] && exit 0

# start the timeout a'tickin'
timeout &

if [ -z "$NOCMDLINE" ] ; then
	if ( grep -q 'bootprofile=' /proc/cmdline ) ; then
		profile=`tr ' ' '\n' </proc/cmdline |awk -F '=' '/bootprofile/{print $2}'`
		if [ -f "/etc/bootprofile/$profile/$profile.profile" ] ; then
			echo "profile $profile chosen on kernel command line"
			doprofile $profile
			exit 0
		fi
	fi
fi

for dir in /etc/bootprofile/*
do
	base=`basename $dir`
	if [ -d $dir -a -f "$dir/$base.profile" ] ; then
		profiles="$profiles $base"
	fi
done

mkline
mkline
select profile in $profiles
do
	mkline
	if [ -f "/etc/bootprofile/$profile/$profile.profile" ] ; then
		echo "Profile \"$profile\" selected ..." ; echo
		doprofile $profile
		break
	fi
	# keep prompting until either timeout or a correct answer
done
mkline
mkline

exit 0

# ---------------------------------------------------------------------------- #
# EOF
# ---------------------------------------------------------------------------- #

