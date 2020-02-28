//
// Name: KOtcp.h
// Description: K.O.'s very own socket library
// Author: K.Olchanski
// Date: 11 Aug 1998
// Date: July 2017, exceptions removed
//

#ifndef KOtcpH
#define KOtcpH

#include <string>
#include <vector>

typedef unsigned int KOtcpType;

class KOtcpError
{
 public:
  bool error = false;
  int xerrno = 0;
  std::string message;
 public:
  KOtcpError(); // ctor
  KOtcpError(const char* func, const char* text);
  KOtcpError(const char* func, int xerrno, const char* text);
};

class KOtcpConnection
{
 public: // status flags
  bool fConnected = false;
  std::string fHostname;
  std::string fService;

 public: // settings
  int fConnectTimeoutMilliSec = 5000;
  int fReadTimeoutMilliSec = 5000;
  int fWriteTimeoutMilliSec = 5000;
  bool fHttpKeepOpen = true;

 public: // state
  int fSocket = -1;

 public: // public api
  KOtcpConnection(const char* hostname, const char* service); // ctor
  ~KOtcpConnection(); // dtor

  KOtcpError Connect();
  KOtcpError Close();

  KOtcpError BytesAvailable(int *nbytes);
  KOtcpError WaitBytesAvailable(int wait_time_millisec, int *nbytes);

  KOtcpError WriteString(const std::string& s);
  KOtcpError WriteBytes(const char* ptr, int len);

  KOtcpError ReadString(std::string* s, unsigned max_length);
  KOtcpError ReadHttpHeader(std::string* s);
  KOtcpError ReadBytes(char* ptr, int len);

  KOtcpError HttpGet(const std::vector<std::string>& headers, const char* url, std::vector<std::string> *reply_headers, std::string *reply_body);
  KOtcpError HttpPost(const std::vector<std::string>& headers, const char* url, const std::string& body, std::vector<std::string> *reply_headers, std::string *reply_body);
  KOtcpError HttpPost(const std::vector<std::string>& headers, const char* url, const char* body, int body_length, std::vector<std::string> *reply_headers, std::string *reply_body);
  KOtcpError HttpReadResponse(std::vector<std::string> *reply_headers, std::string *reply_body);

 public: // internal stuff
  int fBufSize = 0; // size of buffer
  int fBufUsed = 0; // bytes stored in buffer
  int fBufPtr  = 0; // first unread byte in buffer
  char* fBuf = NULL;
  bool CopyBuf(std::string *s);
  bool CopyBufHttp(std::string *s);
  KOtcpError ReadBuf();
};

#if 0
class KOsocketAddr
{
public: // public data members
  std::string fProtocol;
  std::string fHostname;
  int      fPort;

public: // public methods
  KOsocketAddr(); // ctor
  std::string toString() const;

private: // private data members
};

class KOsocketBase
{
public:
  KOsocketBase();          // ctor
  virtual ~KOsocketBase(); // dtor

  // socket operations
  void setSockopt(int opt,int value);

  // set the SO_RCVBUF option
  void setReceiveBufferSize(int value);

  // set the SO_SNDBUF option
  void setSendBufferSize(int value);

  // set or clear close-on-exec flag
  void setCloseOnExec(bool flag);

  // read the close-on-exec flag
  bool getCloseOnExec() const;

  // socket shutdown
  void shutdown();

  // socket error reporting
  int         getErrorCode() const;
  std::string getErrorString() const;

  // socket information reporting
  const KOsocketAddr& getLocalAddr()  const;
  const KOsocketAddr& getRemoteAddr() const;

public:
  bool fIsShutdown;

protected:
  KOsocketAddr fLocalAddr;
  KOsocketAddr fRemoteAddr;
  int          fError;
  KOsocketType fSocket;

private: // hide the assignment and copy constructor operators
  KOsocketBase(const KOsocketBase&s) { abort(); };
  KOsocketBase& operator=(const KOsocketBase&s) { abort(); return *this; };
};

class KOsocket;

class KOserverSocket
: public KOsocketBase
{
 public: // public methods
  
  // create listener socket
  KOserverSocket(int iport,int backlog = 1); // ctor
  
  // accept a connection
  KOsocket* accept();
  
 private:
  KOserverSocket(const KOserverSocket&s) { abort(); };
};

class KOsocket
: public KOsocketBase
{
  friend class KOserverSocket;

 public:
  // open a connection to remote host
  KOsocket(const std::string& remoteHost,
	   int remotePort); // ctor

  // send as many bytes as socket send() would send
  int writeSomeBytes(const char*buffer,int bufferSize);

  // Java-style write()
  void write(const char*buffer,int bufferSize);

  // Java-style read()
  int read(char*buffer,int bufferSize);

  // Java-style setSoTimeout()
  void setSoTimeout(int mstimeout);

  // Java-style readFully(), throws exception if failure.
  void readFully(char*buffer,int bufferSize);

  // Java-style "java.net.Socket->InputStream.available()"
  int available();

 private:
  int fTimeout; // in msec

  // these constructors are used
  // by KOserverSocket::accept().
  KOsocket(KOsocketType fd,
	   const KOsocketAddr& remoteAddr); // ctor
};
#endif

#endif
// end file
