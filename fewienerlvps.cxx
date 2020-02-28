/********************************************************************\

  Name:         fewienerlvps.cxx
  Created by:   K.Olchanski

  Contents:     Frontend for Wiener low voltage power supply via snmpwalk and snmpset

\********************************************************************/

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include <vector>

#include "tmfe.h"
#include "tmvodb.h"

#include "midas.h"

#define C(x) ((x).c_str())

#define LOCK_ODB()

#if 0
class R14xxet: public TMFeRpcHandlerInterface
{
public:
   void WRAlarm(const std::string &alarm)
   {
      if (b) {
         std::string vv = "Alarm: " + v;
         eq->SetStatus(C(vv), "#FF0000");
         
         std::string aa = eq->fName + " alarm " + v;
         mfe->TriggerAlarm(C(eq->fName), C(aa), "Alarm");
      } else {
         eq->SetStatus("Ok", "#00FF00");
         mfe->ResetAlarm(C(eq->fName));
      }
   }
};
#endif

class WienerLvps: public TMFeRpcHandlerInterface
{
public:
   TMFE* mfe = NULL;
   TMFeEquipment* eq = NULL;
   //TMVOdb* fOdb = NULL;
   TMVOdb* fS = NULL; // Settings
   TMVOdb* fV = NULL; // Variables
   TMVOdb* fR = NULL; // Readback
   TMVOdb* fW = NULL; // Snmpwalk

public: // ODB settings
   std::string fHostname;
   std::string fMibDir;
   std::string fSnmpwalkCommand;
   bool fEnableControl = false;
   bool fIgnoreOidNotIncreasing = false;
   int  fNumOutputs = 0;
   int  fVerbose = 0;
   int  fReadPeriodSec = 15;
   bool fConfOverTemperatureTurnOff = false;
   std::string fConfOverTemperatureScript;
   double fConfMaxFanAirTemperature = 0;

public:
   bool   fCommError = false;
   time_t fFastUpdate = 0;
   bool   fOverTemperature = false;

public: // readout data
   double fReadTime = 0;
   // main status
   int fSysMainSwitch = 0;
   int fSysStatus = 0;
   std::string fSysStatusText;
   // per-output channel data
   std::vector<std::string> fOutputMap;
   std::vector<int> fSwitch;
   std::vector<int> fStatus;
   std::vector<std::string> fStatusText;
   std::vector<double> fSettingsVoltage;
   std::vector<double> fDemandVoltage;
   std::vector<double> fSenseVoltage;
   std::vector<double> fSettingsCurrent;
   std::vector<double> fCurrent;
   std::vector<double> fCurrentLimit;
   std::vector<std::string> fOutputName;
   std::vector<double> fOutputTemperature;
   // additional data
   std::string fIpDynamicAddress;
   std::string fIpStaticAddress;
   std::string fMacAddress;
   std::string fPsSerialNumber;
   std::string fFanSerialNumber;
   double fPsOperatingTime = 0;
   // VME power supply reports fan data
   double fFanOperatingTime = 0;
   double fFanAirTemperature = 0;
   std::vector<double> fFanSpeed;
   // VME power supply reports analog temperature sensors
   std::vector<double> fSensorTemperature;

public:

   void run_command(const char* cmd)
   {
      mfe->Msg(MINFO, "run_command", "Running command: %s", cmd);

      FILE *fp = popen(cmd, "r");
      if (!fp) {

      }

      while (1) {
         char buf[256];
         const char* s = fgets(buf, sizeof(buf)-1, fp);
         if (!s) {
            break;
         }
         mfe->Msg(MINFO, "run_command", "Command output: %s", s);
      }

      pclose(fp);
   }

   int set_snmp_float(const char* name, int index, float value)
   {
      if (!fEnableControl)
	 return SUCCESS;
      
      char str[1024];
      char s[1024];
      
      if (index<0) {
	 s[0] = 0;
      } else {
	 //sprintf(s, ".u%d", index);
	 sprintf(s, ".%s", fOutputMap[index].c_str());
      }
      
      sprintf(str, "snmpset -v 2c -M +%s -m +WIENER-CRATE-MIB -c guru %s %s%s F %f", fMibDir.c_str(), fHostname.c_str(), name, s, value);
      
      mfe->Msg(MINFO, "set_snmp_float", "Set wiener float: %s\n", str);
      
      run_command(str);
      
      return SUCCESS;
   }
   
   int set_snmp_int(const char* name, int index, int value)
   {
      if (!fEnableControl)
	 return SUCCESS;
      
      char str[1024];
      char s[1024];
      
      if (index<0) {
	 s[0] = 0;
      } else {
	 //sprintf(s, ".u%d", index);
	 sprintf(s, ".%s", fOutputMap[index].c_str());
      }
      
      sprintf(str, "snmpset -v 2c -M +%s -m +WIENER-CRATE-MIB -c guru %s %s%s i %d", fMibDir.c_str(), fHostname.c_str(), name, s, value);
      
      mfe->Msg(MINFO, "set_snmp_int", "Set wiener integer: %s\n", str);
      
      run_command(str);
      
      return SUCCESS;
   }

   void set_main_switch(int value)
   {
      set_snmp_int("sysMainSwitch.0", -1, value);
      fV->WI("sysMainSwitch_demand", value);
   }

   void check_temperatures()
   {
      //printf("check_temperatures!\n");
      
      bool alarm = false;

#if 0      
      for (unsigned i=0; i<set.maxSensorTemperature.size(); i++)
	 if (set.maxSensorTemperature[i] > 0) {
	    if (rdb.sensorTemperature[i] > set.maxSensorTemperature[i]) {
	       alarm = true;
	       gSensorTemperatureStatus.Add(i, 30, kRed);
	       gMainStatus.Add(0, 30, kRed);
	       cm_msg(MERROR, frontend_name, "Over temperature condition: sensor %d temperature: %d, limit: %d", i, rdb.sensorTemperature[i], set.maxSensorTemperature[i]);
	    }
	 }
#endif
      
      if (fConfMaxFanAirTemperature > 0) {
         if (fFanAirTemperature > fConfMaxFanAirTemperature) {
            alarm = true;
            if (!fOverTemperature) {
               mfe->Msg(MERROR, "check_temperatures", "Over temperature condition: fan air temperature: %.1f, limit: %.1f", fFanAirTemperature, fConfMaxFanAirTemperature);
            }
         }
      }
      
      if (alarm) {
	 if ((fSysMainSwitch > 0) && fConfOverTemperatureTurnOff) {
	    mfe->Msg(MERROR, "check_temperatures", "Over temperature condition: Turning off the main switch");
	    set_main_switch(0);
	 }
	 
	 if (!fOverTemperature) {
	    fOverTemperature = true;
	    
	    if (fConfOverTemperatureScript.length() > 0) {
	       mfe->Msg(MERROR, "check_temperatures", "Over temperature condition: Running the OverTemperature script: %s", fConfOverTemperatureScript.c_str());
               // the script should not block!
               std::string cmd;
               cmd += "/bin/sh -c ";
               cmd += fConfOverTemperatureScript;
               cmd += " >& /dev/null &";
	       system(cmd.c_str());
	    }
	 }
      } else {
         if (fOverTemperature) {
            mfe->Msg(MINFO, "check_temperatures", "Over temperature condition cleared, fan air temperature: %f, limit: %f", fFanAirTemperature, fConfMaxFanAirTemperature);
         }
	 fOverTemperature = false;
      }
   }

