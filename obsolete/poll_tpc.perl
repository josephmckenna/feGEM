#!/bin/perl

while (1) {
    for ($i=1; $i<=11; $i++) {
	#print "i=$i\n";
	$ip = sprintf("feam%d", $i);
	#$cmd = "wget http://$ip -o /dev/null -O /dev/null";
	$cmd = "wget http://$ip -O /dev/null";
	print "cmd $cmd\n";
	system $cmd;
    }
}

#end

