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

my $opt_fieldsep_def = ";";
my $opt_fieldsep = $opt_fieldsep_def;
my $opt_account_def = "";
my $opt_account = $opt_account_def;
my $conv_accounts = {};
my $opt_ledger_def = "";
my $opt_ledger = $opt_ledger_def;
my $conv_ledgers = {};

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

# ---------------------------------------------------------------------
# read a conversion file into a hash and returns it
#   Conversion file are very sample:
#   first all after the first sharp of the line is supposed to be a comment
#   empty lines are ignored
#   conversion must be specified as two spaces-separated values as in
#   <old_value>  <new_value> 
#
# (E): 1. input filename

sub read_convert_file
{
	my $infile = shift;
	my $res = {};
	my $fh;
	my $count = 0;
	if( ! -r $infile ){
		msgerr "$infile: file not found or not readable";
		$errs += 1;
	} elsif( !open( $fh, "<$infile" )){
		msgerr "$infile: $!";
		$errs += 1;
	} else {
		my $line;
		while( <$fh> ){
			chomp;
			$line = $_;
			$line =~ s/#.*//;
			if( $line ne "" ){
				my ( $acc_src, $acc_dest ) = split /\s+/, $line;
				if( $acc_src ne "" && $acc_dest ne "" ){
					$res->{$acc_src} = $acc_dest;
					$count += 1;
				}
			}
		}
		close( $fh );
		print STDERR "$count conversion lines read from $infile\n";
	}
	return $res;
}

# ---------------------------------------------------------------------
# convert a data throught the external conversion file if any
#
# (E): 1. ref to the hash which holds the conversion data
#      2. data value to be converted

sub apply_convert_file
{
	my $ref = shift;
	my $value = shift;
	my $out = $value;
	if( $value ne "" && defined( $ref->{$value} )){
		$out = $ref->{$value};
		$out = $value if $out eq "";
	}
	return( $out );
}

my $my_brief = "Converts EBP entries to Openbook csv format";
my $my_version = "0.39";

my $debug;

sub msg_help(){
	msg_version();
	print "
 $my_brief

 Usage: $0 [options] < 'ebp_file' > 'openbook_file'
   --[no]help                 print this message, and exit [${opt_help_def}]
   --[no]stamp                display messages with a timestamp [${opt_stamp_def}]
   --fieldsep=<char>          field separator [${opt_fieldsep_def}]
   --accounts=<path>          use an account conversion file [${opt_account_def}]
   --ledgers=<path>           use an ledger conversion file [${opt_ledger_def}]
";
}

sub msg_version(){
	print "
 $my_version
 Copyright (C) 2013,2014,2015 Pierre Wieser.
";
}

if( !GetOptions(
	"help!"				=> \$opt_help,
	"stamp!"			=> \$opt_stamp,
	"fieldsep=s"		=> \$opt_fieldsep,
	"account=s"			=> \$opt_account,
	"ledger=s"			=> \$opt_ledger
	)){
		msg "try '${0} --help' to get full usage syntax\n";
		$errs = 1;
		exit;
}

if( !defined( $opt_fieldsep ) || $opt_fieldsep eq "" ){
	msgerr "field separator not set or empty";
	$errs += 1;
}

$conv_accounts = read_convert_file( $opt_account ) if $opt_account ne "";
$conv_ledgers = read_convert_file( $opt_ledger ) if $opt_ledger ne "";

if( $errs ){
	msg "try '${0} --help' to get full usage syntax\n";
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
	my $ledger = apply_convert_file( $conv_ledgers, $infields[1] );
	# operation template is not set
	my $account = apply_convert_file( $conv_accounts, $infields[2] );
	my $debit = convert_amount( $infields[14] );
	my $credit = convert_amount( $infields[15] );
	my $settlement = $infields[28] eq "oui" ? "True" : "";
	my $number = $infields[0];
	my $dcreation = convert_date( $infields[8] );
	my $reconciliation = convert_date( $infields[7] );

	return( join( substr( $opt_fieldsep, 0, 1 ),
		 $dope, $deff, $label, $ref, "", $ledger, "", $account, $debit, $credit,
		 $settlement, "", "", $number, "", "", $dcreation, $reconciliation, "", "" ));
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
		print ""
		."DOpe;DEffect;Label;Ref;Currency;Ledger;OpeTemplate;Account;Debit;Credit;"
		."Settlement;SettlementUser;SettlementStamp;Number;Status;CreationUser;CreationStamp;"
		."ReconciliationDval;ReconciliationUser;ReconciliationStamp\n";
	} else {
		print mapping( $_ )."\n";
	}
}

my $nb = $count-1;
print STDERR "$nb converted entries\n";