   void UpdateSettings()
   {
      mfe->Msg(MINFO, fHostname.c_str(), "Updating settings!");

      fS->RS("Hostname", 0, &fHostname, true);
      fS->RS("SNMP MIB dir", 0, &fMibDir, true);
      fS->RS("SnmpwalkCommand", 0, &fSnmpwalkCommand, true);
      fS->RI("ReadPeriodSec", 0, &fReadPeriodSec, true);
      fS->RB("IgnoreOidNotIncreasing", 0, &fIgnoreOidNotIncreasing, true);
      fS->RB("EnableControl", 0, &fEnableControl, true);

      int numOutputs = 0;
      fV->RI("NumOutputs", 0, &numOutputs, true);

      fS->RDA("outputVoltage", &fSettingsVoltage, true, numOutputs);
      fS->RDA("currentLimit", &fSettingsCurrent, true, numOutputs);

      ResizeArrays(numOutputs);

#if 0
      // set output voltage limits

      sprintf(str, "/Equipment/%s/Settings/maxVoltage", eq_name);
      
      odbResizeArray(str, TID_FLOAT, set.numOutputs);
      
      set.maxVoltage.resize(set.numOutputs);
      
      for (unsigned i=0; i<num; i++)
	 {
	    set.maxVoltage[i] = odbReadFloat(str, i);
	 }
      
      // set resistance
      
      sprintf(str, "/Equipment/%s/Settings/resistance", eq_name);
      
      odbResizeArray(str, TID_FLOAT, set.numOutputs);
      
      set.resistance.resize(set.numOutputs);
      
      for (unsigned i=0; i<num; i++)
	 {
	    set.resistance[i] = odbReadFloat(str, i);
	 }
      
      for (unsigned i=0; i<num; i++)
	 {
	    set.odbDemand[i] = odbReadFloat(str, i);
	    set.demand[i] = set.odbDemand[i];
	    
	    if (set.resistance[i])
	       {
		  double current = rdb.currents[i];
		  if (current < 0)
		     current = 0;
		  double vcorr = set.resistance[i] * current;
		  printf("chan %d, voltage %f V, current %f A, resistance %f Ohm, correction %f V\n", i, set.demand[i], current, set.resistance[i], vcorr);
		  set.demand[i] += vcorr;
	       }
	    
	    if (set.maxVoltage[i])
	       if (set.demand[i] > set.maxVoltage[i])
		  set.demand[i] = set.maxVoltage[i];
	    
	    if ((rdb.sysMainSwitch > 0) && (i<rdb.demandV.size()) && (i<rdb.indices.size())) {
	       if (fabs(set.demand[i] - rdb.demandV[i]) > 0.001)
		  {
		     printf("mainswitch %d %d, num chan %d %d\n", set.mainSwitch, rdb.sysMainSwitch, set.numOutputs, rdb.numOutputs);
		     set_snmp_float("outputVoltage", rdb.indices[i], set.demand[i]);
		     gDemandStatus.Add(i, 1000, kBlue);
		  }
	    }
	 }
      
      gDemandStatus.Write();
      
      // turn channels on and off
      
      sprintf(str, "/Equipment/%s/Settings/outputSwitch", eq_name);
      
      odbResizeArray(str, TID_INT, set.numOutputs);
      
      set.outputSwitch.resize(set.numOutputs);
      set.outputSwitchPrevious.resize(set.numOutputs);
      
      for (unsigned i=0; i<num; i++)
	 {
	    bool odbTurnOn = false;
	    bool odbTurnOff = false;
	    
	    set.outputSwitch[i] = odbReadInt(str, i);
	    
	    if (!set.outputEnable)
	       set.outputSwitch[i] = 0;
	    
	    if (set.outputSwitch[i] != set.outputSwitchPrevious[i]) {
	       // detect a transition
	       if (set.outputSwitch[i])
		  odbTurnOn = true;
	       else
		  odbTurnOff = true;
	       
	       set.outputSwitchPrevious[i] = set.outputSwitch[i];
	    }
	    
	    if (rdb.switches[i]==0 && odbTurnOff) {
	       // hardware says "off", odb was "on" now is "off", means channel tripped, we need to reset the trip status
	       cm_msg(MINFO, frontend_name, "Clearing the event status on channel %d (HWCH %s), outputStatus 0x%x", i, rdb.names[i].c_str(), rdb.status[i]);
	       set_snmp_int("outputSwitch", rdb.indices[i], 10);
	    }
	    
	    if (set.outputSwitch[i] != rdb.switches[i])
	       {
		  if (set.outputSwitch[i]==1) {
		     if (set.enableSparkMode) {
			// in spark counting mode, set supervisor behaviour to 0 (no trip)
			set_snmp_int("outputSupervisionBehavior", rdb.indices[i], 0);
			
			// in spark counting mode, set trip time to 1
			set_snmp_int("outputTripTimeMaxCurrent", rdb.indices[i], 1);
			
			// clear spark count?
			rdb.sparkCount[i] = 0;
			rdb.sparkTime[i]  = 0;
		     }
		  }
		  
		  set_snmp_int("outputSwitch", rdb.indices[i], set.outputSwitch[i]);
		  
		  gSwitchStatus.Add(i, 1000, kBlue);
	       }
	 }
      
      gSwitchStatus.Write();
#endif

#if 0      
      if (1) { // temperature limits
	 
	 sprintf(str, "/Equipment/%s/Variables/sensorTemperature", eq_name);

	 int s = odbReadArraySize(str);

     if (s > 0) {
	sprintf(str, "/Equipment/%s/Settings/maxSensorTemperature", eq_name);
     
	odbResizeArray(str, TID_INT, s);
	
	set.maxSensorTemperature.resize(s);

	gSensorTemperatureStatus.Resize(s);
	gSensorTemperatureStatus.Write();

	for (int i=0; i<s; i++) {
	   set.maxSensorTemperature[i] = odbReadInt(str, i);
	}
     }
  }

  if (1) { // fan air temperature limits

     sprintf(str, "/Equipment/%s/Variables/fanAirTemperature", eq_name);

     int s = odbReadArraySize(str);

     if (s > 0) {
	sprintf(str, "/Equipment/%s/Settings/maxFanAirTemperature", eq_name);
     
	odbResizeArray(str, TID_INT, s);
	
	set.maxFanAirTemperature.resize(s);

	gFanAirTemperatureStatus.Resize(s);
	gFanAirTemperatureStatus.Write();
	
	for (int i=0; i<s; i++) {
	   set.maxFanAirTemperature[i] = odbReadInt(str, i);
	}
     }
  }

  if (1) {
     sprintf(str, "/Equipment/%s/Settings/OverTemperatureTurnOff", eq_name);
     set.overTemperatureTurnOff = odbReadBool(str, 0, false);
  }

  if (1) {
     sprintf(str, "/Equipment/%s/Settings/OverTemperatureScript", eq_name);
     set.overTemperatureScript = odbReadString(str, 0, "", 250);
  }

  // do not call check_temperatures() from hotlink to avoid recursive call! check_temperatures();
#endif
   }

