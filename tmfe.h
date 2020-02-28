/********************************************************************\

  Name:         tmfe.h
  Created by:   Konstantin Olchanski - TRIUMF

  Contents:     C++ MIDAS frontend

\********************************************************************/

#ifndef TMFE_H
#define TMFE_H

#include <string>
#include <vector>
//#include <mutex>
//#include "midas.h"
#include "tmvodb.h"

// from midas.h

#define TID_BYTE      1       /**< unsigned byte         0       255    */
#define TID_SBYTE     2       /**< signed byte         -128      127    */
#define TID_CHAR      3       /**< single character      0       255    */
#define TID_WORD      4       /**< two bytes             0      65535   */
#define TID_SHORT     5       /**< signed word        -32768    32767   */
#define TID_DWORD     6       /**< four bytes            0      2^32-1  */
#define TID_INT       7       /**< signed dword        -2^31    2^31-1  */
#define TID_BOOL      8       /**< four bytes bool       0        1     */
#define TID_FLOAT     9       /**< 4 Byte float format                  */
#define TID_DOUBLE   10       /**< 8 Byte float format                  */
#define TID_BITFIELD 11       /**< 32 Bits Bitfield      0  111... (32) */
#define TID_STRING   12       /**< zero terminated string               */
#define TID_ARRAY    13       /**< array with unknown contents          */
#define TID_STRUCT   14       /**< structure with fixed length          */
#define TID_KEY      15       /**< key in online database               */
#define TID_LINK     16       /**< link in online database              */
#define TID_LAST     17       /**< end of TID list indicator            */

/**
System message types */
#define MT_ERROR           (1<<0)     /**< - */
#define MT_INFO            (1<<1)     /**< - */
#define MT_DEBUG           (1<<2)     /**< - */
#define MT_USER            (1<<3)     /**< - */
#define MT_LOG             (1<<4)     /**< - */
#define MT_TALK            (1<<5)     /**< - */
#define MT_CALL            (1<<6)     /**< - */
#define MT_ALL              0xFF      /**< - */

#define MT_ERROR_STR       "ERROR"
#define MT_INFO_STR        "INFO"
#define MT_DEBUG_STR       "DEBUG"
#define MT_USER_STR        "USER"
#define MT_LOG_STR         "LOG"
#define MT_TALK_STR        "TALK"
#define MT_CALL_STR        "CALL"

#define MERROR             MT_ERROR, __FILE__, __LINE__ /**< - */
#define MINFO              MT_INFO,  __FILE__, __LINE__ /**< - */
#define MDEBUG             MT_DEBUG, __FILE__, __LINE__ /**< - */
#define MUSER              MT_USER,  __FILE__, __LINE__ /**< produced by interactive user */
#define MLOG               MT_LOG,   __FILE__, __LINE__ /**< info message which is only logged */
#define MTALK              MT_TALK,  __FILE__, __LINE__ /**< info message for speech system */
#define MCALL              MT_CALL,  __FILE__, __LINE__ /**< info message for telephone call */

#if defined __GNUC__
#define MATTRPRINTF(a, b) __attribute__ ((format (printf, a, b)))
#else
#define MATTRPRINTF(a, b)
#endif

class TMFeError
{
 public:
   int error;
   std::string error_string;

 public:
   TMFeError() { // default ctor for success
      error = 0;
      error_string = "success";
   }

   TMFeError(int status, const std::string& str) { // ctor
      error = status;
      error_string = str;
   }
};

class TMFeCommon
{
 public:
   int EventID;
   int TriggerMask;
   std::string Buffer;
   int Type;
   int Source;
   std::string Format;
   bool Enabled;
   int ReadOn;
   int Period;
   double EventLimit;
   int NumSubEvents;
   int LogHistory;
   std::string FrontendHost;
   std::string FrontendName;
   std::string FrontendFileName;
   std::string Status;
   std::string StatusColor;
   bool Hidden;

 public:
   TMFeCommon(); // ctor
};

