/********************************************************************\

  Name:         fBsequencer.cxx

  Contents:     ALPHA data collector for Trap Sequencer 2 data

  $Id$

  $Log$


\********************************************************************/

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <assert.h>
#include <ctype.h>

#undef HAVE_ROOT
#undef USE_ROOT

#include "midas.h"
#include "msystem.h" // rb_get_buffer_level()



#include "tmfe.h"
#include "mfe.h"

#include <vector>
#include <string>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define EVID_SEQUENCER 8

std::vector<std::string> allowed_hosts;

/* make frontend functions callable from the C framework */

/*-- Globals -------------------------------------------------------*/
static MVOdb* gSeqSettings = NULL; // ODB /Equipment/sequencerB/Settings/
static MVOdb* gExpSettings = NULL; // ODB /Equipment/sequencerB/Settings/
/* The frontend name (client name) as seen by other MIDAS clients   */
const char *frontend_name = "fe" SEQID "sequencer";
/* The frontend file name, don't change it */
const  char *frontend_file_name = __FILE__;

/* frontend_loop is called periodically if this variable is TRUE    */
   BOOL frontend_call_loop = FALSE;

/* a frontend status page is displayed with this frequency in ms */
   INT display_period = 000;

/* maximum event size produced by this frontend */
   INT max_event_size = 8*200*1024;

/* maximum event size for fragmented events (EQ_FRAGMENTED) */
   INT max_event_size_frag = 8*1024*1024;

/* buffer size to hold events */
   INT event_buffer_size = 8*1000*1024;

  extern INT run_state;
  extern HNDLE hDB;

/*-- Function declarations -----------------------------------------*/
  int frontend_init();
  int frontend_exit();
  int begin_of_run(int run, char *err);
  int end_of_run(int run, char *err);
  int pause_run(int run, char *err);
  int resume_run(int run, char *err);
  int frontend_loop();
  
  INT read_event(char *pevent, INT off);
  int openListenSocket(int port);

/*-- Bank definitions ----------------------------------------------*/

/*-- Equipment list ------------------------------------------------*/
  
EQUIPMENT equipment[] = {

    {"Sequencer" SEQID,               /* equipment name */
     { EVID_SEQUENCER, (1<<EVID_SEQUENCER), /* event ID, trigger mask */
      "SYSTEM",               /* event buffer */
      EQ_POLLED,              /* equipment type */
      LAM_SOURCE(0, 0xFFFFFF),                      /* event source */
      "MIDAS",                /* format */
      TRUE,                   /* enabled */
      RO_ALWAYS,              /* when to read this data */
      100,                    /* poll for this many ms */
      0,                      /* stop run after this event limit */
      0,                      /* number of sub events */
      0,                      /* whether to log history */
      "", "", "",}
     ,
     read_event,      /* readout routine */
    }
    ,
    
    {""}
  };

/********************************************************************\
              Callback routines for system transitions

  These routines are called whenever a system transition like start/
  stop of a run occurs. The routines are called on the following
  occations:

  frontend_init:  When the frontend program is started. This routine
                  should initialize the hardware.

  frontend_exit:  When the frontend program is shut down. Can be used
                  to releas any locked resources like memory, commu-
                  nications ports etc.

  begin_of_run:   When a new run is started. Clear scalers, open
                  rungates, etc.

  end_of_run:     Called on a request to stop a run. Can send
                  end-of-run event and close run gates.

  pause_run:      When a run is paused. Should disable trigger events.

  resume_run:     When a run is resumed. Should enable trigger events.
\********************************************************************/

//#include "utils.cxx"

/*-- Frontend Init -------------------------------------------------*/

