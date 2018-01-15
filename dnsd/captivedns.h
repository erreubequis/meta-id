#ifndef CAPTIVEDNS_h
#define CAPTIVEDNS_h

#define DNS_QR_QUERY 0
#define DNS_QR_RESPONSE 1
#define DNS_OPCODE_QUERY 0

//#define CDNS_BUFFER_SIZE 1024

enum  DNSReplyCode{
  NoError = 0,
  FormError = 1,
  ServerFailure = 2,
  NonExistentDomain = 3,
  NotImplemented = 4,
  Refused = 5,
  YXDomain = 6,
  YXRRSet = 7,
  NXRRSet = 8
};

typedef struct CDNSConnData CDNSConnData ;
typedef struct DNSHeader DNSHeader ;

struct DNSHeader
{
  uint16_t ID;               // identification number
  unsigned char RD : 1;      // recursion desired
  unsigned char TC : 1;      // truncated message
  unsigned char AA : 1;      // authoritive answer
  unsigned char OPCode : 4;  // message_type
  unsigned char QR : 1;      // query/response flag
  unsigned char RCode : 4;   // response code
  unsigned char Z : 3;       // its z! reserved
  unsigned char RA : 1;      // recursion available
  uint16_t QDCount;          // number of question entries
  uint16_t ANCount;          // number of answer entries
  uint16_t NSCount;          // number of authority entries
  uint16_t ARCount;          // number of resource entries
};

//A struct describing a dns connection. 
struct CDNSConnData {
	struct espconn *conn;
	uint32 startTime;
    uint16_t port;
    int currentPacketSize;
    char* buffer;
    unsigned char *domainname;
    unsigned char *requestip;
    DNSHeader* dnsHeader;
    uint32_t ttl;
    uint8 errorReplyCode;

/* from httpd : 
	struct espconn *conn;
	//int remote_port;
	//uint8 remote_ip[4];
	uint32 startTime;
	char requestType;  // HTTP_METHOD_GET | HTTPD_METHOD_POST
	char *url;
	char *getArgs;
	const void *cgiArg;
	void *cgiData;
	void *cgiPrivData; // Used for streaming handlers storing state between requests
	void *cgiResponse; // used for forwarding response to the CGI handler
	int32 hash;		// authentication cookie 
	HttpdPriv *priv;
	cgiSendCallback cgi;
	HttpdPostData *post;
*/
};

void cdnsProcessNextRequest(CDNSConnData *conn);
void cdnsSetErrorReplyCode(CDNSConnData *conn, const uint8 replyCode);
void cdnsSetTTL(CDNSConnData *conn, const uint32_t ttl);

// Returns true if successful, false if there are no sockets available
char cdnsStart(const char* domainName, const uint16_t port);
// stops the DNS server
//void cdnsStop();

// private stuff :
char cdnsRequestIncludesOnlyOneQuestion(CDNSConnData *conn);
/*void ICACHE_FLASH_ATTR cdnsConnectCb(void *arg);
void ICACHE_FLASH_ATTR cdnsReconCb(void *arg, sint8 err);
void ICACHE_FLASH_ATTR cdnsDisconCb(void *arg);
void ICACHE_FLASH_ATTR cdnsRecvCb(void *arg, char *data, unsigned short len);
void ICACHE_FLASH_ATTR cdnsSentCb(void *arg);*/

void cdnsReplyWithIP(CDNSConnData *conn);
void cdnsReplyWithCustomCode(CDNSConnData *conn);
/*

class DNSServer
{
  public:
    DNSServer();
    void processNextRequest();
    void setErrorReplyCode(const DNSReplyCode &replyCode);
    void setTTL(const uint32_t &ttl);

    // Returns true if successful, false if there are no sockets available
    bool start(const uint16_t &port,
              const String &domainName,
              const IPAddress &resolvedIP);
    // stops the DNS server
    void stop();

  private:
    WiFiUDP _udp;
    uint16_t _port;
    String _domainName;
    unsigned char _resolvedIP[4];
    int _currentPacketSize;
    unsigned char* _buffer;
    DNSHeader* _dnsHeader;
    uint32_t _ttl;
    DNSReplyCode _errorReplyCode;

    void downcaseAndRemoveWwwPrefix(String &domainName);
    String getDomainNameWithoutWwwPrefix();
    bool requestIncludesOnlyOneQuestion();
    void replyWithIP();
    void replyWithCustomCode();
};
*/
#endif

