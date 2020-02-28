//
// TsSync.cxx
// Timestamp synchronization
// K.Olchanski
//

#include <stdio.h>
#include <math.h>
#include <assert.h> // assert()

#include "TsSync.h"

TsSyncEntry::TsSyncEntry(uint32_t xts, int xepoch, double xtime) // ctor
      : ts(xts), epoch(xepoch), time(xtime)
{
}

TsSyncModule::TsSyncModule() // ctor
{
   fEpochTs  = 0;
   fFreqHz   = 0;
   fEpsSec = 2000*1e-9; // in sec
   fRelEps = 0;
   fBufMax = 100;

   fEpoch    = 0;
   fFirstTs  = 0;
   fPrevTs   = 0;
   fLastTs   = 0;
   fOffsetSec   = 0;
   fPrevTimeSec = 0;
   fLastTimeSec = 0;
   fMaxDtSec = 0;
   fMaxRelDt = 0;

   fSyncedWith  = -1;
   fOverflow = false;
   fDead = false;
}

void TsSyncModule::Print() const
{
   printf("ts 0x%08x, prev 0x%08x, first 0x%08x, offset %f, time %f %f, diff %f, buf %d", fLastTs, fPrevTs, fFirstTs, fOffsetSec, fLastTimeSec, fPrevTimeSec, fLastTimeSec-fPrevTimeSec, (int)fBuf.size());
}

void TsSyncModule::DumpBuf() const
{
   for (unsigned i=0; i<fBuf.size(); i++) {
      printf("XXX TsSyncEntry %d: 0x%08x %d %f\n",
             i,
             fBuf[i].ts,
             fBuf[i].epoch,
             fBuf[i].time);
   }
}

double TsSyncModule::GetTime(uint32_t ts, int epoch) const
{
   // must do all computations in double precision to avoid trouble with 32-bit timestamp wraparound.
   //return ts/fFreqHz - fFirstTs/fFreqHz + fOffsetSec + epoch*2.0*0x80000000/fFreqHz;
   return ts/fFreqHz - fFirstTs/fFreqHz + fOffsetSec + epoch*fEpochTs/fFreqHz;
}

bool TsSyncModule::Add(uint32_t ts)
{
   if (fFirstTs == 0) {
      fFirstTs = ts;
   }

   // ignore duplicate timestamps
   if (ts == fLastTs)
      return false;

   // ignore duplicate timestamps
   if (ts+1 == fLastTs)
      return false;

   // ignore duplicate timestamps
   if (ts == fLastTs+1)
      return false;

   // ignore duplicate timestamps
   if (ts+2 == fLastTs)
      return false;

   // ignore duplicate timestamps
   if (ts == fLastTs+2)
      return false;

   fPrevTs = fLastTs;
   fPrevTimeSec = fLastTimeSec;
   fLastTs = ts;
   
   if (ts < fPrevTs) {
      fEpoch += 1;
   }
   
   // must do all computations in double precision to avoid trouble with 32-bit timestamp wraparound.
   fLastTimeSec = GetTime(fLastTs, fEpoch); // fLastTs/fFreqHz + fOffsetSec + fEpoch*2.0*0x80000000/fFreqHz;
   
   if (fBuf.size() > fBufMax) {
      fOverflow = true;
      return false;
   }

   fBuf.push_back(TsSyncEntry(fLastTs, fEpoch, fLastTimeSec));
   return true;
}

void TsSyncModule::Retime()
{
   for (unsigned i=0; i<fBuf.size(); i++) {
      fBuf[i].time = GetTime(fBuf[i].ts, fBuf[i].epoch);
   }
}

double TsSyncModule::GetDt(unsigned j)
{
   assert(j>0);
   assert(j<fBuf.size());
   return fBuf[j].time - fBuf[j-1].time;
}

unsigned TsSyncModule::FindDt(double dt)
{
   //printf("FindDt: fBuf.size %d\n", (int)fBuf.size());
   assert(fBuf.size() > 0);
   for (unsigned j=fBuf.size()-1; j>1; j--) {
      double jdt = GetDt(j);
      double jdiff = dt - jdt;
      double rdiff = (dt-jdt)/dt;
      double ajdiff = fabs(jdiff);
      double ardiff = fabs(rdiff);

      //printf("size %d, buf %d, jdt %f, dt %f, diff %.0f ns, diff %f\n", (int)fBuf.size(), j, jdt, dt, jdiff*1e9, rdiff);

      if (fEpsSec > 0 && ajdiff < fEpsSec) {
         if (ajdiff > fMaxDtSec) {
            //printf("update max dt %f %f, eps %f\n", ajdiff*1e9, fMaxDtSec*1e9, fEpsSec*1e9);
            fMaxDtSec = ajdiff;
         }
         if (ardiff > fMaxRelDt)
            fMaxRelDt = ardiff;
         //printf("found %d %f\n", j, jdt);
         return j;
      }

      if (fRelEps > 0 && ardiff < fRelEps) {
         if (ajdiff > fMaxDtSec)
            fMaxDtSec = ajdiff;
         if (ardiff > fMaxRelDt)
            fMaxRelDt = ardiff;
         //printf("found %d %f\n", j, jdt);
         return j;
      }
   }
   //printf("not found!\n");
   return 0;
}

