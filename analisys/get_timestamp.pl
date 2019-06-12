#!/usr/bin/perl -w

$num_args = $#ARGV + 1;
if ($num_args != 1) {
	print "\nUsage: ./get_timestamp.pl TRACE_FILE\n\n";
	exit;
}

$filename=$ARGV[0];

$time_info=`awk '/print/ {print}' $filename`;
my @lines = split /\n/, $time_info;

print "Seconds,Time\n";
foreach my $line (@lines) {
	if ($line =~ m/([0-9]+:[0-9]+:[0-9]+)/) {
		my $time = $1;
		if ($line =~ m/([0-9]+\.[0-9]+):/) {
			print "$1,$time\n";
		}
	}
}
