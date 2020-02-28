#!/usr/bin/perl -w

for (my $i=71; $i<=77; $i++) {
    system "odbedit -c \"create STRING /Equipment/UDP/Settings/pwb$i\"";
    system "odbedit -c \"set /Equipment/UDP/Settings/pwb$i PB$i\"";
}

exit 0;
#end
