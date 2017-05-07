#!/bin/ksh
# Open Firm Accounting
# A double-entry accounting application for professional services.
#
# Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
#
# Open Firm Accounting is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# Open Firm Accounting is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Open Firm Accounting; see the file COPYING. If not,
# see <http://www.gnu.org/licenses/>.
#
# Authors:
#   Pierre Wieser <pwieser@trychlos.org>

errs=0										# will be the exit code of the script
my_cmd="${0}"								# e.g. "./make-ks.sh"
my_parms="$*"								# e.g. "-host toaster"
my_cmdline="${my_cmd} ${my_parms}"
me="${my_cmd##*/}"							# e.g. "make-ks.sh"
											# used in msg and msgerr functions
my_tmproot="/tmp/$(echo ${me} | sed 's?\..*$??').$$"
											# e.g. "/tmp/make-ks.1978"

# These three functions must be defined using the name() syntax in order
# to share traps with the caller process (cf. man (1) ksh).
#
trap_exit()
{
	clear_tmpfiles
	#[ "${opt_verbose}" = "yes" -o ${errs} -gt 0 ] && msg "exiting with code ${errs}"
	msg "exiting with code ${errs}"
	exit ${errs}
}

trap_int()
{
	msg "quitting on keyboard interrupt"
	let errs+=1
	exit
}

trap_term()
{
	[ "${opt_verbose}" = "yes" ] && msg "quitting on TERM signal"
	exit
}

# setup the different trap functions
trap 'trap_term' TERM
trap 'trap_int'  INT
trap 'trap_exit' EXIT

function clear_tmpfiles
{
	\rm -f ${my_tmproot}.*
}

function msg
{
	typeset _eol="\n"
	[ $# -ge 2 ] && _eol="${2}"
	printf "[%s] %s${_eol}" ${me} "${1}"
	return 0
}

function msgerr
{
	msg "error: ${1}" 1>&2
	return $?
}

function msgwarn
{
	msg "warning: ${1}" 1>&2
	return $?
}

function msg_help
{
	msg_version
	echo "
 This script checks for POTFILES.in consistency and completeness.

 Usage: ${my_cmd} [options]
   --[no]help                print this message, and exit [${opt_help_def}]
   --[no]version             print script version, and exit [${opt_version_def}]
   --[no]dummy               dummy execution [${opt_dummy_def}]
   --[no]verbose             runs verbosely [${opt_verbose_def}]
   --potfile=filename        POTFILES.in to be checked [${opt_potfile_def}]"
}

function msg_version
{
	makefile="${top_srcdir}/_build/Makefile"
	pck_name=$(grep '^PACKAGE_NAME' ${makefile} 2>/dev/null | awk '{ print $3 }')
	pck_version=$(grep '^PACKAGE_VERSION' ${makefile} 2>/dev/null | awk '{ print $3 }')
	echo "
 ${pck_name} v ${pck_version}
 Copyright (C) 2017 Pierre Wieser."
}

# initialize common command-line options
nbopt=$#
opt_help=
opt_help_def="no"
opt_dummy=
opt_dummy_def="yes"
opt_version=
opt_version_def="no"
opt_verbose=
opt_verbose_def="no"

# a first loop over command line arguments to detect verbose mode
while :
do
	# break when all arguments have been read
	case $# in
		0)
			break
			;;
	esac

	# get and try to interpret the next argument
	_option=$1
	shift

	# make all options have two hyphens
	_orig_option=${_option}
	case ${_option} in
		--*)
			;;
		-*)
			_option=-${_option}
				;;
		esac

	# now process options and their argument
	case ${_option} in
		--noverb | --noverbo | --noverbos | --noverbose)
			opt_verbose="no"
			;;
		--verb | --verbo | --verbos | --verbose)
			opt_verbose="yes"
				;;
	esac
done

[ "${opt_verbose}" = "yes" ] && msg "setting opt_verbose to 'yes'"

# we have scanned all command-line arguments in order to detect an
# opt_verbose option;
# reset now arguments so that they can be scanned again in main script
set -- ${my_parms}

# interpreting command-line arguments
opt_potfile=
opt_potfile_def="po/POTFILES.in"

