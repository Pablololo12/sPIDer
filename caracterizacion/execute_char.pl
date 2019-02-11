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
#system("adb","shell","echo 0",">/sys/devices/system/cpu/cpu7/online");

system("adb","shell","echo","userspace",">/sys/devices/system/cpu/cpufreq/policy4/scaling_governor");
`adb shell echo \"$freqs[1] >/sys/devices/system/cpu/cpufreq/policy4/scaling_setspeed\"`;

print "\tModifiying thermal\n";
`adb shell \"echo user_space >/sys/class/thermal/thermal_zone0/policy\"`;
`adb shell \"echo 75000 >/sys/class/thermal/thermal_zone0/trip_point_0_temp\"`;
`adb shell \"echo 85000 >/sys/class/thermal/thermal_zone0/trip_point_1_temp\"`;

print "\tStarting job\n";
$pid_bench = fork();
if ($pid_bench == 0) {
	exec("adb","shell","/data/local/tmp/bin/benchmark","-t 2","-c 4,7","-m 4096") or die "Child couldn\'t exec benchmark";
	exit 0;
}

print "\tStarting file poller\n";
$pid_poller = fork();
if ($pid_poller == 0) {
	exec("adb","shell","/data/local/tmp/bin/poller","-t 100000","/sys/class/thermal/thermal_zone0/temp","/sys/devices/system/cpu/cpu4/cpufreq/cpuinfo_cur_freq",">/data/local/tmp/bin/output.csv") or die "Child couldn\'t exec poller";
	exit 0;
}

#@ind_freq = (1,0,1,2,1,0,2,1,2,1,0,1,2);
@ind_freq = (1,0,1,0);
my $time_sleep = 1200;

print "\tLoop through the frequencies\n";
foreach $ind (@ind_freq) {
	print "\t\t$freqs[$ind]\n";
	`adb shell echo \"$freqs[$ind] >/sys/devices/system/cpu/cpufreq/policy4/scaling_setspeed\"`;
	sleep $time_sleep;
}

system("adb","shell","killall","benchmark");
system("adb","shell","killall","poller");
system("adb","pull","/data/local/tmp/bin/output.csv");
system("adb","shell","rm","/data/local/tmp/bin/output.csv");
waitpid($pid_bench, 0);
waitpid($pid_poller,0);
