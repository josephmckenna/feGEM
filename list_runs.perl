#!/usr/bin/perl -w

my $dir = "/z8tb/agdaq/data/";

my @ls = `/bin/ls -1 $dir`;

my %runsize;

foreach my $f (sort @ls)
{
    chop $f;
    my $s = -s "$dir/$f";
    print "$s $f\n";

    $f =~ /run(\d+)sub(\d+)/;
    my $run = $1;
    my $sub = $2;

    $run = "norun" unless $run;

    $runsize{$run} += $s;
}

foreach my $r (sort keys %runsize)
{
    my $s = $runsize{$r};
    print "$s run$r\n";
}

exit 0;

# end
