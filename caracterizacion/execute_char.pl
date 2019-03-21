#!/usr/bin/perl -w

print " | System characterization |\n";
print "\tStarting ADB as Root...\n";

`adb root`;

@freqs = (903000, 1421000, 1805000, 2112000);

print "\tShuting down unused cores...\n";
for my $i (0..3) {
	system("adb","shell","echo 0",">/sys/devices/system/cpu/cpu$i/online");
}
system("adb","shell","echo 0",">/sys/devices/system/cpu/cpu5/online");
system("adb","shell","echo 0",">/sys/devices/system/cpu/cpu6/online");
system("adb","shell","echo 0",">/sys/devices/system/cpu/cpu7/online");

system("adb","shell","echo","userspace",">/sys/devices/system/cpu/cpufreq/policy4/scaling_governor");
`adb shell echo \"$freqs[1] >/sys/devices/system/cpu/cpufreq/policy4/scaling_setspeed\"`;

print "\tModifiying thermal\n";
`adb shell \"echo user_space >/sys/class/thermal/thermal_zone0/policy\"`;
`adb shell \"echo 75000 >/sys/class/thermal/thermal_zone0/trip_point_0_temp\"`;
`adb shell \"echo 85000 >/sys/class/thermal/thermal_zone0/trip_point_1_temp\"`;

sub ExecMatrix {
	print "\tStarting Matrix benchmark\n";
	$pid_bench = fork();
	if ($pid_bench == 0) {
		exec("adb","shell","/data/local/tmp/bin/benchmark","-t 1","-c 4","-m 4096") or die "Child couldn\'t exec benchmark";
		exit 0;
	}
	return $pid_bench;
}

sub ExecDhry {
	print "\tStarting Dhrystone\n";
	$pid_bench = fork();
	if ($pid_bench == 0) {
		exec("adb","shell","/data/local/tmp/bin/dhrystone","-r 14400","-t 1") or die "Child couldn\'t exec Dhrystone";
		exit 0;
	}
	return $pid_bench;
}

sub ExecMem {
	print "\tStarting Memory\n";
	$pid_bench = fork();
	if ($pid_bench == 0) {
		#exec("adb","shell","/data/local/tmp/bin/memcpy","-c 4","-i 829496729", "-s 10485760") or die "Child couldn\'t exec Memcpy";
		exec("adb","shell","while","true;","do","/data/local/tmp/bin/memcpy","-c 4","-i 10000000","-s 1048576","||","break;","done") or die "Child couldn\'t exec Memcpy";
		exit 0;
	}
	return $pid_bench;
}

sub ReadTemp {
	@ind_freq = @_;
	print "\tStarting file poller\n";
	$pid_poller = fork();
	if ($pid_poller == 0) {
		exec("adb","shell","/data/local/tmp/bin/poller","-t 100000","/sys/class/thermal/thermal_zone0/temp","/sys/devices/system/cpu/cpu4/cpufreq/cpuinfo_cur_freq",">/data/local/tmp/bin/output.csv") or die "Child couldn\'t exec poller";
		exit 0;
	}

	my $time_sleep = 600;

	print "\tLoop through the frequencies\n";
	foreach $ind (@ind_freq) {
		print "\t\t$freqs[$ind]\n";
		`adb shell echo \"$freqs[$ind] >/sys/devices/system/cpu/cpufreq/policy4/scaling_setspeed\"`;
		sleep $time_sleep;
	}
	system("adb","shell","killall","poller");
	waitpid($pid_poller,0);
}

=pod
ExecMatrix();
@ind_freq = (1,2,1,0,1,2,1,0,1,2,1,0);
$pid = ReadTemp(@ind_freq);
system("adb","shell","killall","benchmark");
system("adb","pull","/data/local/tmp/bin/output.csv");
`mv output.csv matrix.csv`;
system("adb","shell","rm","/data/local/tmp/bin/output.csv");
waitpid($pid, 0);

sleep 1200;

ExecDhry();
@ind_freq = (1,2,1,0,1,2,1,0);
$pid = ReadTemp(@ind_freq);
system("adb","shell","killall","dhrystone");
system("adb","pull","/data/local/tmp/bin/output.csv");
`mv output.csv dhrystone.csv`;
system("adb","shell","rm","/data/local/tmp/bin/output.csv");
waitpid($pid, 0);

sleep 1200;
=cut

ExecMem();
@ind_freq = (0,1,2,1,0,1,2,1,0);
$pid = ReadTemp(@ind_freq);
system("adb","shell","killall","memcpy");
system("adb","pull","/data/local/tmp/bin/output.csv");
`mv output.csv memcpy.csv`;
system("adb","shell","rm","/data/local/tmp/bin/output.csv");
waitpid($pid, 0);

