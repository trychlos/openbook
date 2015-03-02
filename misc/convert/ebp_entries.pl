#!/usr/bin/perl
# 

use locale;
use strict;
use warnings;
use File::Basename;
use Getopt::Long;
use POSIX qw/strftime/;
use utf8;

my $me = basename( $0 );
my $errs = 0;
my $ftmproot = "/tmp/$me.$$";
my $nbopts = scalar( @ARGV );

my $opt_help_def = "no";
my $opt_help = 0;
my $opt_verbose_def = "no";
my $opt_verbose = 0;
my $opt_stamp_def = "yes";
my $opt_stamp = 1;

# ---------------------------------------------------------------------
# signal handlers

# doesn't clean temp files if there is an error in order to keep a debug
# trace 

END {
	msg( "exiting with code $errs" ) if $opt_verbose || $errs;
	exit( $errs );
}

sub int_handler {
	msg( "quitting on keyboard interrupt..." ) if $opt_verbose;
	$errs += 1;
	exit;
}

sub term_handler {
	msg( "quitting on TERM signal" ) if $opt_verbose;
	exit;
}

# ---------------------------------------------------------------------
# remove temp files
# pwi 2012-12-27 not used as is; see File::Temp

sub clear_tmpfiles
{
	if( $ftmproot ){
		my @temps = <$ftmproot.*>;
		foreach my $tmpf ( @temps ){
			msg( "clearing $tmpf temp file" ) if $opt_verbose;
			unlink( $tmpf );
		}
	}
}

# ---------------------------------------------------------------------
# display a message on stdout
#
# (E): 1. string to be displayed
#      2. (fac.) string to be displayed at then end of line, instead of '\n';

sub msg
{
	my $msg = shift;
	my $eol = "\n";
	$eol = shift if scalar( @_ );
	msgprint( $msg, $eol, \*STDOUT );
}

# ---------------------------------------------------------------------
# display an error message on stderr
#
# (E): 1. string to be displayed
#      2. (fac.) string to be displayed at then end of line, instead of '\n';

sub msgerr
{
	my $msg = shift;
	my $eol = "\n";
	$eol = shift if scalar( @_ );
	msgprint( "error: $msg", $eol, \*STDERR );
}

# ---------------------------------------------------------------------
# display a message
#
# (E): 1. string to be displayed
#      2. string to be displayed at then end of line, instead of '\n';
#      3. output stream

sub msgprint
{
	my $msg = shift;
	my $eol = shift;
	my $fd = shift;
	my $str = "[";
	$str .= strftime("%Y-%m-%d %H:%M:%S", localtime)." " if $opt_stamp;
	$str .= "$me] " if $me;
	$str .= $msg;
	$str .= $eol;
	print $fd $str;
}

my $my_brief = "Converts Entries from EBP to Openbook csv format";

my $debug;

sub msg_help(){
	msg_version();
	print "
 $my_brief

 Usage: $0 [options] < 'ebp_file' > 'openbook_file'
   --[no]help                 print this message, and exit [${opt_help_def}]
   --[no]stamp                display messages with a timestamp [${opt_stamp_def}]
";
}

sub msg_version(){
	print "
 $my_version
 Copyright (C) 2013 Pierre Wieser.
";
}

if( !GetOptions(
	"help!"				=> \$opt_help,
	"stamp!"			=> \$opt_stamp
	)){
		msg "try '${0} --help' to get full usage syntax\n";
		$errs = 1;
		exit;
}

#print "nbopts=$nbopts\n";
#$opt_help = 1 if $nbopts == 0;

if( $opt_help ){
	msg_help();
	print "\n";
	exit;
}

# ---------------------------------------------------------------------
# convert dd/mm/yyyy date to yyyy-mm-dd
#
# (E): 1. date to be converted

sub convert_date
{
	my $date = shift;
	my $out = "";
	if( 0+substr( $date, 0, 2 ) > 0 ){
		$out = substr( $date, 6, 4 );
		$out .= "-" . substr( $date, 3, 2 );
		$out .= "-" . substr( $date, 0, 2 );
	}
	return( $out );
}

# ---------------------------------------------------------------------
# convert an amount with a comma as a decimal separator to a dot
#
# (E): 1. amount to be converted as a string

sub convert_amount
{
	my $amount = shift;
	my $out = $amount;
	$out =~ s/,/./;
	return( $out );
}

# ---------------------------------------------------------------------
# map input to output
#
# (E): 1. string to be converted

sub mapping
{
	my $inline = shift;
	my @infields = split /;/, $inline;
	my $dope = convert_date( $infields[3] );
	my $deff = convert_date( $infields[9] );
	my $label = $infields[13];
	my $ref = $infields[11];
	# curency is not set
	my $ledger = $infields[1];
	# operation template is not set
	my $account = $infields[2];
	my $debit = convert_amount( $infields[14] );
	my $credit = convert_amount( $infields[15] );
	my $settlement = $infields[28] eq "oui" ? "True" : "";
	my $reconciliation = convert_date( $infields[7] );
	
	return( join( ';', $dope, $deff, $label, $ref, "", $ledger, "", $account, $debit, $credit, $settlement, "", "", $reconciliation, "", "" ));
}

###
### MAIN
###

binmode STDIN, ':encoding(iso-8859-15)';
binmode STDOUT, ":utf8";
my $count = 0;

while( <> ){
	$count += 1;
	if( $count == 1 ){
		print "DOpe;DEffect;Label;Ref;Currency;Ledger;OpeTemplate;Account;Debit;Credit;Settlement;Reconciliation\n";
	} else {
		print mapping( $_ )."\n";
	}
}

my $nb = $count-1;
print STDERR "$nb converted entries\n";
