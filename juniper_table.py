#!/usr/bin/env python

import pxssh
import re
import time
import socket
import sys
import subprocess as sp
import pythonMidas

import numpy as np
#import matplotlib.pyplot as plt

import pickle

def save_obj( obj, path, name ):
    with open( path + name + '.pkl', 'wb') as f:
        pickle.dump(obj, f, pickle.HIGHEST_PROTOCOL) # use 0 for text format

def load_obj( path, name ):
    with open( path + name + '.pkl', 'rb') as f:
        return pickle.load(f)


def junoscli():
    try:
        s = pxssh.pxssh()
        if not s.login('juniper-private', 'root', 'root123'):
            print "SSH session failed on login."
            print str(s)
        else:
            print "SSH session login successful"
            s.sendline('whoami; hostname')
            s.prompt()
            resp=s.before.splitlines()
            print resp[2], '@', resp[3]

            s.sendline("echo 'show interfaces diagnostics optics | no-more' | cli")
            #s.sendline('cli')
            #s.sendline('show interfaces diagnostics optics | no-more')
            s.prompt()         # match the prompt
            optics=s.before    # print everything before the prompt
            #s.sendline('q')

            s.sendline("echo 'show ethernet-switching table | no-more' | cli")
            #s.sendline('cli')
            #s.sendline('show ethernet-switching table | no-more')
            s.prompt()
            macaddr=s.before    # print everything before the prompt
            #s.sendline('q')

            s.logout()
            print 'session closed'

            return optics, macaddr
    
    except pxssh.ExceptionPxssh, e:
        print "pxssh failed on login."
        print str(e)
    
def read_files():
    #s.sendline("echo 'show interfaces diagnostics optics | no-more' | cli")
    #optics=s.before    # print everything before the prompt

    with open('../juniper/optics.txt', 'r') as myfile:
        optics = myfile.read()

    #s.sendline("echo 'show ethernet-switching table | no-more' | cli")
    #macaddr=s.before    # print everything before the prompt
    
    with open('../juniper/table.txt', 'r') as myfile:
        macaddr = myfile.read()

    return optics, macaddr

def ExtractOpticalData(optics):
    print '----> optics <----'
    '''
    Laser bias current
    Laser output power
    Module temperature
    Module voltage
    Receiver signal average optical power
    '''
    head='Physical interface: '
    report=optics.splitlines()
    odata={} # {port number : { i:mA, tx:mW, T:Celsius, v:Volts, rx:mW } }
    var=['i','tx','T','v','rx']
    for line in report:
        if head in line: # identify data block
            port=re.search('[a-z]{2}-[0-9]\/[0-9]\/[0-9]{1,2}',line).group()
            ip=port.find(':') # isolate the interface number

            name=re.search('[0-9]{1,2}$',port[ip+1:len(port)]).group()
            #print 'port:', name, '\t',
            odata[name]={}

            idx=report.index(line)
            for jdx in range(0,5): # only the next 5 lines matter
                readout=report[jdx+idx+1]
                s=readout.find(':')
                # #print var[jdx], readout[s+3:len(readout)], '\t',
                value=float( re.match( '[0-9]{,}\.?[0-9]{,}', readout[s+3:len(readout)] ).group() )
                #print var[jdx], value, '\t',
                odata[name][var[jdx]]=value
            #print ''
    return odata


