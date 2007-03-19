#! /usr/bin/env - perl

sub gen_type
{
    my($subr, $arg_str) = @_;
    my(@args);
    print REVP "\ntypedef struct _${subr}_request {\n";
    print REVP "    int condition_var;\n";
    @args = split( ", ",  $arg_str,2);
    foreach $arg (split (", ", $args[1])) {
	$_ = $arg;
	if (/^\s*(.*\W+)(\w+)$\s*/) {
	    $argtype = $1;
	    $argname = $2;
	    $argtype =~ s/\s+$//;
	    $argtype =~ s/(?!\w)\s+(?=\W)//;  #remove unnecessary white space
	    $argtype =~ s/(?!\W)\s+(?=\w)//;  #remove unnecessary white space
	    $iotype = $argtype;
	    $sizetype = $argtype;
	  switch:for ($argtype) {
	      /attr_list/ && do {$iotype = "string"; $argtype="char*"; last;};
	      /char*/ && do {$iotype = "string"; $argtype="char*"; last;};
	      /EVstone$/ && do {$iotype = "integer"; $argtype="EVstone"; last;};
	      /EVstone\*/ && do {print REVP "    int ${argname}_len;\n";
$iotype = "integer[${argname}_len]"; $argtype="int *"; last;};
	  }
	}
	print REVP "    $argtype $argname;\n";
    }
    print REVP "} ${subr}_request;\n";
    $ret_type = $return_type{$subr};
  switch:  for ($ret_type) {
      /attr_list/ && do {$retiotype = "string"; $ret_type="char*"; last;};
      /char*/ && do {$retiotype = "string"; $ret_type="char*"; last;};
      /EVstone/ && do {$retiotype = "integer"; $ret_type="EVstone"; last;};
    }
    print REVP "\ntypedef struct _${subr}_response {\n";
    print REVP "    int condition_var;\n";
    print REVP "    $ret_type ret;\n" unless ($return_type{$subr} eq "void");
    print REVP "} ${subr}_response;\n";
}

sub gen_field_list
{
    my($subr, $arg_str) = @_;
    my(@args);
    print REVP "\nIOField  ${subr}_req_flds[] = {\n";
    print REVP "    {\"condition_var\", \"integer\", sizeof(int), IOOffset(${subr}_request*, condition_var)},\n";
    @args = split( ", ",  $arg_str,2);
    foreach $arg (split (", ", $args[1])) {
	$_ = $arg;
	if (/^\s*(.*\W+)(\w+)$\s*/) {
	    $argtype = $1;
	    $argname = $2;
	    $argtype =~ s/\s+$//;
	    $argtype =~ s/(?!\w)\s+(?=\W)//;  #remove unnecessary white space
	    $argtype =~ s/(?!\W)\s+(?=\w)//;  #remove unnecessary white space
	    $iotype = $argtype;
	    $sizetype = $argtype;
	  switch:for ($argtype) {
	      /attr_list/ && do {$iotype = "string"; $argtype="char*"; last;};
	      /char*/ && do {$iotype = "string"; $argtype="char*"; last;};
	      /EVstone/ && do {$iotype = "integer"; $argtype="EVstone"; last;};
	  }
	}
	print REVP "    {\"$argname\", \"$iotype\", sizeof($sizetype), IOOffset(${subr}_request*,$argname)},\n";
    }
    print REVP "};\n";
}

