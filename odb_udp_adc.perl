#!/usr/bin/perl -w

for (my $i=1; $i<=20; $i++) {
    my $ii = sprintf("%02d", $i);
    system "odbedit -c \"create STRING /Equipment/UDP/Settings/adc$ii\"";
    system "odbedit -c \"set /Equipment/UDP/Settings/adc$ii AA$ii\"";
}

exit 0;
#end