def plot_optdata(optdata,name,connected):
    
    pwblist=[]
    fslist=[]
    for ipwb in sorted(connected.keys()):
        #print 'PWB: ', ipwb, '\tPort: ', connected[ipwb]
        pwblist.append(ipwb)
        fslist.append(connected[ipwb])

    pscale=1.
    portlist=[]
    fign=0
    rot=90
    alg='center'
    #xmax=float(len(optdata))
    xmax=float(len(pwblist))
    if 'Avago' in name:
        col='r'
        pscale=1.e-3
        #portlist=sorted(optdata.keys())
        portlist=pwblist
        fign=2
        rot=45
        alg='edge'
        xmax+=1
    elif 'Fiberstore' in name:
        col='b'
        #portlist=sorted(optdata.keys(), key=int)
        portlist=fslist
        fign=1
    else:
        col='k'
    
    #print portlist

    #centers=range(len(optdata))
    centers=range(len(pwblist))
    #xlab=[re.search('[0-9]{1,2}$',port).group() for port in optdata.keys()]
    #print xlab

    fig=plt.figure(fign,figsize=(30, 30),facecolor='w')
    fig.canvas.set_window_title(name)

    plt.subplot(221)
    plt.title(name+' Current',fontsize=16)
    current=[optdata[port]['i'] for port in portlist]
    plt.bar(centers, current, width=1, align=alg, color=col)
    #plt.xticks(centers, portlist, rotation=45, fontsize=8)
    plt.xticks(centers, portlist, rotation=rot, fontsize=8)
    plt.xlim(-1.,xmax)
    plt.ylabel('i [mA]')
    plt.ylim(0.,9.)
    plt.grid(axis='y')
    #print plt.xlim()

    plt.subplot(222)
    plt.title(name+' Output Power',fontsize=16)
    txpow=[optdata[port]['tx'] for port in portlist]
    txpow[:] = [x * pscale for x in txpow]
    plt.bar(centers, txpow, width=1, align=alg, color=col)
    plt.xticks(centers, portlist, rotation=rot, fontsize=8)
    plt.xlim(-1.,xmax)
    plt.ylabel('tx [mW]')
    plt.ylim(0.,0.5)
    plt.grid(axis='y')

    plt.subplot(223)
    plt.title(name+' Temperature',fontsize=16)
    temp=[optdata[port]['T'] for port in portlist]
    plt.bar(centers, temp, width=1, align=alg, color=col)
    plt.xticks(centers, portlist, rotation=rot, fontsize=8)
    plt.xlim(-1.,xmax)
    plt.ylabel('T [deg C]')
    plt.ylim(0.,40.)
    plt.grid(axis='y')

    
    #plt.subplot(324)
    #plt.title(name+' Voltage',fontsize=16)
    bias=[optdata[port]['v'] for port in portlist]
    #plt.bar(centers, bias, width=1, align=alg, color=col)
    #plt.xticks(centers, portlist, rotation=rot, fontsize=8)
    #plt.xlim(-1.,xmax)
    #plt.ylabel('v [V]')
    #plt.ylim(0.,3.5)
    #plt.grid(axis='y')
    

    '''
    plt.subplot(325)
    plt.title(name+' Current vs Output Power',fontsize=16)
    plt.scatter(current,txpow, c=col)
    plt.xlabel('i [mA]')
    plt.xlim(0.,9.)
    plt.ylabel('tx [mW]')
    plt.ylim(0.,0.5)
    plt.grid()
    '''

    plt.subplot(224)
    plt.title(name+' Received Power',fontsize=16)
    rxpow=[optdata[port]['rx'] for port in portlist]
    rxpow[:] = [x * pscale for x in rxpow]
    plt.bar(centers, rxpow, width=1, align=alg, color=col)
    plt.xticks(centers, portlist, rotation=rot, fontsize=8)
    plt.xlim(-1.,xmax)
    plt.ylabel('rx [mW]')
    plt.ylim(0.,0.7)
    plt.grid(axis='y')

    plt.subplots_adjust(left=0.03, bottom=0.04, right=0.99, top=0.96,
                        wspace=0.1, hspace=0.18)



    fig4=plt.figure(4,figsize=(30, 30),facecolor='w')
    fig4.canvas.set_window_title('Correlations Plots')

    if 'Fiberstore' in name:
        plt.subplot(221)
    elif 'Avago' in name:
        plt.subplot(222)
    plt.title(name+' Current vs Output Power',fontsize=16)
    plt.scatter(current,txpow, c=col)
    plt.xlabel('i [mA]')
    plt.xlim(0.,9.)
    plt.ylabel('tx [mW]')
    plt.ylim(0.,0.5)
    plt.grid()

    if 'Fiberstore' in name:
        plt.subplot(223)
    elif 'Avago' in name:
        plt.subplot(224)
    plt.title(name+' Current vs Temperature',fontsize=16)
    plt.scatter(current,temp, c=col)
    plt.xlabel('i [mA]')
    plt.xlim(0.,9.)
    plt.ylabel('T [deg C]')
    plt.ylim(0.,40.)
    plt.grid()

    plt.subplots_adjust(left=0.03, bottom=0.04, right=0.99, top=0.96,
                        wspace=0.1, hspace=0.18)


    fig5=plt.figure(5,figsize=(30, 30),facecolor='w')
    fig5.canvas.set_window_title('i-V Characteristics')

    if 'Fiberstore' in name:
        plt.subplot(321)
    elif 'Avago' in name:
        plt.subplot(322)
    plt.title(name+' Voltage',fontsize=16)
    plt.bar(centers, bias, width=1, align=alg, color=col)
    plt.xticks(centers, portlist, rotation=rot, fontsize=8)
    plt.xlim(-1.,xmax)
    plt.ylabel('v [V]')
    plt.ylim(0.,3.5)
    plt.grid(axis='y')

    if 'Fiberstore' in name:
        plt.subplot(323)
    elif 'Avago' in name:
        plt.subplot(324)
    iv=[a*b for a,b in zip(bias,current)]
    plt.title(name+' Current x Voltage',fontsize=16)
    plt.bar(centers, iv, width=1, align=alg, color=col)
    plt.xticks(centers, portlist, rotation=rot, fontsize=8)
    plt.xlim(-1.,xmax)
    plt.ylabel('i*v [mW]')
    plt.ylim(0.,27.)
    plt.grid(axis='y')

    if 'Fiberstore' in name:
        plt.subplot(325)
    elif 'Avago' in name:
        plt.subplot(326)
    plt.title(name+' Current Vs. Voltage',fontsize=16)
    plt.scatter(current, bias, c=col)
    plt.xlabel('i [mA]')
    plt.xlim(0.,9.)
    plt.ylabel('v [V]')
    plt.ylim(3.15,3.4)
    plt.grid()

    plt.subplots_adjust(left=0.03, bottom=0.04, right=0.99, top=0.96,
                        wspace=0.1, hspace=0.18)