   void UpdateHardware()
   {
      mfe->Msg(MINFO, fHostname.c_str(), "Updating hardware!");

      for (unsigned i=0; i<fSettingsVoltage.size(); i++) {
         set_snmp_float("outputVoltage", i, fSettingsVoltage[i]);
      }

      for (unsigned i=0; i<fSettingsCurrent.size(); i++) {
         set_snmp_float("outputCurrent", i, fSettingsCurrent[i]);
      }
   }

   void Zero()
   {
      assert((int)fSwitch.size() == fNumOutputs);
      fSysMainSwitch = 0;
      fSysStatus = 0;
      fSysStatusText = "";
      fIpDynamicAddress = "";
      fIpStaticAddress = "";
      fMacAddress = "";
      fPsSerialNumber = "";
      fPsOperatingTime = 0;
      fFanSerialNumber = "";
      fFanOperatingTime = 0;
      fFanAirTemperature = 0;
      for (int i=0; i<fNumOutputs; i++) {
         fOutputMap[i] = "";
         fSwitch[i] = 0;
         fStatus[i] = 0;
         fStatusText[i] = "";
         fDemandVoltage[i] = 0;
         fSenseVoltage[i] = 0;
         fCurrent[i] = 0;
         fCurrentLimit[i] = 0;
         fOutputName[i] = "";
         fOutputTemperature[i] = 0;
      }
   }

   void ResizeArrays(int numoutputs)
   {
      if (numoutputs > fNumOutputs) {
         if (fNumOutputs != 0) {
            mfe->Msg(MINFO, "ResizeArrays", "Number of outputs changed from %d to %d", fNumOutputs, numoutputs);
         }
         for (int i=fNumOutputs; i<numoutputs; i++) {
            fOutputMap.push_back("");
            fSwitch.push_back(0);
            fStatus.push_back(0);
            fStatusText.push_back("");
            fDemandVoltage.push_back(0);
            fSenseVoltage.push_back(0);
            fSettingsCurrent.push_back(0);
            fCurrent.push_back(0);
            fCurrentLimit.push_back(0);
            fOutputName.push_back("");
            fOutputTemperature.push_back(0);
         }
         fNumOutputs = numoutputs;
         assert((int)fSwitch.size() == numoutputs);
      }

      fSettingsVoltage.resize(numoutputs);
      fSettingsCurrent.resize(numoutputs);
   }

   unsigned GetIndex(const char* name)
   {
      const char *ss = strstr(name, ".u");
      if (!ss) {
         return (unsigned)-1;
      }
      //unsigned chan = atoi(ss+2);
      //return chan;
      std::string u = ss+1;
      for (unsigned i=0; i<fOutputMap.size(); i++) {
         if (fOutputMap[i] == u) {
            return i;
         } else if (fOutputMap[i].length() == 0) {
            fOutputMap[i] = u;
            return i;
         }
      }
      // should not be reached
      return (unsigned)-1;
   }