# loop over command line arguments
pos=0
while :
do
	# break when all arguments have been read
	case $# in
		0)
			break
			;;
	esac

	# get and try to interpret the next argument
	option=$1
	shift

	# make all options have two hyphens
	orig_option=${option}
	case ${option} in
		--*)
			;;
		-*)
			option=-${option}
			;;
	esac

	# split and extract argument for options that take one
	case ${option} in
		--*=*)
			optarg=$(echo ${option} | sed -e 's/^[^=]*=//')
			option=$(echo ${option} | sed 's/=.*//')
			;;
		# these options take a mandatory argument
		# since, we didn't find it in 'option', so it should be
		# next word in the command line
		--p | --po | --pot | --potf | --potfi | --potfil | --potfile)
			optarg=$1
			shift
			;;
		esac

	# now process options and their argument
	case ${option} in
		--d | --du | --dum | --dumm | --dummy)
			[ "${opt_verbose}" = "yes" ] && msg "setting opt_dummy to 'yes'"
			opt_dummy="yes"
			;;
		--h | --he | --hel | --help)
			[ "${opt_verbose}" = "yes" ] && msg "setting opt_help to 'yes'"
			opt_help="yes"
			;;
		--nod | --nodu | --nodum | --nodumm | --nodummy)
			[ "${opt_verbose}" = "yes" ] && msg "setting opt_dummy to 'no'"
			opt_dummy="no"
			;;
		--noh | --nohe | --nohel | --nohelp)
			[ "${opt_verbose}" = "yes" ] && msg "setting opt_help to 'no'"
			opt_help="no"
			;;
		--noverb | --noverbo | --noverbos | --noverbose)
			;;
		--novers | --noversi | --noversio | --noversion)
			[ "${opt_verbose}" = "yes" ] && msg "setting opt_version to 'no'"
			opt_version="no"
			;;
		--p | --po | --pot | --potf | --potfi | --potfil | --potfile)
			[ "${opt_verbose}" = "yes" ] && msg "setting opt_potfile to '${optarg}'"
			opt_potfile="${optarg}"
			;;
		--verb | --verbo | --verbos | --verbose)
			;;
		--vers | --versi | --versio | --version)
			[ "${opt_verbose}" = "yes" ] && msg "setting opt_version to 'yes'"
			opt_version="yes"
			;;
		--*)
			msgerr "unrecognized option: '${orig_option}'"
			let errs+=1
			;;
		# positional parameters
		*)
			let pos+=1
			#if [ ${pos} -eq 1 ]; then
			#	[ "${opt_verbose}" = "yes" ] && msg "setting opt_output to '${option}'"
			#	opt_output=${option}
			#else
				msgerr "unexpected positional parameter #${pos}: '${option}'"
				let errs+=1
			#fi
			;;
	esac
done

# set option defaults
# does not work with /bin/sh ??
#set | grep -e '^opt_' | cut -d= -f1 | while read _name; do
#	if [ "$(echo ${_name} | sed 's/.*\(_def\)/\1/')" != "_def" ]; then
#		_value="$(eval echo "$"${_name})"
#		if [ "${_value}" = "" ]; then
#			eval ${_name}="$(eval echo "$"${_name}_def)"
#		fi
#	fi
#done

opt_help=${opt_help:-${opt_help_def}}
opt_dummy=${opt_dummy:-${opt_dummy_def}}
opt_verbose=${opt_verbose:-${opt_verbose_def}}
opt_version=${opt_version:-${opt_version_def}}

opt_potfile=${opt_potfile:-${opt_potfile_def}}