def ExtractMAC(macaddr):
    print '::::: only macs :::::'
    mdata={} # { mac address : port number }
    for line in macaddr.splitlines():
        # find MAC address
        res=re.search('([0-9A-Fa-f]{2}:){5}([0-9A-Fa-f]{2})',line)
        if res:
            # find port number <-- NOT UNIQUE, e.g., local switch
            pp=re.search('[a-z]{2}-[0-9]\/[0-9]\/[0-9]{1,2}',line)
            port=re.search('[0-9]{1,2}$',pp.group()).group()
            #print port, res.group()    
            #mdata[port]=res.group()
            mdata[res.group()]=port
    return mdata


def ParseDHCPconfPWB():
    pwbmac={}
    with open('/etc/dhcp/dhcpd.conf','r') as f:
        for line in f:
            addr=re.search('pwb[0-9]{2}',line)
            if addr:
                addr=addr.group()
                #print addr,
                mac=re.search('([0-9A-Fa-f]{2}:){5}([0-9A-Fa-f]{2})',line)
                if mac:
                    mac=mac.group()
                    #print mac
                    pwbmac[addr]=mac
                #else:
                    #print ''
    return pwbmac

def ParseDHCPconfADC():
    adcmac={}
    with open('/etc/dhcp/dhcpd.conf','r') as f:
        for line in f:
            addr=re.search('adc[0-9]{2}',line)
            if addr:
                addr=addr.group()
                #print addr,
                mac=re.search('([0-9A-Fa-f]{2}:){5}([0-9A-Fa-f]{2})',line)
                if mac:
                    mac=mac.group()
                    #print mac
                    adcmac[addr]=mac
                #else:
                    #print ''
    return adcmac


def ReadBackPWBs():
    key='/Equipment/CTRL/Settings/PWB/modules'
    pwbs=pythonMidas.getString( key ).split()
    odata={} # {pwb : { i:mA, tx:mW, T:Celsius, v:Volts, rx:mW } }
    #var=['i','tx','T','v','rx']
    var=['T','v','i','tx','rx']
    #print pwbs
    for ipwb in pwbs:
        key='/Equipment/CTRL/Readback/%s/sfp'%ipwb
        sfp=pythonMidas.getString( key ).split()
        odata[ipwb]={}
        #print ipwb, 
        for x in range(1,6):
            odata[ipwb][var[-x]]=float(sfp[-x])
            #print '\t', var[-x], ':', float(sfp[-x]),
        #print ''
    return odata


