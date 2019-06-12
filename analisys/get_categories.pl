#!/usr/bin/perl -w

$num_args = $#ARGV + 1;
if ($num_args < 2) {
	print "Usage: get_categories.pl [csv with categories] [logcat file]\n";
	exit;
}

my $benchs = $ARGV[0];
my $logcat = $ARGV[1];

open my $catfile, $benchs or die "Could not open $benchs: $!";

my %cat;

my $header = <$catfile>;
while (my $line = <$catfile>) {
	$line =~ s/\n//g;
	my @cols = split /,/, $line;
	$cat{$cols[0]} = $cols[1];
}

print "Workload, Type, Timestamp\n";
foreach my $work (keys %cat) {
	$out = `awk '/$work/ {print \$2}' $logcat`;
	@outlist = split /\n/, $out;
	foreach my $timestamp (@outlist) {
		print "$timestamp,$work,$cat{$work}\n";
	}
}