TsSync::TsSync() // ctor
{
   fSyncOk = false;
   fSyncFailed = false;
   fTrace = false;
   fOverflow = false;
   fDeadMin = 0;
}

TsSync::~TsSync() // dtor
{
}

void TsSync::SetDeadMin(int dead_min)
{
   fDeadMin = dead_min;
}

void TsSync::Configure(unsigned i, double epoch_ts, double freq_hz, double eps_sec, double rel_eps, int buf_max)
{
   // grow the array if needed
   TsSyncModule m;
   for (unsigned j=fModules.size(); j<=i; j++)
      fModules.push_back(m);
   
   fModules[i].fEpochTs = epoch_ts;
   fModules[i].fFreqHz = freq_hz;
   fModules[i].fEpsSec = eps_sec;
   fModules[i].fRelEps = rel_eps;
   fModules[i].fBufMax = buf_max;
}

void TsSync::CheckSync(unsigned ii, unsigned i)
{
   unsigned ntry = 3;
   unsigned jj = fModules[ii].fBuf.size()-1;
   
   double tt = fModules[ii].GetDt(jj);
   
   unsigned j = fModules[i].FindDt(tt);

   //printf("TsSync::CheckSync: module %d buf %d, dt %f with module %d buf %d\n", ii, jj, tt, i, j);
   
   if (j == 0) {
      if (jj < 2) {
         return;
      }

      jj -= 1;
      tt = fModules[ii].GetDt(jj);
      j = fModules[i].FindDt(tt);

      //printf("TsSync::CheckSync: module %d buf %d, dt %f with module %d buf %d (2nd try)\n", ii, jj, tt, i, j);
      
      if (j == 0) {
         return;
      }
   }
   
   // demand a few more good matches
   for (unsigned itry=1; itry<=ntry; itry++) {
      if (jj-itry == 0)
         return;
      if (j-itry == 0)
         return;
      double xtt = fModules[ii].GetDt(jj-itry);
      double xt  = fModules[i].GetDt(j-itry);
      double dxt = xt - xtt;
      double rdxt = dxt/xt;

      if (fModules[ii].fEpsSec > 0 && fabs(dxt) > fModules[ii].fEpsSec) {
         return;
      }

      if (fModules[ii].fRelEps > 0 && fabs(rdxt) > fModules[ii].fRelEps) {
         return;
      }
   }

   fModules[ii].fSyncedWith = i;

   // check for sync loop
   if (fModules[i].fSyncedWith >= 0)
      fModules[ii].fSyncedWith = fModules[i].fSyncedWith;
   
   double off = fModules[i].fBuf[j].time - fModules[ii].fBuf[jj].time;
   fModules[ii].fOffsetSec = off;

   fModules[ii].Retime();

   printf("TsSync: module %d buf %d synced with module %d buf %d, offset %f\n", ii, jj, i, j, off);

   if (fTrace) {
      Dump();
   }
}