INT frontend_init()
{
   int status;

   setbuf(stdout,NULL);
   setbuf(stderr,NULL);

   allowed_hosts.push_back("localhost");

   std::vector<std::string> name;

   gSeqSettings->Chdir("/Equipment/sequencer" SEQID "/Settings/", true);
   gSeqSettings->RSA("allowed_hosts", &allowed_hosts, true, 0, 0);

   //odbReadString("/Equipment/sequencerB/Settings/allowed_hosts", 0, "localhost", 250);
   //odbResizeArray("/Equipment/sequencerB/Settings/allowed_hosts", TID_STRING, 10);

   //int sz = odbReadArraySize("/Equipment/sequencerB/Settings/allowed_hosts");
   //int last = 0;
   //for (int i=0; i<sz; i++) 
   //{
   //   const char* s = odbReadString("/Equipment/sequencerB/Settings/allowed_hosts", i, NULL, 250);
   //   if (strlen(s) < 1)
   //      continue;
   //   if (s[0] == '#')
   //      continue;
   //   printf("allowed_hosts %d [%s]\n", i, s);
   //   allowed_hosts.push_back(s);
   //   last = i;
   //}

   //if (sz - last < 10)
   //   odbResizeArray("/Equipment/sequencerB/Settings/allowed_hosts", TID_STRING, last+10);

   if (1)
   {
      std::string s = "";
      for (unsigned i=0; i<allowed_hosts.size(); i++)
      {
         if (i>0)
            s += ", ";
         s += allowed_hosts[i];
      }
      cm_msg(MINFO, "frontend_init", "Allowed hosts: %s", s.c_str());
   }

   int listen_port=12020 + ((int)*SEQID-(int)'A');
   gExpSettings->Chdir("/experiment",false);
   std::string experiment_name;
   gExpSettings->RS("name", &experiment_name, false);
   if( !strcmp(experiment_name.c_str(),"agdaq") )
   {
      listen_port+=100;
      gSeqSettings->RI("tcp_port",&listen_port,true);
   }
   status = openListenSocket(listen_port);
   if (status != FE_SUCCESS)
      return status;
   return FE_SUCCESS;
}

static int gHaveRun = 0;

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
  gHaveRun = 0;

  return SUCCESS;
}

/*-- Begin of Run --------------------------------------------------*/

