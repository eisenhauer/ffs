#perl
use strict;
use warnings;
use Text::Balanced qw/extract_multiple extract_bracketed/;

sub parse_c_test($);
sub generate_cod_test($$$);

my $filename = "960209-1.c";

my ($decls, $subroutines) = parse_c_test($filename) ;
generate_cod_test($filename, $decls, $subroutines);

sub generate_cod_test($$$) 
{
    my ($filename, $decls, $subroutines) = @_;

    foreach my $sub (@$subroutines) {
        my ($name, $decl, $body) = @$sub;
	print "$name,   $decl,  $body\n";
    }

    unless (open (INT, ">gnu_tests/$filename")) { die "Failed to open cm_interface.c";}
print INT<<EOF;
#include "config.h"
#include "cod.h"
#include "assert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int exit_value = 0; /* success */
void
abort()
{
    exit_value = 1; /* failure */
}

void
exit(int value)
{
    if (value != 0) {
	exit_value = value;
    }
}

int
main(int argc, char**argv)
{
    
EOF
    my $externs_table= "    cod_extern_entry externs[] = \n    {\n";
    my $externs_string = "    char extern_string[] = \"\\n\\\n";
    my $function_bodies = "    char *func_bodies[] = {\n";
    my $function_decls = "    char *func_decls[] = {\n";
    foreach my $sub (@$subroutines) {
        my ($name, $decl, $body) = @$sub;
	chomp($decl);
	$externs_table .= "	{\"$name\", (void*)(long)-1},\n";
	$externs_string .= "	$decl;\\n\\\n";
	$body =~ s/\n/\\n\\\n/g;
	$body =~ s/"/\\"/g;
	$function_bodies .= "\n/* body for $name */\n\"$body\",\n";
	$function_decls .= "\t\"$decl;\",\n";
    }
    $externs_table .= "	{\"abort\", (void*)abort},\n	{\"exit\", (void*)exit},\n	{(void*)0, (void*)0}\n    };\n";
    $externs_string .= "    	void exit(int value);\\n\\\n        void abort();\";";
    $function_bodies .= "\"\"};\n";
    $function_decls .= "\t\"\"};\n";
    print INT "$externs_table\n";
    print INT "$externs_string\n";
    print INT "$function_bodies\n";
    print INT "$function_decls\n";
    print INT "    int i;\n";
    print INT "    cod_code gen_code[". scalar @$subroutines. "];\n";
    print INT "    for (i=0; i < ". scalar @$subroutines."; i++) {\n";
    print INT "        cod_parse_context context = new_cod_parse_context();\n";
    print INT "        cod_assoc_externs(context, externs);\n";
    print INT "        cod_parse_for_context(extern_string, context);\n";
    print INT "        cod_subroutine_declaration(func_decls[i], context);\n";
    print INT "        gen_code[i] = cod_code_gen(func_bodies[i], context);\n";
    print INT "        externs[i].extern_value = (void*) gen_code[i]->func;\n";
    print INT "        if (i == ". scalar @$subroutines - 1 . ") {\n";
    print INT "            int (*func)() = (int(*)()) externs[i].extern_value;\n";
    print INT "            if (exit_value != 0) {\n";
    print INT "                printf(\"Test $filename failed\\n\");\n";
    print INT "                exit(exit_value);\n";
    print INT "            }\n";
    print INT "        }\n";
    print INT "    }\n";
    print INT "}\n";
}

sub parse_c_test($) {
    my ($filename) = @_;
    local $_;
    my $file;
    
    my @decls = ();
    my @subroutines = ();
    open my $fileHandle, '<', $filename;
    
    { 
	local $/ = undef; # or use File::Slurp
	$file = <$fileHandle>;
    }
    
    close $fileHandle;
    
    my @array = extract_multiple(
				 $file,
				 [ sub{extract_bracketed($_[0], '{}')},],
				 undef,
				 0
				);
    
    
    while (@array) {
	my $non_bracket_item = shift(@array);
	$non_bracket_item =~ s/^\s+//;
	next if ($non_bracket_item eq "");
	
	if (substr($non_bracket_item, 0, 1) eq "{") {
	    print "Failure in item {$non_bracket_item }\n";
	    next;
	}
	my @bits = split(/\n\n/, $non_bracket_item);
	my $last_segment = pop @bits;
	if (@bits) {
	    # everything prior to the last empty lines must be declarations
	    my $item = join ("\n\n", @bits);
	    $item =~ s/\s+$//;
	    next if ($item eq "");
	    print "This is a 1 declaration set [\n $item\n]\n\n\n";
	    push @decls, $item;
	}
	#
	@bits = split(/ /, $last_segment);
	my $last = pop @bits;
	my $before_last = "";
	if (@bits) {
	    $before_last = pop @bits;
	    push @bits, $before_last;
	}
	push @bits, $last;
	
	if (($before_last eq "struct") || ($before_last eq "enum")) {
	    my $decl_set = $last_segment . shift(@array);
	    my $next = shift(@array);
	    if (substr($next, 0, 1) eq ";") {
		$decl_set .= ";";
	    }
	    unshift @array, substr($next, 1);
	    print "This is a declaration set [\n $decl_set\n]\n\n\n";
	    push @decls, $decl_set;
	    next;
	}
	if (substr($last_segment, -1) eq ")") {
	    my $subroutine_name;
	    if ($last_segment =~ /^\W*\w+\W*$/)   {
	      $last_segment = "void $last_segment";
	    }
	    if ($last_segment =~ /(\w+)\W*\(/)   {
		$subroutine_name = $1;
	    }
	    print "Ha, must be a subroutine 1, name $subroutine_name, profile $last_segment\n";
	    push @subroutines, [($subroutine_name, $last_segment, shift(@array))];
	    next;
	}
	# Well, maybe K&R style decls.
	my @lines = split (/\n/, $last_segment);
	my $count = 0;
	my $last_line;
	while (@lines) {
	    $last_line = pop @lines;
	    $last_line =~ s/\s+$//;
	    if (substr($last_line, -1) eq ";") {
		$count++;
		next;
	    }
	    if (substr($last_line, -1) ne ")") {
		print "Didn't get it\n";
	    }
	    while(@lines) {
		$last_line = pop(@lines) . "\n" . $last_line;
	    }
	}
	my ($subroutine_prefix, $params) = split /\(/, $last_line;
	my @params = split(/,/, $params);
	if (($count != 0) && (@params != $count)) {
	    print "Param count didn't match " . scalar @params . " vs $count\n";
	}
	my $subroutine_name;
	if ($subroutine_prefix =~ /^\W*\w+\W*$/)   {
	    $subroutine_prefix = "void $subroutine_prefix";
	}
	if ($subroutine_prefix =~ /(\w+)\W*$/)   {
	    $subroutine_name = $1;
	}
	$subroutine_prefix =~ s/\n/ /g;
	my $subroutine_header = $subroutine_prefix . "(";
	@lines = split (/\n/, $last_segment);
	while ($count) {
	    my $param = pop @lines;
	    $param =~ s/;\s*$//;
	    $param =~ s/^\s*//;
	    $subroutine_header .= "$param";
	    if ($count != 1) {
		$subroutine_header .= ", ";
	    }
	    $count--;
	}
	$subroutine_header .= ")";
	print "Ha, must be a subroutine 2, name $subroutine_name, profile $subroutine_header\n";
	push @subroutines, [($subroutine_name, $subroutine_header, shift(@array))];
	next;
	
    }  
    return (\@decls, \@subroutines);
}