void TsSync::Check(unsigned inew)
{
   assert(fModules[inew].fBuf.size() > 0);

   unsigned min = 0;
   unsigned max = 0;
   
   for (unsigned i=0; i<fModules.size(); i++) {
      unsigned s = fModules[i].fBuf.size();
      if (s > 0) {
         if (min == 0)
            min = s;
         if (s < min)
            min = s;
         if (s > max)
               max = s;
      }
   }

   if (fTrace)
      printf("TsSync::Check: min %d, max %d\n", min, max);

   fMin = min;
   fMax = max;
   
   if (!fSyncOk) {
      if (max > min + fPopThreshold) {
         printf("TsSync: popping old data:\n");
         for (unsigned i=0; i<fModules.size(); i++) {
            printf("TsSync: module %d buf size %d\n", i, (int)fModules[i].fBuf.size());
            if (fModules[i].fBuf.size() > 0) {
               fModules[i].fBuf.pop_front();
            }
         }
         printf("TsSync: popping old data, done.\n");
         return;
      }
   }

   if (min < 3)
      return;

   unsigned sync_with = -1;
   unsigned max_size = 1;
   for (unsigned i=0; i<fModules.size(); i++) {
      if (fModules[i].fBuf.size() > max_size) {
         max_size = fModules[i].fBuf.size();
         sync_with = i;
         break;
      }
   }

   for (unsigned i=0; i<fModules.size(); i++) {
      if (fModules[i].fBuf.size() < 1)
         continue;
      if (inew != i && fModules[inew].fSyncedWith < 0) {
         if (i==sync_with) {
            CheckSync(inew, i);
         }
      }
   }

   if (fTrace) {
      Dump();
   }

   int modules_with_data = 0;
   int no_sync = 0;
   for (unsigned i=0; i<fModules.size(); i++) {
      if (fModules[i].fBuf.size() > 0) {
         modules_with_data++;
         if (fModules[i].fSyncedWith < 0) {
            no_sync += 1;
         }
      }
   }

   if (fTrace)
      printf("modules: %d, with data: %d, sync_with %d, no_sync: %d, min %d, fDeadMin %d, max %d\n", (int)fModules.size(), modules_with_data, sync_with, no_sync, min, fDeadMin, max);

   if (min > fDeadMin && modules_with_data > 1 && no_sync <= 1) {
      // at least one module has data and
      // only one unsynced module (all other modules synced to it)
      if (fTrace)
         printf("sync ok, modules %d, modules_with_data %d, no_sync %d, min %d, max %d, fDeamMin %d\n",
                (int)fModules.size(),
                modules_with_data,
                no_sync,
                min,
                max,
                fDeadMin);
      fSyncOk = true;
   } else if (min > fDeadMin && fModules.size() == 2 && modules_with_data == 1 && no_sync == 1) {
      // total 2 modules, one of them has data, the other one is dead
      fSyncOk = true;
   } else if (min > fDeadMin && fModules.size() == 1 && modules_with_data == 1 && no_sync == 1) {
      // total 1 modules, 1 module has data
      fSyncOk = true;
   }

   if (fSyncOk) {
      int count_dead = 0;
      for (unsigned i=0; i<fModules.size(); i++) {
         if ((fModules[i].fSyncedWith < 0) && (fModules[i].fBuf.size() == 0)) {
            fModules[i].fDead = true;
            count_dead += 1;
         }
      }

      printf("TsSync: synchronization completed, %d dead modules.\n", count_dead);
   }
}

void TsSync::Add(unsigned i, uint32_t ts)
{
   if (fSyncFailed)
      return;

   if (fOverflow)
      return;

   if (0 && fTrace) {
      printf("Add %d, ts 0x%08x\n", i, ts);
   }

   bool added = fModules[i].Add(ts);

   if (!fSyncOk && fModules[i].fOverflow) {
      fOverflow = true;
      fSyncFailed = true;
      printf("TsSync: module %d buffer overflow, synchronization failed.\n", i);
      Dump();
      return;
   }
   
   if (0 && fTrace) {
      printf("module %2d: ", i);
      fModules[i].Print();
      printf("\n");
   }
   
   if (!fSyncOk && added) {
      Check(i);
   }
}

void TsSync::Print() const
{
   printf("TsSync: ");
   printf("min: %d, max: %d, ", fMin, fMax);
   printf("sync_ok: %d, ", fSyncOk);
   printf("sync_failed: %d, ", fSyncFailed);
   printf("overflow: %d", fOverflow);
}

void TsSync::Dump() const
{
   unsigned min = 0;
   unsigned max = 0;
   
   for (unsigned i=0; i<fModules.size(); i++) {
      unsigned s = fModules[i].fBuf.size();
      if (s > 0) {
         if (min == 0)
            min = s;
         if (s < min)
            min = s;
         if (s > max)
               max = s;
      }
   }

   for (unsigned j=1; j<max; j++) {
      printf("buf %2d: ", j);
      for (unsigned i=0; i<fModules.size(); i++) {
         if (j<fModules[i].fBuf.size()) {
            double dt = fModules[i].fBuf[j].time - fModules[i].fBuf[j-1].time;
            printf(" %10.6f", dt);
         } else {
            printf(" %10s", "-");
         }
      }
      printf("\n");
   }
   
   printf("buf %2d: ", -1);
   for (unsigned i=0; i<fModules.size(); i++) {
      printf(" %10d", fModules[i].fSyncedWith);
   }
   printf(" (synced with)\n");
   printf("buf %2d: ", -2);
   for (unsigned i=0; i<fModules.size(); i++) {
      printf(" %10.0f", fModules[i].fMaxDtSec*1e9);
   }
   printf(" (max mismatch, ns)\n");
   printf("buf %2d: ", -3);
   for (unsigned i=0; i<fModules.size(); i++) {
      printf(" %10.1f", fModules[i].fMaxRelDt*1e6);
   }
   printf(" (max relative mismatch, ns/ns*1e6)\n");
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