INT begin_of_run(INT run_number, char *error)
{
  gHaveRun = 1;
  printf("begin run %d\n",run_number);

  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/
INT end_of_run(INT run_number, char *error)
{
  static bool gInsideEndRun = false;

  if (gInsideEndRun)
    {
      printf("breaking recursive end_of_run()\n");
      return SUCCESS;
    }

  gInsideEndRun = true;

  gHaveRun = 0;
  printf("end run %d\n",run_number);

  gInsideEndRun = false;

  return SUCCESS;
}

/*-- Pause Run -----------------------------------------------------*/
INT pause_run(INT run_number, char *error)
{
  gHaveRun = 0;

  return SUCCESS;
}

/*-- Resume Run ----------------------------------------------------*/
INT resume_run(INT run_number, char *error)
{
  gHaveRun = 1;

  return SUCCESS;
}

/*-- Frontend Loop -------------------------------------------------*/
INT frontend_loop()
{
  /* if frontend_call_loop is true, this routine gets called when
     the frontend is idle or once between every event */
  return SUCCESS;
}

/********************************************************************\

  Readout routines for different events

\********************************************************************/

#include <fcntl.h>

const int kBufSize = 1000*1024*8;

time_t gPreviousTimestamp = 0;
int gCountTimestampClashes = 1;

struct LabviewSocket
{
  int fd;
  char buf[kBufSize];
  int  bufwptr;
  int  ofd;
  bool haveNewData;
  bool isOpen;

  LabviewSocket(int socket) // ctor
  {
    fd = socket;
    bufwptr = 0;
    haveNewData = true;
    isOpen      = true;
    ofd = 0;

    if (true)
      {
        time_t t = time(0);
        char fname[1024];
        if( t == gPreviousTimestamp )
          {
            //            sprintf(fname,"/home/alpha/online/sequencerB_2012/sequencerB%d_%d.dat",(int)t, gCountTimestampClashes );
            sprintf(fname,"/home/alpha/online/sequencerB/sequencer" SEQID "%d_%d.dat",(int)t, gCountTimestampClashes );
            gCountTimestampClashes++;
          }
        else
          {           
            gCountTimestampClashes = 1;
            //            sprintf(fname,"/home/alpha/online/sequencerB_2012/sequencerB%d_0.dat",(int)t);
            sprintf(fname,"/home/alpha/online/sequencerB/sequencer" SEQID "%d_0.dat",(int)t);
            // ofd = open(fname,O_WRONLY|O_CREAT|O_LARGEFILE,0777);
            // printf("Socket %d data saved to %s\n",socket,fname);
          }

        ofd = open(fname,O_WRONLY|O_CREAT|O_LARGEFILE,0777);
        printf("Socket %d data saved to %s\n",socket,fname);
                
        gPreviousTimestamp = t;
      }
  }

  void closeSocket() // close socket
  {
    if (!isOpen)
      return;

    //cm_msg(MINFO,"CloseLabviewSocket","Socket %d closed",fd);

    if (fd > 0)
      close(fd);
    fd = 0;

    if (ofd > 0)
      close(ofd);
    ofd = 0;

    isOpen = false;
  }

  ~LabviewSocket() // dtor
  {
    if (isOpen)
      closeSocket();
    //cm_msg(MINFO,"DeleteLabviewSocket","Socket %d deleted",fd);
    bufwptr = 0;
    haveNewData = false;
  }

  int tryRead()
  {
    int avail = kBufSize - bufwptr;
    int rd = read(fd,buf+bufwptr,avail);
    //printf("read from socket %d yelds %d\n",fd,rd);
    if (rd == 0) // nothing to read, socket closed at the other end
      return 0;
    if (rd < 0) // error, close socket
      return -1;
    if (ofd > 0)
      write(ofd,buf+bufwptr,rd);
    bufwptr += rd;
    buf[bufwptr] = 0;
    haveNewData = true;
    return rd;
  }

  void flush()
  {
    bufwptr = 0;
    haveNewData = false;
  }
};

LabviewSocket* gSocket = NULL;

int gListenSocket = 0;

#include <sys/socket.h>
#include <netinet/in.h>

int openListenSocket(int port)
{
  gListenSocket = socket(PF_INET,SOCK_STREAM,0);
  
  if (gListenSocket <= 0)
    {
      cm_msg(MERROR,"openListenSocket","Cannot create socket, errno %d (%s)",errno,strerror(errno));
      return FE_ERR_HW;
    }

  int on = 1;
  int status = setsockopt(gListenSocket,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  if (status != 0)
    {
      cm_msg(MERROR,"openListenSocket","Cannot setsockopt(gListenSocket,SOL_SOCKET,SO_REUSEADDR), errno %d (%s)",errno,strerror(errno));
      return FE_ERR_HW;
    }

  struct sockaddr_in xsockaddr;
  xsockaddr.sin_family = AF_INET;
  xsockaddr.sin_port = htons(port);
  xsockaddr.sin_addr.s_addr = INADDR_ANY;

  status = bind(gListenSocket,(sockaddr*)(&xsockaddr),sizeof(xsockaddr));
  if (status != 0)
    {
      cm_msg(MERROR,"openListenSocket","Cannot bind() to port %d, errno %d (%s)",port,errno,strerror(errno));
      return FE_ERR_HW;
    }

  status = listen(gListenSocket,5);
  if (status != 0)
    {
      cm_msg(MERROR,"openListenSocket","Cannot listen() to port %d, errno %d (%s)",port,errno,strerror(errno));
      return FE_ERR_HW;
    }

  printf("bound to port %d\n",port);
  //return FE_ERR_HW;

  return FE_SUCCESS;
}

#include <netdb.h>

void tryAccept()
{
  struct sockaddr_in xsockaddr;
  socklen_t xsockaddrlen = sizeof(xsockaddr);
  int socket = accept(gListenSocket,(sockaddr*)(&xsockaddr),&xsockaddrlen);
  if (socket <= 0)
    {
      cm_msg(MERROR,"openListenSocket","Cannot accept() new connection, errno %d (%s)",errno,strerror(errno));
      return;
    }

 /* check access control list */ 
  if (allowed_hosts.size() > 0) { 
    int allowed = FALSE; 
    struct hostent *remote_phe; 
    char hname[256]; 
    struct in_addr remote_addr; 
 
    /* save remote host address */ 
    memcpy(&remote_addr, &(xsockaddr.sin_addr), sizeof(remote_addr)); 
 
    remote_phe = gethostbyaddr((char *) &remote_addr, 4, PF_INET); 
 
    if (remote_phe == NULL) { 
      /* use IP number instead */ 
      strlcpy(hname, (char *)inet_ntoa(remote_addr), sizeof(hname)); 
    } else 
      strlcpy(hname, remote_phe->h_name, sizeof(hname)); 
 
    /* always permit localhost */ 
    if (strcmp(hname, "localhost.localdomain") == 0) 
      allowed = TRUE; 
    if (strcmp(hname, "localhost") == 0) 
      allowed = TRUE; 
 
    if (!allowed) { 
      for (unsigned i=0 ; i<allowed_hosts.size(); i++) 
        if (strcmp(hname, allowed_hosts[i].c_str()) == 0) { 
          allowed = TRUE; 
          break; 
        } 
    } 
 
    if (!allowed) { 
      static int max_report = 10;  
      if (max_report > 0) {  
        max_report--;  
        if (max_report == 0)  
          cm_msg(MERROR, "tryAccept", "rejecting connection from unallowed host \'%s\', this message will no longer be reported", hname);  
        else  
          cm_msg(MERROR, "tryAccept", "rejecting connection from unallowed host \'%s\'", hname);  
      }  
      close(socket); 
      return;
    } 
  } 

  if (gSocket)
    {
      gSocket->closeSocket();
      delete gSocket;
    }

  gSocket = new LabviewSocket(socket);
}

bool trySelect()
{
  fd_set rset, wset, eset;
  struct timeval timeout;

  int maxsocket = 0;

  timeout.tv_sec = 0;
  timeout.tv_usec = 1000;

  FD_ZERO(&rset);
  FD_ZERO(&wset);
  FD_ZERO(&eset);

  FD_SET(gListenSocket,&rset);
  FD_SET(gListenSocket,&eset);

  if (gListenSocket > maxsocket)
    maxsocket = gListenSocket;

  if (gSocket && gSocket->isOpen)
    {
      FD_SET(gSocket->fd,&rset);
      FD_SET(gSocket->fd,&eset);
      if (gSocket->fd > maxsocket)
	maxsocket = gSocket->fd;
    }

  int ret = select(maxsocket+1,&rset,&wset,&eset,&timeout);
  //printf("try select: maxsocket %d, ret %d!\n",maxsocket,ret);
  if (ret < 0)
    {
      if (errno == EINTR) // Interrupted system call
	return false;

      cm_msg(MERROR,"trySelect","select() returned %d, errno %d (%s)",ret,errno,strerror(errno));
      return false;
    }

  if (FD_ISSET(gListenSocket,&rset))
    tryAccept();

  if (gSocket)
    if (FD_ISSET(gSocket->fd,&rset) || FD_ISSET(gSocket->fd,&eset))
      {
	int ret = gSocket->tryRead();
	if (ret <= 0)
	  gSocket->closeSocket();
      }

  bool haveNewData = false;

  if (gSocket)
    haveNewData |= gSocket->haveNewData;

  //printf("try select: have new data: %d\n",haveNewData);

  return haveNewData;
}

/*-- Trigger event routines ----------------------------------------*/
INT poll_event(INT source, INT count, BOOL test)
/* Polling routine for events. Returns TRUE if event
   is available. If test equals TRUE, don't return. The test
   flag is used to time the polling */
{
  //printf("poll_event %d %d %d!\n",source,count,test);

  for (int i = 0; i < count; i++)
    {
      int lam = trySelect();
      if (lam)
	if (!test)
	  return lam;
    }
  return 0;
}

/*-- Interrupt configuration ---------------------------------------*/
INT interrupt_configure(INT cmd, INT source, PTYPE adr)
{
   switch (cmd) {
   case CMD_INTERRUPT_ENABLE:
     break;
   case CMD_INTERRUPT_DISABLE:
     break;
   case CMD_INTERRUPT_ATTACH:
     break;
   case CMD_INTERRUPT_DETACH:
     break;
   }
   return SUCCESS;
}

/*-- Event readout -------------------------------------------------*/

void decodeData(char*pevent,char*buf,int bufLength)
{
  //printf("decode data [%s] length %d\n",buf,bufLength);
  //printf("decode data [...] length %d\n",bufLength);

  /* send sequencer XML files out as TID_CHAR arrays */

  if (true)
    {
      /* init bank structure */
      bk_init32(pevent);
      char* pdata;
      bk_create(pevent, "SEQ2", TID_CHAR, (void**)&pdata);
      int len = bufLength;
      memcpy(pdata,buf,len);
      pdata += len;
      *pdata++ = 0; // zero terminate the string array
      bk_close(pevent, pdata);
      
      if(0)
    {

      // Grab the first line of the buffer and write it to the ODB
      char header[800];
      for(int i = 0;i<300 && (*buf!=10) && (*buf!=13) && (*buf!=0);i++)
	{
		header[i]=*buf++;
	}

      int status;
      HNDLE hdir = 0;
      HNDLE hkey;
      const char* name="/Experiment/Run Parameters/Comment";
      
      status = db_find_key (hDB, hdir, name, &hkey);
      if (status != SUCCESS)
        {
          cm_msg(MERROR, frontend_name, "Cannot create \'%s\', db_find_key() status %d", name, status);
          return;
        }
       
      status = db_set_data_index(hDB, hkey, &header, sizeof(header), 0, TID_STRING);
      if (status != SUCCESS)
        {
          cm_msg(MERROR, frontend_name, "Cannot write \'%s\'[%d] of type %d to odb, db_set_data_index() status %d", name, 0, TID_STRING, status);
          return;
        }
 
    }     
 
    }
}

INT read_event(char *pevent, INT off)
{
  //printf("read event\n");

  if (!gSocket)
    return 0;

  if (!gSocket->haveNewData)
    return 0;

  const char* term = "</Sequencer2XML>";
  char*end = strstr(gSocket->buf,term);
  
  if (!end)
    {
      // xml file terminator- incomplete
      // data- wait for more data...
      gSocket->haveNewData = false;
      return 0;
    }

  end += strlen(term);
  
  // zero-terminate the data buffer
  *end = 0;
  
  int endoffset = end-gSocket->buf;
  int leftover = gSocket->bufwptr - endoffset - 1;
  
  printf("socket, length %d, end %p, terminated at %d, leftover data %d\n",gSocket->bufwptr,end,endoffset,leftover);

  decodeData(pevent,gSocket->buf,endoffset);
  
  if (leftover > 0)
    {
      // move any leftover data to the beginning of
      // the data buffer using memmove() because
      // source and destination do overlap
      memmove(gSocket->buf,end+1,leftover);
      gSocket->buf[leftover] = 0;
      gSocket->bufwptr = leftover;
      gSocket->haveNewData = true;
      
      printf("leftover bytes %d: [%s]\n",gSocket->bufwptr,gSocket->buf);
    }
  else
    {
      leftover = 0;
      gSocket->buf[leftover] = 0;
      gSocket->bufwptr = leftover;
      gSocket->haveNewData = true;

      if (!gSocket->isOpen)
	{
	  delete gSocket;
	  gSocket = NULL;
	}
    }
  
  // send the data to MIDAS
  return bk_size(pevent);
}



/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
