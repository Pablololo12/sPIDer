#!/usr/bin/perl -w

$num_args = $#ARGV + 1;
if ($num_args < 1) {
	print "Usage: mix_all.pl files...\n";
	exit;
}

$header = `head -n1 $ARGV[1]`;
$header =~ s/\n//g;
print "$header,type,tag,cores,sel_freq\n";

foreach (0..$#ARGV) {
	$file = shift @ARGV;
	$_ = $file;
	$type = 0;
	$tag = $1 if ($file =~ /[^\/]*\/([^\/]*)/);
	$cores = 0;
	$sel_freq = 903000;
	if (m/(benchmark|float)/) {
		$type = 1;
	} elsif (m/(dhry|cpu)/) {
		$type = 2;
	} elsif (m/mem/) {
		$type = 3;
	}

	if (m/01/) {
		$cores = 1;
	} elsif (m/02/) {
		$cores = 2;
	}

	if (m/903000/) {
		$sel_freq = 903000;
	} elsif (m/1421000/) {
		$sel_freq = 1421000;
	}

	$out = `awk '{if (\$0!~/time/){ print \$0",$type,$tag,$cores,$sel_freq"}}' $file`;
	print "$out";
}