def MatchPort2PWB(macdata,pwbmac):
    '''
    macdata is a dictionary with 
    MAC address of the connected device : juniper port
    from juniper cli show ethernet-switching table

    pwbmac is a dictionary with
    PWB : MAC
    from dhcp.conf

    returns   PWB : juniper port
    '''
    match={}
    for pwb in sorted(pwbmac.keys()):
        try:
            port=macdata[pwbmac[pwb]]
            match[pwb]=port
        except KeyError:
            print pwb, 'with MAC', pwbmac[pwb], 'does not respond'
            continue
    print 'MatchPort2PWB status: ',len(match), ' PWBs connected'
    return match


def FiberLoss(jun,pwb,pwb2p):
    '''
    jun is a dictionary with 
    port : {current, power, etc }

    pwb is a dictionary with
    PWB : {current, power, etc }

    pwb2p is a dictionary with
    PWB : port
    '''
    towardsTPC=[]
    towardsJUN=[]
    rxTPC=[]
    rxJUN=[]
    txTPC=[]
    txJUN=[]
    iTPC=[]
    iJUN=[]
    for i in pwb2p.keys():
        rxpwb=pwb[i]['rx']*1.e-3
        rxTPC.append(rxpwb)
        txpwb=pwb[i]['tx']*1.e-3
        txTPC.append(txpwb)
        ipwb=pwb[i]['i']
        iTPC.append(ipwb)
        rxjun=jun[pwb2p[i]]['rx']
        rxJUN.append(rxjun)
        txjun=jun[pwb2p[i]]['tx']
        txJUN.append(txjun)
        ijun=jun[pwb2p[i]]['i']
        iJUN.append(ijun)
        #towardsTPC.append( pwb[i]['rx']*1.e-3/jun[pwb2p[i]]['tx'] )
        #towardsJUN.append( jun[pwb2p[i]]['rx']/(pwb[i]['tx']*1.e-3) )
        if txjun > 0.:
            towardsTPC.append( rxpwb / txjun )
        if txpwb > 0.:
            towardsJUN.append( rxjun / txpwb )
        

    centers=range(len(pwb2p))
    fig=plt.figure(3,figsize=(20, 20),facecolor='w')
    fig.canvas.set_window_title('FiberLoss')

    plt.subplot(221)
    plt.title('Avago Tx --> Fiberstore Rx', fontsize=18)
    plt.plot(txTPC,rxJUN,'ro',label='Avago --> Fiberstore')
    #plt.plot(txJUN,rxTPC,'bo',label='Fiberstore --> Avago')
    #plt.plot(np.linspace(0.,0.7),np.linspace(0.,0.7),'k-')
    plt.xlabel('tx [mW]')
    plt.xlim(0.,0.7)
    plt.ylabel('rx [mW]')
    plt.ylim(0.,0.7)
    plt.grid()
    #plt.legend(loc='best')

    plt.subplot(222)
    plt.title('Fiberstore Tx--> Avago Rx', fontsize=18)
    plt.plot(txJUN,rxTPC,'bo',label='Fiberstore --> Avago')
    #plt.plot(np.linspace(0.,0.7),np.linspace(0.,0.7),'k-')
    plt.xlabel('tx [mW]')
    plt.xlim(0.,0.7)
    plt.ylabel('rx [mW]')
    plt.ylim(0.,0.7)
    plt.grid()

    '''
    #plt.subplot(211)
    plt.subplot(212)
    plt.plot(centers,towardsTPC,'ro',label='Avago --> Fiberstore')
    plt.plot(centers,towardsJUN,'bo',label='Fiberstore --> Avago')
    plt.xticks(centers, pwb2p.values(), rotation=0, fontsize=12)
    plt.xticks(centers, pwb2p.keys(), rotation=45, fontsize=12)
    plt.xlim(-1,len(pwb2p.keys()))
    plt.ylabel('rx/tx')
    plt.grid()
    plt.legend(loc='best')
    #fig.tight_layout()
    '''

    plt.subplot(223)
    plt.title('Avago Current --> Fiberstore Rx', fontsize=18)
    plt.plot(iTPC,rxJUN,'ro',label='Avago --> Fiberstore')
    plt.xlabel('i [mA]')
    plt.xlim(0.,9.)
    plt.ylabel('rx [mW]')
    plt.ylim(0.,0.7)
    plt.grid()
    #plt.legend(loc='best')

    plt.subplot(224)
    plt.title('Fiberstore Current --> Avago Rx', fontsize=18)
    plt.plot(iJUN,rxTPC,'bo',label='Fiberstore --> Avago')
    plt.xlabel('i [mA]')
    plt.xlim(0.,9.)
    plt.ylabel('rx [mW]')
    plt.ylim(0.,0.7)
    plt.grid()
    

    plt.subplots_adjust(left=0.03, bottom=0.05, right=0.99, top=0.96,
                        wspace=0.25, hspace=0.25)

 