sub gen_stub {
    my($subr, $arg_str) = @_;
    my(@args);
    @args = split( ", ",  $arg_str,2);
    print REVP "\nextern $return_type{$subr}\n";
    if ($#args > 0) {
	print REVP "R$subr(CMConnection conn, $args[1])\n";
    } else {
	print REVP "R$subr(CMConnection conn)\n";
    }
    print REVP "{\n";
    undef $cmanager;
    undef $cmconnection;
    undef $evsource;
    undef $cmtaskhandle;
    undef $cmformat;
    foreach $arg (split ( ",", $arg_str)) {
	$_ = $arg;
	if (/\W+(\w+)\W*$/) {
	    $name = $1;
	}
	if (/CManager/) {
	    $cmanager = $name;
	}
	if (/CMConnection/) {
	    $cmconnection = $name;
	}
	if (/EVsource/) {
	    $evsource = $name;
	}
	if (/CMTaskHandle/) {
	    $cmtaskhandle = $name;
	}
	if ((/CMFormat/) && (!/CMFormatList/)){
	    $cmformat = $name;
	}
    }
    
    $_ = $return_type{$subr};
    if (/^\s*void\s*$/) {
	$return_type{$subr} = "void";
    }
    $retsubtype = $return_type{$subr};
  switch:  for ($ret_type) {
      /attr_list/ && do {$retsubtype = "string"; $ret_type="char*"; last;};
      /char*/ && do {$retsubtype = "string"; $ret_type="char*"; last;};
      /EVstone/ && do {$retsubtype = "int"; $ret_type="EVstone"; last;};
      /EVaction/ && do {$retsubtype = "int"; $ret_type="EVaction"; last;};
    }
    print REVP "    int cond = CMCondition_get(conn->cm, conn);\n";
    print REVP "    CMFormat f = CMlookup_format(conn->cm, ${subr}_req_flds);\n";
    print REVP "    EV_${retsubtype}_response *response;\n" unless ($return_type{$subr} eq "void");
    print REVP "    ${subr}_request request;\n";
    foreach $arg (split (", ", $args[1])) {
	$_ = $arg;
	if (/^\s*(.*\W+)(\w+)$\s*/) {
	    $argtype = $1;
	    $argname = $2;
	    $argtype =~ s/\s+$//;
	    $argtype =~ s/(?!\w)\s+(?=\W)//;  #remove unnecessary white space
	    $argtype =~ s/(?!\W)\s+(?=\w)//;  #remove unnecessary white space
	    $argright = $argname;
	  switch:for ($argtype) {
	      /attr_list/ && do {$argright = "attr_list_to_string($argname)"; last;};
	  }
	}
	print REVP "    request.$argname = $argright;\n";
    }
    print REVP "    if (f == NULL) {\n";
    print REVP "        f = CMregister_format(conn->cm, \"${subr}_request\", ${subr}_req_flds,\n";
    print REVP "			      all_subformats_list);\n";
    print REVP "    }\n";
    print REVP "    CMCondition_set_client_data(conn->cm, cond, &response);\n"  unless ($return_type{$subr} eq "void");
    print REVP "    CMwrite(conn, f, &request);\n";
    print REVP "    CMCondition_wait(conn->cm, cond);\n";
  switch:for ($return_type{$subr}) {
      /attr_list/ && do {print REVP "    return attr_list_from_string(response->ret);\n"; last;};
      /void/ && do {last;};
      /EVstone/ && do {print REVP "    return (EVstone) response->ret;\n"; last;};
      /EVaction/ && do {print REVP "    return (EVaction) response->ret;\n"; last;};
      /int/ && do {print REVP "    return response->ret;\n"; last;};
      /EVevent_list/ && do {print REVP "    return response->ret;\n"; last;};
  }
    print REVP "}\n";
}

{
    local ($/, *INPUT);
	
    unless (open(INPUT,"<evpath.h")) {
	die "sudden flaming death, no evpath.h\n";
    }

    $_ = <INPUT>;
    s[/\*NOLOCK\*/][NOLOCK]g;
    s[/\*REMOTE\*/][REMOTE]g;
    s{/\*.+\*/}{}g;
    @f = split(/\n/);
    close INPUT;
}
LINE:
for (@f) {
    if (/NOLOCK/) {
	$nolock = 1;
    }
    if (/REMOTE/) {
	$remote = 1;
    }
    if (/^extern/) {
	next LINE if (/\"C\"/);
	$decl = "";
	if ($nolock == 1) {$decl = "NOLOCK";}
	if ($remote == 1) {$decl = "REMOTE";}
	$nolock = 0;
	$remote = 0;
	$pending = 1;
    }
    if (($pending) && /;/) {
	$decl = $decl . " " . $_;
	push (@DECLS, $decl);
	$pending = 0;
    }
    if ($pending) {
	$decl = $decl . " " . $_;
    }
}
for (@DECLS) {
    $nolock = 0;
    $remote = 0;
    if (/NOLOCK/) {
	s/NOLOCK//g;
	$nolock = 1;
    }
    if (/REMOTE/) {
	s/REMOTE//g;
	$remote = 1;
    }
    if (/extern\W+(\w+\W+)(\w+).*\((.*)\)/) {
	$return = $1;
	$name = $2;
	$_ = $3;
	s/\)//g;
	s/\s+/ /g;
	$return =~ s/(?!\w)\s+(?=\W)//;  #remove unnecessary white space
	$return =~ s/(?!\W)\s+(?=\w)//;  #remove unnecessary white space
	$return =~ s/\s*$//;  #remove unnecessary white space
	$return =~ s/^\s*//;  #remove unnecessary white space
	$return_type{$name} = $return;
	$args = $_;
	$arguments{$name} = "$args";
    }
    if ($nolock == 1) {
	$nolocking{$name} = 1;
    }
    if ($remote == 1) {
	$remote_enabled{$name} = 1;
    }
}

open FIXUP, ">fixup.pl";
print FIXUP "#!/usr/bin/env perl -pi.bak\n";
unless (open (INT, ">cm_interface.c")) { die "Failed to open cm_interface.c";}
print INT<<EOF;
/*
 *  This file is automatically generated by gen_interface.pl from evpath.h.
 *
 *  DO NOT EDIT
 *
 */
