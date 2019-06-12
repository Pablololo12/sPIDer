#!/usr/bin/perl -w

$num_args = $#ARGV + 1;
if ($num_args != 1) {
	print "\nUsage: ./get_temperature.pl TRACE_FILE\n\n";
	exit;
}

$filename=$ARGV[0];

$temp_info=`awk '/thermal_temperature/ {print}' $filename`;
my @lines = split /\n/, $temp_info;

my @time = ();
my @temp = ();

foreach my $line (@lines) {
	my @fields = split /\s{1,}/, $line;
	my $st = substr $fields[3], 0, -1;

	if ($line =~ m/temp=([0-9]*)/) {
		push(@time, $st);
		push(@temp, $1);
	}
}

my $size = scalar @time;
print "Timestamp, temp\n";

for (my $i=0; $i<$size; $i=$i+1) {
	#my $s = $time[$i] + $start_time;
	#my $hour = int($s/3600);
	#my $min = int(($s - $hour*3600) / 60);
	#my $sec = $s - ($hour*3600) - ($min*60);
	#print "$hour:$min:$sec, $temp[$i]\n";
	print "$time[$i],$temp[$i]\n";
}