   std::string ReadSnmp()
   {
      std::string walk;

      double start_time = TMFE::GetTime();
      
      Zero();

      //
      // Note: please copy WIENER-CRATE-MIB.txt to /usr/share/snmp/mibs/
      //
      // Control commands:
      //
      // snmpset -v 2c -m +WIENER-CRATE-MIB -c guru daqtmp1 sysMainSwitch.0 i 1
      // snmpset -v 2c -m +WIENER-CRATE-MIB -c guru daqtmp1 outputSwitch.u100 i 1
      // snmpset -v 2c -m +WIENER-CRATE-MIB -c guru daqtmp1 outputVoltage.u100 F 10
      //

      bool read_ok = false;

      char str[1024];
      
      if (fSnmpwalkCommand.length() > 2)
	 sprintf(str, "%s 2>&1", fSnmpwalkCommand.c_str());
      else
	 sprintf(str, "snmpwalk -v 2c -M +%s -m +WIENER-CRATE-MIB -c guru %s crate 2>&1", fMibDir.c_str(), fHostname.c_str());
      
      printf("Read wiener event: %s\n", str);

      //rdb.Clear();

      FILE *fp = popen(str, "r");
      if (fp == NULL) {
	 ss_sleep(200);
	 return walk;
      }

      while (1) {
	 char *s = fgets(str, sizeof(str), fp);
	 if (!s)
	    break;

	 if (fVerbose) {
	    printf("wiener: %s\n", s);
	 }

	 walk += s;

	 char name[1024];

	 s = strstr(str, "No Response from");
	 if (s) {
	    printf("No response from : %s", str);
	    if (!fCommError) {
	       mfe->Msg(MERROR, "ReadAllData", "read_wiener_event: No response from \'%s\': %s", fHostname.c_str(), str);
	       fCommError = true;
	    }
	    continue;
	 }
	 
	 s = strstr(str, "Error: OID not increasing");
	 if (s) {
	    if (fIgnoreOidNotIncreasing) {
	       pclose(fp);
	       return walk;
	    }
	    fCommError = true;
	    printf("Strange SNMP error : %s", str);
	    mfe->Msg(MERROR, "ReadAllData", "read_wiener_event: Strange SNMP error from \'%s\': %s", fHostname.c_str(), str);
	    pclose(fp);
	    return walk;
	 }

	 s = strstr(str, "WIENER-CRATE-MIB::");
	 if (s == NULL)	{
	    fCommError = true;
	    printf("unknown response (no WIENER-CRATE-MIB::) : %s", str);
	    continue;
	 }

	 strcpy(name, s+18);
	 
	 char* q = strstr(name, " = ");
	 if (q == NULL)	{
	    printf("unknown response (no \'=\'): %s", str);
	    continue;
	 }

	 *q = 0;

	 //printf("name [%s]\n", name);

	 if ((s = strstr(str, "No more variables")) != NULL) {
	    continue;
	 } else if ((s = strstr(str, "INTEGER:")) != NULL) {
	    s += 8;
	    while (*s != 0) {
	       if (isdigit(*s))
		  break;
	       if (*s == '-')
		  break;
	       if (*s == '+')
		  break;
	       s++;
	    }
	    
	    int val = atoi(s);
	    //printf("%s = int value %d from %s", name, val, str);
	    //db_set_value(hDB, hRdb, name, &val, sizeof(val), 1, TID_INT);

            if (0) {
	    } else if (strstr(name, "sysMainSwitch")) {
               fSysMainSwitch = val;
	       read_ok = true;
	       fCommError = false;	
	    } else if (strstr(name, "psOperatingTime")) {
               fPsOperatingTime = val;
	    } else if (strstr(name, "fanOperatingTime")) {
               fFanOperatingTime = val;
	    } else if (strstr(name, "fanAirTemperature")) {
               fFanAirTemperature = val;
            } else if (strstr(name, "outputNumber")) {
               ResizeArrays(val);
	    } else if (strstr(name, "outputSwitch")) {
               unsigned chan = GetIndex(name);
               if (chan<fSwitch.size()) {
                  fSwitch[chan] = val;
               }
	    } else if (strstr(name, "groupsSwitch")) {
               //printf("group name [%s]\n", name);
               //rdb.groupsSwitchNames.push_back(name);
	    } else if (strstr(name, "outputMeasurementTemperature.u")) {
               unsigned chan = GetIndex(name);
               if (chan < fOutputTemperature.size()) {
                  fOutputTemperature[chan] = val;
                  //printf("name [%s] chan %d, val %d\n", name, chan, val);
               }
	    } else if (strstr(name, "sensorNumber")) {
               fSensorTemperature.resize(val);
               for (unsigned i=0; i<fSensorTemperature.size(); i++) {
                  fSensorTemperature[i] = 0;
               }
	    } else if (strstr(name, "sensorTemperature")) {
	       char *ss = strstr(name, ".temp");
               if (ss) {
                  unsigned chan = atoi(ss+5);
                  chan = chan-1; // counted from 1: temp1, temp2, etc
                  if (chan < fSensorTemperature.size()) {
                     fSensorTemperature[chan] = val;
                     //printf("name [%s] chan %d, val %d\n", name, chan, val);
                  }
               }
	    } else if (strstr(name, "fanNumberOfFans")) {
               fFanSpeed.resize(val);
               for (unsigned i=0; i<fFanSpeed.size(); i++) {
                  fFanSpeed[i] = 0;
               }
	    } else if (strstr(name, "fanSpeed")) {
	       char *ss = strstr(name, ".");
               if (ss) {
                  unsigned chan = atoi(ss+1);
                  if (chan < fFanSpeed.size()) {
                     fFanSpeed[chan] = val;
                     //printf("name [%s] chan %d, val %d\n", name, chan, val);
                  }
               }
	    }
	 } else if ((s = strstr(str, "Float:")) != NULL) {
	    float val = atof(s + 6);
	    //printf("%s = float value %f\n", name, val);
	    //db_set_value(hDB, hRdb, name, &val, sizeof(val), 1, TID_FLOAT);

	    if (strstr(name, "outputMeasurementCurrent.u")) {
               unsigned chan = GetIndex(name);
               if (chan < fCurrent.size()) {
                  //printf("chan %d current %f\n", chan, val);
                  fCurrent[chan] = val;
               }
	    } else if (strstr(name, "psAux")) {
	       // ignore
            } else if (strstr(name, "outputMeasurementSenseVoltage.u")) {
               unsigned chan = GetIndex(name);
               if (chan < fSenseVoltage.size()) {
                  //printf("chan %d current %f\n", chan, val);
                  fSenseVoltage[chan] = val;
               }
            } else if (strstr(name, "outputMeasurementCurrent.u")) {
               unsigned chan = GetIndex(name);
               if (chan < fCurrent.size()) {
                  //printf("chan %d current %f\n", chan, val);
                  fCurrent[chan] = val;
               }
	    } else if (strstr(name, "outputVoltage.u")) {
               unsigned chan = GetIndex(name);
               if (chan < fDemandVoltage.size()) {
                  //printf("chan %d current %f\n", chan, val);
                  fDemandVoltage[chan] = val;
               }
#if 0
	    } else if (strstr(name, "outputVoltageRiseRate.u")) {
               //unsigned chan = GetIndex(name);
	       //printf("chan %d current %f\n", chan, val);
	       //rdb.rampUpRates.push_back(val);
	    } else if (strstr(name, "outputVoltageFallRate.u")) {
               //unsigned chan = GetIndex(name);
	       //printf("chan %d current %f\n", chan, val);
	       //rdb.rampDownRates.push_back(val);
#endif
	    } else if (strstr(name, "outputCurrent.u")) {
               unsigned chan = GetIndex(name);
	       //printf("chan %d current %f\n", chan, val);
               if (chan < fCurrentLimit.size()) {
                  fCurrentLimit[chan] = val;
               }
	    }
	 } else if ((s = strstr(str, "BITS:")) != NULL)	{
	    uint32_t val = 0;
	    char* ss = s+5;
	    
	    //printf("bits %s\n", ss);
	    
	    int ishift = 0;
	    
	    while (*ss) {
	       while (isspace(*ss))
		  ss++;
	       
	       int xval = 0;
	       
	       if (isdigit(*ss)) {
		  xval = (*ss - '0');
	       } else if ((toupper(*ss) >= 'A') && (toupper(*ss) <= 'F')) {
		  xval = 10 + (toupper(*ss) - 'A');
	       } else {
		  break;
	       }
	       
	       // bits go in reverse order
	       
	       int ival = 0;
	       
	       if (xval&1)
		  ival |= 8;
	       
	       if (xval&2)
		  ival |= 4;
	       
	       if (xval&4)
		  ival |= 2;
	       
	       if (xval&8)
		  ival |= 1;
	       
	       val |= (ival<<ishift);
	       
	       ishift += 4;
	       
	       ss++;
	    }
	    
	    if (1) {
	       char *xss;
	       xss = strchr(ss, '\n');
	       if (xss)
		  *xss = 0;
	       xss = strchr(ss, '\r');
	       if (xss)
		  *xss = 0;
	    }
	    
	    char* text = ss;

	    //printf("%s = bit value 0x%08x from [%s], text [%s]\n", name, val, str, text);
	    
	    if (strstr(name, "sysStatus")) {
               fSysStatus = val;
               fSysStatusText = text;
	    } else if (strstr(name, "outputStatus")) {
               unsigned chan = GetIndex(name);
               if (chan<fStatus.size()) {
                  fStatus[chan] = val;
                  fStatusText[chan] = text;
               }
	    }
	 } else if ((s = strstr(str, "STRING:")) != NULL) {
	    char *ss =  (s + 8);
	    while (isspace(*ss))
	       ss++;
	    if (ss[strlen(ss)-1]=='\n')
	       ss[strlen(ss)-1]=0;

            const char* val = ss;
	    
	    //printf("%s = string value [%s]\n", name, ss);
	    
	    if (strstr(name,"outputName.u")) {
               unsigned chan = GetIndex(name);
               if (chan<fOutputName.size()) {
                  fOutputName[chan] = val;
               }
            } else if (strstr(name,"macAddress")) {
               fMacAddress = val;
            } else if (strstr(name,"psSerialNumber")) {
               fPsSerialNumber = val;
            } else if (strstr(name,"fanSerialNumber")) {
               fFanSerialNumber = val;
            }
	 } else if ((s = strstr(str, "IpAddress:")) != NULL) {
	    char *ss =  (s + 10);
	    while (isspace(*ss))
	       ss++;
	    if (ss[strlen(ss)-1]=='\n')
	       ss[strlen(ss)-1]=0;

            const char* val = ss;
	    //printf("%s = IpAddress value [%s]\n", name, val);
	    
	    if (strstr(name,"ipDynamicAddress")) {
               fIpDynamicAddress = val;
            } else if (strstr(name,"ipStaticAddress")) {
               fIpStaticAddress = val;
            }
	 } else if ((s = strstr(str, " = \"\"")) != NULL) {
	    //db_set_value(hDB, hRdb, name, "", 1, 1, TID_STRING);
	 } else {
	    printf("%s = unknown data type: %s", name, str);
	 }
      }

      pclose(fp);

      double end_time = TMFE::GetTime();

      fReadTime = end_time - start_time;

      printf("read_ok %d, fCommError %d, time %.3f, sysMainSwitch %d, sysStatus 0x%x (%s), fan air temp %.1f\n", read_ok, fCommError, fReadTime, fSysMainSwitch, fSysStatus, fSysStatusText.c_str(), fFanAirTemperature);
      
      check_temperatures();

#if 0
      //printf("sizes: %d %d %d %d %d %d\n", names.size(), switches.size(), status.size(), demandV.size(), senseV.size(), currents.size());
      
      static int numPrev = 0;
      
      int num = rdb.names.size();
      
      //printf("num %d->%d, rdb.names %d, rdb.numOutputs %d, set.demand %d, set.numOutputs %d\n", numPrev, num, (int)rdb.names.size(), rdb.numOutputs, (int)set.demand.size(), set.numOutputs);
      
  if (read_ok && gCommError==0) {
     if (num != numPrev) {
        set_equipment_status(eq_name, "Reconfiguration", "red");
        cm_msg(MINFO, frontend_name, "Number of reported channels changed from %d to %d", numPrev, num);
        numPrev = num;
        gOkToControl = false;
        gNextRead = time(NULL) + 5;
        gFastRead = 10;
        return 0;
     }
     set_eq_status();
  } else {
     //gOkToControl = false;
     set_equipment_status(eq_name, "Communication problem", "red");
  }

  numPrev = num;

  if (num == 0) {
     gOkToControl = false;
  } else {
     if (gOkToControl == false) {
        gActionUpdate = 1;
     }

     gOkToControl = true;
  }

  //printf("OkToControl %d\n", gOkToControl);

  if (num == 0)
     {
        if (rdb.numOutputs == 0) {
           open_hotlink(hDB, hSet);
           return 0;
        }

        num = rdb.numOutputs;

        for (int i=0; i<num; i++)
           {
              rdb.switches.push_back(0);
              rdb.status.push_back(0);
              rdb.statusString.push_back("");
              rdb.demandV.push_back(0);
              rdb.senseV.push_back(0);
              rdb.currents.push_back(0);
              rdb.rampUpRates.push_back(0);
              rdb.rampDownRates.push_back(0);
           }
     }

  rdb.numOutputs = num;

  if (hNames==0)
     {
        bool doWriteNames = false;
        char str[1024];
        sprintf(str, "/Equipment/%s/Settings/Names", eq_name);
        
        int status = db_find_key(hDB, 0, str, &hNames);
        if (status == DB_NO_KEY)
           {
              status = db_create_key(hDB, 0, str, TID_STRING);
              if (status == DB_SUCCESS)
                 status = db_find_key(hDB, 0, str, &hNames);
              doWriteNames = true;
           }
        
        if (status != SUCCESS)
           {
              cm_msg(MERROR, frontend_name, "read_wiener_event: Cannot find or create %s, status %d, exiting", str, status);
              exit(1);
           }

        if (doWriteNames)
           {
              for (int i=0; i<num; i++)
                 {
                    status = db_set_data_index(hDB, hNames, rdb.names[i].c_str(), NAME_LENGTH, i, TID_STRING);
                    assert(status == DB_SUCCESS);
                 }
           }

        if (1)
           {
              HNDLE hKey;
              sprintf(str, "/Equipment/%s/Status/HwNames", eq_name);
        
              int status = db_find_key(hDB, 0, str, &hKey);
              if (status == DB_NO_KEY)
                 {
                    status = db_create_key(hDB, 0, str, TID_STRING);
                    if (status == DB_SUCCESS)
                       status = db_find_key(hDB, 0, str, &hKey);
                 }

              for (int i=0; i<num; i++)
                 {
                    status = db_set_data_index(hDB, hKey, rdb.names[i].c_str(), NAME_LENGTH, i, TID_STRING);
                    assert(status == DB_SUCCESS);
                 }
           }
     }

  if (rdb.sensorTemperature.size()>0 && hTemperatureNames==0)
     {
        bool doWriteNames = false;
        char str[1024];
        sprintf(str, "/Equipment/%s/Settings/Names sensorTemperature", eq_name);
        
        int status = db_find_key(hDB, 0, str, &hTemperatureNames);
        if (status == DB_NO_KEY)
           {
              status = db_create_key(hDB, 0, str, TID_STRING);
              if (status == DB_SUCCESS)
                 status = db_find_key(hDB, 0, str, &hTemperatureNames);
              doWriteNames = true;
           }
        
        if (status != SUCCESS)
           {
              cm_msg(MERROR, frontend_name, "read_wiener_event: Cannot find or create %s, status %d, exiting", str, status);
              exit(1);
           }

        if (doWriteNames)
           {
              for (unsigned i=0; i<rdb.sensorTemperature.size(); i++)
                 {
		    char xname[256];
		    sprintf(xname, "temp%d", i+1);
                    status = db_set_data_index(hDB, hTemperatureNames, xname, NAME_LENGTH, i, TID_STRING);
                    assert(status == DB_SUCCESS);
                 }
           }
     }

  if (rdb.fanSpeed.size()>0 && hFanSpeedNames==0)
     {
        bool doWriteNames = false;
        char str[1024];
        sprintf(str, "/Equipment/%s/Settings/Names fanSpeed", eq_name);
        
        int status = db_find_key(hDB, 0, str, &hFanSpeedNames);
        if (status == DB_NO_KEY)
           {
              status = db_create_key(hDB, 0, str, TID_STRING);
              if (status == DB_SUCCESS)
                 status = db_find_key(hDB, 0, str, &hFanSpeedNames);
              doWriteNames = true;
           }
        
        if (status != SUCCESS)
           {
              cm_msg(MERROR, frontend_name, "read_wiener_event: Cannot find or create %s, status %d, exiting", str, status);
              exit(1);
           }

        if (doWriteNames)
           {
              for (unsigned i=0; i<rdb.fanSpeed.size(); i++)
                 {
		    char xname[256];
		    sprintf(xname, "Fan %d", i+1);
                    status = db_set_data_index(hDB, hFanSpeedNames, xname, NAME_LENGTH, i, TID_STRING);
                    assert(status == DB_SUCCESS);
                 }
           }
     }

  if (rdb.fanAirTemperature.size()>0 && hFanAirTemperatureNames==0)
     {
        bool doWriteNames = false;
        char str[1024];
        sprintf(str, "/Equipment/%s/Settings/Names fanAirTemperature", eq_name);
        
        int status = db_find_key(hDB, 0, str, &hFanAirTemperatureNames);
        if (status == DB_NO_KEY)
           {
              status = db_create_key(hDB, 0, str, TID_STRING);
              if (status == DB_SUCCESS)
                 status = db_find_key(hDB, 0, str, &hFanAirTemperatureNames);
              doWriteNames = true;
           }
        
        if (status != SUCCESS)
           {
              cm_msg(MERROR, frontend_name, "read_wiener_event: Cannot find or create %s, status %d, exiting", str, status);
              exit(1);
           }

        if (doWriteNames)
           {
              for (unsigned i=0; i<rdb.fanAirTemperature.size(); i++)
                 {
		    char xname[256];
		    sprintf(xname, "FanAirTemp%d", i);
                    status = db_set_data_index(hDB, hFanAirTemperatureNames, xname, NAME_LENGTH, i, TID_STRING);
                    assert(status == DB_SUCCESS);
                 }
           }
     }

  if (1)
     {
        HNDLE hkey;
        char str[1024];
        sprintf(str, "/Equipment/%s/Settings/outputSwitch", eq_name);
        
        int status = db_find_key(hDB, 0, str, &hkey);

        if (status != DB_SUCCESS)
           write_data_int(hDB, hSet, "outputSwitch",  num, rdb.switches);
     }

  
  if (1)
     {
        HNDLE hkey;
        char str[1024];
        sprintf(str, "/Equipment/%s/Settings/outputVoltage", eq_name);
        
        int status = db_find_key(hDB, 0, str, &hkey);

        if (status != DB_SUCCESS)
           write_data_float(hDB, hSet, "outputVoltage",  num, rdb.demandV);
     }

  if (1)
     {
        HNDLE hkey;
        char str[1024];
        sprintf(str, "/Equipment/%s/Settings/outputCurrent", eq_name);
        
        int status = db_find_key(hDB, 0, str, &hkey);

        if (status != DB_SUCCESS)
           write_data_float(hDB, hSet, "outputCurrent",  num, rdb.demandI);
     }

#define STATUS_ON                           (1)
#define STATUS_OutputFailureMinSenseVoltage (0x0004)
#define STATUS_OutputFailureMaxCurrent      (0x0020)
#define STATUS_OutputCurrentLimited         (0x0400)
#define STATUS_RAMPUP                       (0x0800)
#define STATUS_RAMPDOWN                     (0x1000)
#define STATUS_ENABLEKILL                   (0x2000)
#define STATUS_outputAdjusting              (0x8000)
#define STATUS_outputConstantVoltage       (0x10000)

  int xnum = set.numOutputs;
  if (xnum == 0)
     xnum = num;

  rdb.sparkCount.resize(xnum);
  rdb.sparkTime.resize(xnum);

  gOutputStatus.Resize(xnum);
  gSwitchStatus.Resize(xnum);
  gStatusStatus.Resize(xnum);
  gDemandStatus.Resize(xnum);
  gMeasuredStatus.Resize(xnum);

  for (int i=0; i<num; i++) {
     if ((rdb.status[i]&STATUS_ON) == 0)
        continue;

     if (rdb.status[i]&(STATUS_OutputCurrentLimited|STATUS_OutputFailureMaxCurrent)) {

        if (rdb.sparkTime[i] == 0) {
           rdb.sparkTime[i] = now;
           rdb.sparkCount[i] ++;
        }

        if (set.enableSparkMode) {
           int age = time(NULL) - rdb.sparkTime[i];

           cm_msg(MINFO, frontend_name, "Spark on channel %d (HWCH %s), outputStatus 0x%x, spark count %d, spark age %d", i, rdb.names[i].c_str(), rdb.status[i], rdb.sparkCount[i], age);

           if (age > set.sparkShutdownTime) {
	      cm_msg(MERROR, frontend_name, "Turning off channel %d (HWCH %s), outputStatus 0x%x, spark count %d, spark age %d", i, rdb.names[i].c_str(), rdb.status[i], rdb.sparkCount[i], age);
              set_snmp_int("outputSwitch", rdb.indices[i], 0); // turn off
	   } else {
              set_snmp_int("outputSwitch", rdb.indices[i], 10); // clear error status
           }

           gNextRead = time(NULL) + 1;
        }
     } else {
        rdb.sparkTime[i] = 0;
     }
  }

  write_data_int(hDB, hVar, "switch",  num, rdb.switches);
  write_data_int(hDB, hVar, "status",  num, rdb.status);
  write_data_string(hDB, hRdb, "outputStatusString",  num, rdb.statusString, 100);
  write_data_float(hDB, hVar, "demandVoltage", num, rdb.demandV);
  write_data_float(hDB, hVar, "senseVoltage",  num, rdb.senseV);
  write_data_float(hDB, hVar, "current",       num, rdb.currents);
  write_data_int(hDB, hVar, "sparkCount",      num, rdb.sparkCount);

  if (rdb.sensorTemperature.size() > 0)
     write_data_int(hDB, hVar, "sensorTemperature", rdb.sensorTemperature.size(), rdb.sensorTemperature);

  if (rdb.fanSpeed.size() > 0)
     write_data_int(hDB, hVar, "fanSpeed", rdb.fanSpeed.size(), rdb.fanSpeed);

  if (rdb.fanAirTemperature.size() > 0)
     write_data_int(hDB, hVar, "fanAirTemperature", rdb.fanAirTemperature.size(), rdb.fanAirTemperature);

  open_hotlink(hDB, hSet);

  if (set.numOutputs != rdb.numOutputs) {
     update_settings();
     return 0;
  }

  static int prevSysMainSwitch = -1;
  if (prevSysMainSwitch != rdb.sysMainSwitch) {
     prevSysMainSwitch = rdb.sysMainSwitch;
     update_settings();
     return 0;
  }

  gMainStatus.Reset();

  gMainSwitchStatus.Reset();
  
  if (gWeSetMainSwitch && set.mainSwitch != rdb.sysMainSwitch) {
     // cannot disable this here - the red alarm will never fire because gWeSetMainSwitch will be always set to false // gWeSetMainSwitch = false;
     gMainSwitchStatus.Add(0, 100, kRed);
  } else if (rdb.sysMainSwitch == 0)
     gMainSwitchStatus.Add(0, 10, kWhite);
  else if (rdb.sysMainSwitch == 1)
     gMainSwitchStatus.Add(0, 20, kGreen);
  else
     gMainSwitchStatus.Add(0, 100, kRed);

  gMainStatus.Add(0, gMainSwitchStatus, 0);

  gOutputStatus.Reset();
  gSwitchStatus.Reset();
  gStatusStatus.Reset();
  gDemandStatus.Reset();
  gMeasuredStatus.Reset();

  for (unsigned i=0; i<rdb.indices.size(); i++) {
     if (set.resistance[i]) {
	double current = rdb.currents[i];
	if (current < 0)
	   current = 0;
	double vcorr = set.resistance[i] * current;
	printf("chan %d, voltage %f / %f V, current %f A, resistance %f Ohm, correction %f V\n", i, set.odbDemand[i], set.demand[i], current, set.resistance[i], vcorr);

	set.demand[i] = set.odbDemand[i] + vcorr;
     }
     
     if (set.maxVoltage[i])
	if (set.demand[i] > set.maxVoltage[i])
	   set.demand[i] = set.maxVoltage[i];

     if (rdb.sysMainSwitch > 0) {
	if (fabs(set.demand[i] - rdb.demandV[i]) > 0.100) {
	   set_snmp_float("outputVoltage", rdb.indices[i], set.demand[i]);
	   
	   gDemandStatus.Add(i, 1000, kBlue);
	   
	   gNextRead = time(NULL);
	   gFastRead = 10;
	}
     }

     if ((rdb.status[i]&(STATUS_RAMPUP|STATUS_RAMPDOWN))) {
        //printf("ramping %d!\n", i);
        gNextRead = time(NULL);
        gFastRead = 10;
     }
  }

  for (int i=0; i<num; i++) {
     if (rdb.switches[i] != set.outputSwitch[i])
        gSwitchStatus.Add(i, 100, kRed);
     else if (rdb.switches[i] == 0)
        gSwitchStatus.Add(i, 10, kWhite);
     else if (rdb.switches[i] == 1)
        gSwitchStatus.Add(i, 20, kGreen);
     else
        gSwitchStatus.Add(i, 100, kRed);

     if ((rdb.status[i]&~STATUS_ENABLEKILL&~STATUS_outputAdjusting&~STATUS_outputConstantVoltage) == 0)
        gStatusStatus.Add(i, 10, kWhite);
     else if ((rdb.status[i]&~STATUS_ENABLEKILL&~STATUS_outputAdjusting&~STATUS_outputConstantVoltage) == STATUS_ON)
        gStatusStatus.Add(i, 20, kGreen);
     else if ((rdb.status[i]&~STATUS_ENABLEKILL&~STATUS_ON&~STATUS_outputAdjusting&~STATUS_outputConstantVoltage) == STATUS_RAMPUP)
        gStatusStatus.Add(i, 40, kBlue);
     else if ((rdb.status[i]&~STATUS_ENABLEKILL&~STATUS_ON&~STATUS_outputAdjusting&~STATUS_outputConstantVoltage) == STATUS_RAMPDOWN)
        gStatusStatus.Add(i, 40, kBlue);
     else if (rdb.sysMainSwitch == 0 && rdb.status[i] == STATUS_OutputFailureMinSenseVoltage) // special condition for VME and LVPS power supplies
        gStatusStatus.Add(i, 10, kWhite);
     else
        gStatusStatus.Add(i, 100, kRed);

     if ((rdb.status[i]&STATUS_ON) == 0)
        gDemandStatus.Add(i, 10, kWhite);
     else if (fabs(set.demand[i] - rdb.demandV[i]) < 1.0)
        gDemandStatus.Add(i, 20, kGreen);
     else
        gDemandStatus.Add(i, 30, kRed);
     
     if ((rdb.status[i]&STATUS_ON) == 0)
        gMeasuredStatus.Add(i, 10, kWhite);
     else if (fabs(set.demand[i] - fabs(rdb.senseV[i])) < 1.0)
        gMeasuredStatus.Add(i, 20, kGreen);
     else
        gMeasuredStatus.Add(i, 30, kYellow);
     
     gOutputStatus.Add(i, gSwitchStatus, i);
     gOutputStatus.Add(i, gStatusStatus, i);
     gOutputStatus.Add(i, gDemandStatus, i);
     gOutputStatus.Add(i, gMeasuredStatus, i);

     gMainStatus.Add(0, gOutputStatus, i);
  }

  gMainStatus.Write();

  if (1) {
     static time_t x = 0;

     bool update = false;

     if (gMainStatus.fStatus[0] <= gDelayedMainStatus.fStatus[0])
	update = true;

     if (gMainStatus.fStatus[0] > gDelayedMainStatus.fStatus[0]) {
	if (x == 0) {
	   x = now + gMainStatusDelay;
	}

	if (now >= x)
	   update = true;

	//printf("status %d -> %d, delay %d, update %d\n", gDelayedMainStatus.fStatus[0], gMainStatus.fStatus[0], x-now, update);
     }

     if (update) {
	gDelayedMainStatus.Reset();
	gDelayedMainStatus.Add(0, gMainStatus, 0);
	gDelayedMainStatus.Write();
        set_eq_status();
	x = 0;
     }
  }

  gMainSwitchStatus.Write();

  gOutputStatus.Write();
  gSwitchStatus.Write();
  gStatusStatus.Write();
  gDemandStatus.Write();
  gMeasuredStatus.Write();
#endif

  return walk;
   }