TMFeError WriteToODB(const char* odbpath, const TMFeCommon* common);
TMFeError ReadFromODB(const char* odbpath, const TMFeCommon* defaults, TMFeCommon* common);

class TMFeEquipment
{
 public:
   std::string fName;
   TMFeCommon *fCommon;
   TMFeCommon *fDefaultCommon;
   int fBuffer;
   int fSerial;

 public:
   TMVOdb* fOdbEq;          ///< ODB equipment/EQNAME
   TMVOdb* fOdbEqCommon;    ///< ODB equipment/EQNAME/Common
   TMVOdb* fOdbEqSettings;  ///< ODB equipment/EQNAME/Settings
   TMVOdb* fOdbEqVariables; ///< ODB equipment/EQNAME/Variables

 public:
   double fStatEvents;
   double fStatBytes;
   double fStatEpS; // events/sec
   double fStatKBpS; // kbytes/sec (factor 1000, not 1024)

   double fStatLastTime;
   double fStatLastEvents;
   double fStatLastBytes;

 public:
   TMFeEquipment(const char* name); // ctor
   TMFeError Init(TMVOdb* odb, TMFeCommon* defaults); // ctor
   TMFeError SendData(const char* data, int size);
   TMFeError ComposeEvent(char* pevent, int size);
   TMFeError BkInit(char* pevent, int size);
   void* BkOpen(char* pevent, const char* bank_name, int bank_type);
   TMFeError BkClose(char* pevent, void* ptr);
   int BkSize(const char* pevent);
   TMFeError SendEvent(const char* pevent);
   TMFeError ZeroStatistics();
   TMFeError WriteStatistics();
   TMFeError SetStatus(const char* status, const char* color);
};

class TMFeRpcHandlerInterface
{
 public:
   virtual void HandleBeginRun();
   virtual void HandlePauseRun();
   virtual void HandleResumeRun();
   virtual void HandleEndRun();
   virtual std::string HandleRpc(const char* cmd, const char* args);
};

//#define TMFE_LOCK_MIDAS(mfe) std::lock_guard<std::mutex> lock(*mfe->GetMidasLock())

class TMFE
{
 public:
   
   std::string fHostname; ///< hostname where the mserver is running, blank if using shared memory
   std::string fExptname; ///< experiment name, blank if only one experiment defined in exptab

 public:
   int  fDB; ///< ODB database handle
   TMVOdb* fOdbRoot; ///< ODB root

 public:
   bool fShutdown; ///< shutdown was requested by Ctrl-C or by RPC command

 public:   
   std::vector<TMFeEquipment*> fEquipments;
   std::vector<TMFeRpcHandlerInterface*> fRpcHandlers;
   
 private:
   /// TMFE is a singleton class: only one
   /// instance is allowed at any time
   static TMFE* gfMFE;
   //static std::mutex* gfMidasLock;
   
   TMFE(); ///< default constructor is private for singleton classes
   virtual ~TMFE(); ///< destructor is private for singleton classes

 public:
   
   /// TMidasOnline is a singleton class. Call instance() to get a reference
   /// to the one instance of this class.
   static TMFE* Instance();
   
   TMFeError Connect(const char* progname, const char*hostname = NULL, const char*exptname = NULL);
   TMFeError Disconnect();
   TMFeError SetWatchdogSec(int sec);

   //void CreateMidasLock();
   //std::mutex* GetMidasLock();

   void PollMidas(int millisec);
   void MidasPeriodicTasks();

   TMFeError RegisterEquipment(TMFeEquipment*eq);

   TMFeError TriggerAlarm(const char* name, const char* message, const char* aclass);
   TMFeError ResetAlarm(const char* name);

   void Msg(int message_type, const char *filename, int line, const char *routine, const char *format, ...) MATTRPRINTF(6,7);

   void RegisterRpcHandler(TMFeRpcHandlerInterface* handler);

   void SetTransitionSequence(int start, int stop, int pause, int resume);

   static double GetTime(); /// return current time with micro-second precision
};

#endif
/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