if __name__=='__main__':

    if socket.gethostname() == 'alphagdaq.cern.ch':
        print 'Good! We are on', socket.gethostname()
    else:
        sys.exit('Wrong host %s'%socket.gethostname())

    ###############################################################################

    
    when=time.strftime("%Y%b%d%H%M", time.localtime())    
    #opt,mac=junoscli()
    opt,mac=read_files()
    ###############################################################################
    '''
    #when='2018Apr261639'
    when='2018Apr262137'
    '''

    ###############################################################################

    optdata=ExtractOpticalData(opt)
    #save_obj( optdata, './juniperdata/', 'opticdata'+when )
    
    #optdata=load_obj( './juniperdata/', 'opticdata'+when )

    sfp_data = {}

    for port in sorted(optdata.keys(), key=int):
        sfp_data[port] = "%5.1f %5.2f %5.1f    %5.0f    %5.0f" % (optdata[port]['T'], optdata[port]['v'], optdata[port]['i'], 1000.0*optdata[port]['tx'], 1000.0*optdata[port]['rx'])
    
    for port in sorted(optdata.keys(), key=int):
        print 'port:', port, '\t',
        for var in optdata[port].keys():
            print '%s = %1.3f' % (var, optdata[port][var]), '\t',
        print ''
    
    macdata=ExtractMAC(mac)
    #save_obj( macdata, './juniperdata/', 'junmacdata'+when )
    
    #macdata=load_obj('./juniperdata/', 'junmacdata'+when )
    #for mac in macdata.keys():
        #print 'mac:', mac, '\tport:', macdata[mac]
    
    pwbmac=ParseDHCPconfPWB()
    #for pwb in sorted(pwbmac.keys()):
        #print 'module:', pwb, '\t', pwbmac[pwb]

    adcmac=ParseDHCPconfADC()
    #for adc in sorted(adcmac.keys()):
        #print 'module:', adc, '\t', adcmac[adc]

    ppwb=MatchPort2PWB(macdata,pwbmac)

    pwbsfp=ReadBackPWBs()
    #save_obj( pwbsfp, './juniperdata/', 'pwbsfp'+when )
    #pwbsfp=load_obj( './juniperdata/', 'pwbsfp'+when )
    
    odb_data = {}

    for pwb in sorted(pwbsfp.keys()):
        #print 'module:', pwb, '\t',
        #for var in pwbsfp[pwb].keys():
        #    print '%s = %1.3f' % (var, pwbsfp[pwb][var]), '\t',
        #print ''
        odb_data[pwb] = "%5.1f %5.2f %5.1f    %5.0f    %5.0f" % (pwbsfp[pwb]['T'], pwbsfp[pwb]['v'], pwbsfp[pwb]['i'], pwbsfp[pwb]['tx'], pwbsfp[pwb]['rx'])

    print ' PWB   port        MAC        SFP vcc   temp tx_bias tx_power rx_power  vcc   temp tx_bias tx_power rx_power'
    for ipwb in sorted(ppwb.keys()):
        port = ppwb[ipwb]
        if (not sfp_data.has_key(port)):
            sfp_data[port] = "no sfp data"
        print '%s   %2s   %s | %s | %s' % (ipwb, port, pwbmac[ipwb], sfp_data[port], odb_data[ipwb])

    padc=MatchPort2PWB(macdata,adcmac)

    print ' ADC   port        MAC        SFP vcc   temp tx_bias tx_power rx_power'
    for iadc in sorted(padc.keys()):
        print '%s   %2s   %s | %s' % (iadc, padc[iadc], adcmac[iadc], sfp_data[padc[iadc]])

    #name='Juniper - Fiberstore SFP'
    #plot_optdata(optdata,name,ppwb)
    
    name='PWB - Avago SFP'
    #plot_optdata(pwbsfp,name,ppwb)
    ###############################################################################
    

    #FiberLoss(optdata,pwbsfp,ppwb)
    #plt.show()
