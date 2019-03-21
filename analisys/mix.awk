#! /usr/bin/awk -f

{
	if ($0!~/time/)
		if (FILENAME~/matrix/)
			print $0,1;
		else if (FILENAME~/dhrystone/)
			print $0,2;
		else if (FILENAME~/memcpy/)
			print $0,3;
}
