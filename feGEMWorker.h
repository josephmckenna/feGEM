#include "feGEMClass.h"


#ifndef FEGEM_WORKER_
#define FEGEM_WORKER_

class feGEMWorker :
   public feGEMClass
{
   public:
   MVOdb* fOdbSupervisorSettings;
   feGEMWorker(TMFE* mfe, TMFeEquipment* eq, AllowedHosts* hosts, const char* client_hostname, int debugMode = 0);
   void Init(MVOdb* supervisor_settings_path);

};


#endif
