#!/usr/bin/perl -w

#my $fw = "/home/agdaq/online/firmware/alpha16/20180327-ko/alpha16_one_page_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/alpha16/20180411-ko/alpha16_one_page_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/alpha16/20180428-ko/alpha16_one_page_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/alpha16/20180504-ko/alpha16_one_page_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/alpha16/20180511-ko/alpha16_one_page_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/alpha16/20180524-ko/alpha16_one_page_auto.rpd";
my $fw = "/home/agdaq/online/firmware/alpha16/20180927-ko/alpha16_one_page_auto.rpd";

if ($ARGV[0] eq "test") {
    $fw = "/home/agdaq/online/firmware/git/adc_firmware/bin/alpha16_one_page_auto.rpd";
    #$fw = "/home/olchansk/git/adc_firmware/bin/alpha16_one_page_auto.rpd";
}

die "Firmware file \"$fw\" does not exist: $!" unless -r $fw;

if ($ARGV[0] eq "all") {
    update($fw, "adc01");
    update($fw, "adc02");
    update($fw, "adc03");
    update($fw, "adc04");
    update($fw, "adc05");
    update($fw, "adc06");
    update($fw, "adc07");
    update($fw, "adc08");

    update($fw, "adc09");
    update($fw, "adc10");
    update($fw, "adc11");
    update($fw, "adc12");
    update($fw, "adc13");
    update($fw, "adc14");
    update($fw, "adc17");
    update($fw, "adc16");
} else {
    foreach my $x (@ARGV) {
	print "update adc [$x]\n";
	update($fw, $x);
    }
}
    
exit 0;

sub update
{
   my $fw = shift @_;
   my $pwb = shift @_;
   my $cmd = sprintf("esper-tool -v upload -f %s http://%s update file_rpd", $fw, $pwb);
   print $cmd,"\n";
   system $cmd." &";
}

# end