   void ReadAllData()
   {
      std::string walk = ReadSnmp();
      if (!fCommError && walk.length() > 0) {
	 if (fSysMainSwitch) {
            bool allOff = true;
            bool allOn = true;
            int count = 0;
            for (unsigned i=0; i<fSwitch.size(); i++) {
               if (fSwitch[i]) {
                  allOff = false;
                  count += 1;
               } else {
                  allOn = false;
               }
            }
            if (allOff) {
               eq->SetStatus("Main on, Output off", "white");
            } else if (allOn) {
               eq->SetStatus("On", "#00FF00");
            } else {
               char str[256];
               float fpower=0;
               memset(str,0, sizeof(str));
               for (int i=0; i<12; i++){
                  fpower += fCurrent[i]*fSenseVoltage[i];
               }
               sprintf(str, "%d channels On - Tot. Power:%5.0f[W]", count, fpower);
               eq->SetStatus(str, "#00FF00");
            }
	 } else {
	    eq->SetStatus("Off", "white");
	 }
	 fV->WD("ReadTime", fReadTime);
	 fV->WI("NumOutputs", fNumOutputs);
	 fW->WS("snmpwalk", walk.c_str());
	 fV->WI("sysMainSwitch", fSysMainSwitch);
	 fV->WI("sysStatus", fSysStatus);
	 fR->WS("sysStatus", fSysStatusText.c_str());
	 fR->WSA("outputMap", fOutputMap, 256);
         fV->WIA("switch", fSwitch);
         fV->WIA("status", fStatus);
         fR->WSA("status", fStatusText, 256);
         fV->WDA("demandVoltage", fDemandVoltage);
         fV->WDA("senseVoltage", fSenseVoltage);
         fV->WDA("current", fCurrent);
         fV->WDA("currentLimit", fCurrentLimit);
	 fR->WSA("outputName", fOutputName, 256);
	 fV->WDA("outputTemperature", fOutputTemperature);
         fR->WS("ipDynamicAddress", fIpDynamicAddress.c_str());
         fR->WS("ipStaticAddress", fIpStaticAddress.c_str());
         fR->WS("macAddress", fMacAddress.c_str());
         fR->WS("psSerialNumber", fPsSerialNumber.c_str());
         fV->WD("psOperatingTime", fPsOperatingTime);
         fR->WS("fanSerialNumber", fFanSerialNumber.c_str());
         fV->WD("fanOperatingTime", fFanOperatingTime);
         fV->WD("fanAirTemperature", fFanAirTemperature);
         if (fFanSpeed.size() > 0) {
            fV->WDA("fanSpeed", fFanSpeed);
         }
         if (fSensorTemperature.size() > 0) {
            fV->WDA("sensorTemperature", fSensorTemperature);
         }
      } else {
	 eq->SetStatus("Communication error", "red");
      }
   }