# check that we are running from the top of srcdir
maintainer_dir=$(cd ${0%/*}; pwd)
top_srcdir="${maintainer_dir%/*}"
if [ ! -f "${top_srcdir}/configure.ac" ]; then
	msgerr "this script is only meant to be run by the maintainer,"
	msgerr "and current working directory should be the top source directory."
	let errs+=1
	exit
fi

if [ "${opt_help}" = "yes" -o ${nbopt} -eq 0 ]; then
	msg_help
	echo ""
	exit
fi

if [ "${opt_version}" = "yes" ]; then
	msg_version
	echo ""
	exit
fi

if [ ! -r "${top_srcdir}/${opt_potfile}" ]; then
	msgerr "${opt_potfile}: file not found or not readable"
	let errs+=1
fi

if [ ${errs} -gt 0 ]; then
	msg "${errs} error(s) have been detected"
	msg "try '${my_cmd} --help' for usage"
	exit
fi

# ---------------------------------------------------------------------
# MAIN CODE

totpass=5
nbpass=0
cd "${top_srcdir}"

# first, check that all .ui are in po/POTFILE.in
let nbpass+=1
nbfiles=0
nberrs=0
msg "pass ${nbpass}/${totpass}: checking that all .ui are in ${opt_potfile}..."
for f in $(git ls-files *.ui); do
	#if [ "$(grep -xe "\[type:\s*gettext/glade]\s*${f}" ${opt_potfile})" = "" ]; then
	if [ "$(grep -xe "\s*${f}\s*$" ${opt_potfile})" = "" ]; then
		msg "  ${f} should be added to ${opt_potfile}"
		let nberrs+=1
	elif [ "${opt_verbose}" = "yes" ]; then
		msg "  ${f}: OK"
	fi
	let nbfiles+=1
done
msg "  nbfiles=${nbfiles} error(s)=${nberrs}"
let errs+=${nberrs}

# second, check that all .ui in PO exist
let nbpass+=1
nbfiles=0
nberrs=0
msg "pass ${nbpass}/${totpass}: checking that all .ui from ${opt_potfile} actually exist..."
for f in $(grep -e '\.ui$' "${top_srcdir}/${opt_potfile}" | sed 's,\[type:\s*gettext/glade]\s*,,'); do
	if [ ! -r "${top_srcdir}/${f}" ]; then
		msg "  ${f} should be removed from ${opt_potfile}"
		let nberrs+=1
	elif [ "${opt_verbose}" = "yes" ]; then
		msg "  ${f}: OK"
	fi
	let nbfiles+=1
done
msg "  nbfiles=${nbfiles} error(s)=${nberrs}"
let errs+=${nberrs}

# third, check that all files which use _( construct are in PO
let nbpass+=1
nbfiles=0
nberrs=0
msg "pass ${nbpass}/${totpass}: checking that all translatable files are in ${opt_potfile}..."
for f in $(git grep -I '_(' src | cut -d: -f1 | sort -u); do
	if [ "$(grep -x ${f} ${opt_potfile})" != "${f}" ]; then
		msg "  ${f} should be added to ${opt_potfile}"
		let nberrs+=1
	elif [ "${opt_verbose}" = "yes" ]; then
		msg "  ${f}: OK"
	fi
	let nbfiles+=1
done
msg "  nbfiles=${nbfiles} error(s)=${nberrs}"
let errs+=${nberrs}

# fourth, check that all files in PO actually use the _( construct
let nbpass+=1
nbfiles=0
nberrs=0
msg "pass ${nbpass}/${totpass}: checking that all files in ${opt_potfile} actually use the '_(' construct..."
for f in $(grep -E '^src/' "${top_srcdir}/${opt_potfile}" | grep -vE '\.ui$' | grep -vE '\.in$'); do
	grep '_(' "${top_srcdir}/${f}" 1>/dev/null 2>&1
	if [ $? -ne 0 ]; then
		msg "  ${f} should be removed from ${opt_potfile}"
		let nberrs+=1
	elif [ "${opt_verbose}" = "yes" ]; then
		msg "  ${f}: OK"
	fi
	let nbfiles+=1
done
msg "  nbfiles=${nbfiles} error(s)=${nberrs}"
let errs+=${nberrs}

# last, check that all files which include gi18n.h are relevant
let nbpass+=1
nbfiles=0
nberrs=0
msg "pass ${nbpass}/${totpass}: checking that all files have a good reason to include gi18n.h..."
for f in $(git grep '#include <glib/gi18n.h>' src | cut -d: -f1 | sort -u); do
	grep '_(' ${f} 1>/dev/null 2>&1
	if [ $? -ne 0 ]; then
		msg "  ${f} should not include <glib/gi18n.h>"
		let nberrs+=1
	elif [ "${opt_verbose}" = "yes" ]; then
		msg "  ${f}: OK"
	fi
	let nbfiles+=1
done
msg "  nbfiles=${nbfiles} error(s)=${nberrs}"
let errs+=${nberrs}

exit
