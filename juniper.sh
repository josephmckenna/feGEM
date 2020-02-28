#!/bin/sh
cd ~/online/juniper
ssh -x root@juniper-private cli show interfaces diagnostics optics > optics.txt
ssh -x root@juniper-private cli show chassis hardware > hardware.txt
ssh -x root@juniper-private cli show chassis pic fpc-slot 0 pic-slot 0 > pic.txt
ssh -x root@juniper-private cli show ethernet-switching table > table.txt
ssh -x root@juniper-private cli show interfaces extensive > extensive.txt
#
grep "current  " optics.txt | sort -n > sfp_tx_bias.txt
grep "output power  " optics.txt | sort -n > sfp_tx_power.txt
grep "optical power  " optics.txt | sort -n > sfp_rx_power.txt
grep -i "module temperature  " optics.txt | sort -n > sfp_temperature.txt
#end