   std::string HandleRpc(const char* xcmd, const char* args)
   {
      mfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", xcmd, args);

      std::string cmd = xcmd;

      if (false) {
      } else if (cmd == "update_settings") {
	 UpdateSettings();
	 UpdateHardware();
	 ReadAllData();
      } else if (cmd == "main_off") {
	 mfe->Msg(MINFO, "TurnOff", "Turning off power supply");
	 set_main_switch(0);
	 ReadAllData();
      } else if (cmd == "main_on") {
	 mfe->Msg(MINFO, "TurnOn", "Turning on power supply");
	 UpdateSettings();
	 UpdateHardware();
	 set_main_switch(1);
	 ReadAllData();
      } else if (cmd == "turn_on") {
         int ichan = atoi(args);
	 UpdateSettings();
	 UpdateHardware();
         set_snmp_int("outputSwitch", ichan, 1);
	 ReadAllData();
      } else if (cmd == "turn_off") {
         int ichan = atoi(args);
         set_snmp_int("outputSwitch", ichan, 0);
	 ReadAllData();
      }

      fFastUpdate = time(NULL) + 30;
      return "OK";
   }
};

static WienerLvps* gPs = NULL;

static void handler(int a, int b, int c, void* d)
{
   //printf("db_watch handler %d %d %d\n", a, b, c);
   cm_msg(MINFO, "handler", "db_watch requested update settings!");
   gPs->UpdateSettings();
}