#include "io.h"
#include "atl.h"
#include "evpath.h"
#include "cm_internal.h"
#ifdef	__cplusplus
extern "C" \{
#endif
EOF
    foreach $subr (keys %return_type) {
	print INT "\nextern $return_type{$subr}\n";
	print INT "$subr ( $arguments{$subr} )\n";
	print INT "{\n";
	undef $cmanager;
	undef $cmconnection;
	undef $evsource;
	undef $cmtaskhandle;
	undef $cmformat;
	foreach $arg (split ( ",", $arguments{$subr})) {
	    $_ = $arg;
	    if (/\W+(\w+)\W*$/) {
		$name = $1;
	    }
	    if (/CManager/) {
		$cmanager = $name;
	    }
	    if (/CMConnection/) {
		$cmconnection = $name;
	    }
	    if (/EVsource/) {
		$evsource = $name;
	    }
	    if (/CMTaskHandle/) {
		$cmtaskhandle = $name;
	    }
	    if ((/CMFormat/) && (!/CMFormatList/)){
		$cmformat = $name;
	    }
	}

	$_ = $return_type{$subr};
	if (/^\s*void\s*$/) {
	    $return_type{$subr} = "void";
	}
	if ($return_type{$subr} ne "void") {
	    print INT "\t$return_type{$subr} ret;\n";
	}
	if (!defined($nolocking{$subr})) {
	    if (defined($cmanager)) {
		print INT "\tCManager_lock($cmanager);\n";
	    } else {
		if (defined($cmconnection)) {
		    print INT "\tCManager cm = $cmconnection->cm;\n";
		} elsif (defined($evsource)) {
		    print INT "\tCManager cm = $evsource->cm;\n";
		} elsif (defined($cmtaskhandle)) {
		    print INT "\tCManager cm = $cmtaskhandle->cm;\n";
		} elsif (defined($cmformat)) {
		    print INT "\tCManager cm = $cmformat->cm;\n";
		} else {
#		    print INT "\tCManager cm = duh;\n";
		}
		print INT "\tCManager_lock(cm);\n";
	    }
	}
	if ($return_type{$subr} eq "void") {
	    print INT "\t";
	} else {
	    print INT "\tret = ";
	}

	print INT "INT_$subr(";
	$first = 1;
	foreach $arg (split ( ",", $arguments{$subr})) {
	    if ($first != 1) {
		print INT ", ";
	    } else {
		$first = 0;
	    }
	    $_ = $arg;
	    if (/\W+(\w+)\W*$/) {
		print INT "$1";
	    }
	}
	print INT ");\n";
	if ((!defined($nolocking{$subr})) && ($subr ne "CManager_close")) {
	    if (defined($cmanager)) {
		print INT "\tCManager_unlock($cmanager);\n";
	    } else {
		print INT "\tCManager_unlock(cm);\n";
	    }
	}
	print INT "\treturn ret;\n" unless ($return_type{$subr} eq "void");
	print INT "}\n";
	print FIXUP "s/$subr/INT_$subr/g unless /INT_$subr/;\n";
    }
print "done\n";

print INT<<EOF;
#ifdef	__cplusplus
\}
#endif
EOF
close INT;
unless (open (REVPH, ">revpath.h")) { die "Failed to open revpath.h";}
unless (open (REVP, ">revp.c")) { die "Failed to open revp.c";}
print REVP<<EOF;
/*
 *  This file is automatically generated by gen_interface.pl from evpath.h.
 *
 *  DO NOT EDIT
 *
 */
#include "io.h"
#include "atl.h"
#include "evpath.h"
#include "cm_internal.h"
#ifdef	__cplusplus
extern "C" \{
#endif

CMFormatRec all_subformats_list[] = {{NULL, NULL}};

typedef struct _EV_void_response {
    int condition_var;
} EV_void_response;

typedef struct _EV_int_response {
    int condition_var;
    int  ret;
} EV_int_response;

typedef struct _EV_string_response {
    int condition_var;
    char *ret;
} EV_string_response;

typedef struct _EV_EVevent_list_response {
    int condition_var;
    int ret_len;
    EVevent_list ret;
} EV_EVevent_list_response;

EOF
    foreach $subr (keys %return_type) {
	defined($remote_enabled{$subr}) || next;

	print REVPH "\nextern $return_type{$subr}\n";
	@args = split( ", ",  $arguments{$subr});
	print REVPH "R$subr(CMConnection conn, $args[1]);\n";
	gen_type(${subr}, $arguments{$subr});
	gen_field_list(${subr}, $arguments{$subr});
	gen_stub(${subr}, $arguments{$subr});
    }
print "done\n";

print INT<<EOF;
#ifdef	__cplusplus
\}
#endif
EOF
close INT;
close FIXUP;
