#!/usr/bin/perl -w

$num_args = $#ARGV + 1;
if ($num_args != 1) {
	print "\nUsage: ./trace_info.pl TRACE_FILE\n\n";
	exit;
}

$filename=$ARGV[0];

$gov_info=`awk '/=sugov/ && /sched_switch/ {print} /cpu_frequency:/ && /cpu_id=4/ {print}' $filename`;
my @lines = split /\n/, $gov_info;

my @start = ();
my @duration = ();
my @freqs = ();

foreach my $line (@lines) {

	if ($line =~ m/state=([0-9]+)/) {
		push(@freqs,$1);
	}

	my @fields = split /\s{1,}/, $line;
	my $st = substr $fields[3], 0, -1;
    #$st = $st * 1000.0;

	if ($line =~ m/next_comm=sugov/) {
		push(@start,$st);
	}

	if ($line =~ m/prev_comm=sugov/) {
		push(@duration, $st - $start[-1]);
		if (scalar(@start) != scalar(@freqs)) {
			push(@freqs, $freqs[-1]);
		}
	}
}

print "start, duration, frequency\n";
my $size = scalar @duration;
my $acum = 0.0;
for ($i=0; $i<$size; $i=$i+1) {
	#my $s = $start[$i] + $start_time;
	#my $hour = int($s/3600);
	#my $min = int(($s - $hour*3600) / 60);
	#my $sec = $s - ($hour*3600) - ($min*60);
	my $d = $duration[$i];
	$acum = $acum + $d;
	#print "$hour:$min:$sec, $d, $freqs[$i]\n";
	print "$start[$i],$d,$freqs[$i]\n";
}

#$acum = $acum/$size;
#print "\n$size changes with a mean time of: $acum\n";