static void setup_watch(TMFE* mfe, TMFeEquipment* eq, WienerLvps* ps)
{
   gPs = ps;

   std::string path;
   path += "/Equipment/";
   path += eq->fName;
   path += "/Settings";

   HNDLE hkey;
   int status = db_find_key(mfe->fDB, 0, C(path), &hkey);

   //printf("db_find_key status %d\n", status);
   if (status != DB_SUCCESS)
      return;

   status = db_watch(mfe->fDB, hkey, handler, NULL);

   //printf("db_watch status %d\n", status);
}

int main(int argc, char* argv[])
{
   setbuf(stdout, NULL);
   setbuf(stderr, NULL);

   signal(SIGPIPE, SIG_IGN);

   const char* name = argv[1];
   //const char* bank = NULL;

   if (strcmp(name, "lvps01")==0) {
      // good
      //bank = "HV01";
   } else if (strcmp(name, "lvps02")==0) {
      // good
      //bank = "HV01";
   } else if (strcmp(name, "vmeps01")==0) {
      // good
      //bank = "HV02";
   } else if (strcmp(name, "vmeps02")==0) {
      // good
      //bank = "HV02";
   } else {
      printf("Only lvps01 and vmeps01 permitted. Bye.\n");
      return 1;
   }

   TMFE* mfe = TMFE::Instance();

   TMFeError err = mfe->Connect(C(std::string("fewiener_") + name));
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *eqc = new TMFeCommon();
   eqc->EventID = 3;
   eqc->FrontendName = std::string("fewiener_") + name;
   eqc->LogHistory = 1;
   
   TMFeEquipment* eq = new TMFeEquipment(C(std::string("WIENER_") + name));
   eq->Init(mfe->fOdbRoot, eqc);
   eq->SetStatus("Starting...", "white");

   mfe->RegisterEquipment(eq);

   WienerLvps* ps = new WienerLvps;

   ps->mfe = mfe;
   ps->eq = eq;
   //ps->fOdb = MakeOdb(mfe->fDB);
   ps->fS = eq->fOdbEqSettings; // ps->fOdb->Chdir(("Equipment/" + eq->fName + "/Settings").c_str(), true);
   ps->fV = eq->fOdbEqVariables; // ps->fOdb->Chdir(("Equipment/" + eq->fName + "/Variables").c_str(), true);
   ps->fR = eq->fOdbEq->Chdir("Readback", true); // ps->fOdb->Chdir(("Equipment/" + eq->fName + "/Readback").c_str(), true);
   ps->fW = eq->fOdbEq->Chdir("Snmpwalk", true); // ps->fOdb->Chdir(("Equipment/" + eq->fName + "/Snmpwalk").c_str(), true);

   ps->UpdateSettings();

   setup_watch(mfe, eq, ps);

   mfe->RegisterRpcHandler(ps);
   mfe->SetTransitionSequence(-1, -1, -1, -1);

   while (!mfe->fShutdown) {

      ps->ReadAllData();
      
      if (ps->fFastUpdate != 0) {
	 if (time(NULL) > ps->fFastUpdate)
	    ps->fFastUpdate = 0;
      }
      
      if (ps->fFastUpdate) {
	 //mfe->Msg(MINFO, "main", "fast update!");
	 mfe->PollMidas(1000);
	 if (mfe->fShutdown)
	    break;
         } else {
            for (int i=0; i<ps->fReadPeriodSec; i++) {
               mfe->PollMidas(1000);
               if (mfe->fShutdown)
                  break;
            }
            if (mfe->fShutdown)
               break;
      }
   }

   mfe->Disconnect();

   return 0;
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
