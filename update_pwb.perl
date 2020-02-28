#!/usr/bin/perl -w

#my $fw = "/home/agdaq/online/firmware/pwb_rev1/feam-2018-01-24/feam_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/pwb_rev1/feam-2018-03-12-test/feam_rev1_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/pwb_rev1/pwb_rev1_20180531_cabf9d3d_bryerton/feam_rev1_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/pwb_rev1/pwb_rev1_20180613_test/feam_rev1_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/pwb_rev1/pwb_rev1_20180628_ae04285d/feam_rev1_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/pwb_rev1/pwb_rev1_20180808_0f5edf1b_bryerton/feam_rev1_auto.rpd";
#my $fw = "/home/agdaq/online/firmware/pwb_rev1/pwb_rev1_20180912_6c3810a7_bryerton/feam_rev1_auto.rpd";
my $fw = "/home/agdaq/online/firmware/pwb_rev1/pwb_rev1_20180913_a8b51569_bryerton/feam_rev1_auto.rpd";

die "Cannot read RPD file $fw: $!\n" if ! -r $fw;

foreach my $x (@ARGV)
{
    print "update pwb [$x]\n";
    update($fw, $x);
}

if ($ARGV[0] eq "all") {
    update($fw, "pwb12");
    update($fw, "pwb13");
    update($fw, "pwb14");
    update($fw, "pwb02");
    update($fw, "pwb11");
    update($fw, "pwb17");
    update($fw, "pwb18");
    update($fw, "pwb19");

    update($fw, "pwb20");
    update($fw, "pwb21");
    update($fw, "pwb22");
    update($fw, "pwb23");
    update($fw, "pwb24");
    update($fw, "pwb25");
    update($fw, "pwb26");
    update($fw, "pwb27");

    update($fw, "pwb28");
    update($fw, "pwb29");
    update($fw, "pwb03");
    update($fw, "pwb77");
    update($fw, "pwb01");
    update($fw, "pwb33");
    update($fw, "pwb34");
    update($fw, "pwb35");

    update($fw, "pwb36");
    update($fw, "pwb37");
    update($fw, "pwb38");
    update($fw, "pwb39");
    update($fw, "pwb76");
    update($fw, "pwb41");
    update($fw, "pwb42");
    update($fw, "pwb43");

    update($fw, "pwb44");
    update($fw, "pwb45");
    update($fw, "pwb46");
    update($fw, "pwb47");
    update($fw, "pwb48");
    update($fw, "pwb49");
    update($fw, "pwb50");
    update($fw, "pwb04");

    update($fw, "pwb52");
    update($fw, "pwb53");
    update($fw, "pwb54");
    update($fw, "pwb55");
    update($fw, "pwb56");
    update($fw, "pwb57");
    update($fw, "pwb58");
    update($fw, "pwb59");

    update($fw, "pwb60");
    update($fw, "pwb00");
    update($fw, "pwb07");
    update($fw, "pwb63");
    update($fw, "pwb64");
    update($fw, "pwb65");
    update($fw, "pwb66");
    update($fw, "pwb67");
    
    update($fw, "pwb68");
    update($fw, "pwb69");
    update($fw, "pwb70");
    update($fw, "pwb71");
    update($fw, "pwb72");
    update($fw, "pwb73");
    update($fw, "pwb74");
    update($fw, "pwb75");
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
