#include <esp8266.h>
#include "captivedns.h"

#define CDNS_DBG
#ifdef CDNS_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

/* arpa internet macros taken from https://github.com/esp8266/Arduino/blob/master/libraries/Ethernet/src/utility/util.h */
#define htons(x) ( ((x)<< 8 & 0xFF00) | \
                   ((x)>> 8 & 0x00FF) )
#define ntohs(x) htons(x)

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )
#define ntohl(x) htonl(x)


#define MAX_SENDBUFF_LEN 2600

// debug string to identify connection (ip address & port)
// a static string works because callbacks don't get interrupted...
static char connStr[24];


static void debugConn(void *arg, char *what) {
#if 1
  struct espconn *espconn = arg;
  esp_udp *udp = espconn->proto.udp;
  os_sprintf(connStr, "%d.%d.%d.%d:%d ",
    udp->remote_ip[0], udp->remote_ip[1], udp->remote_ip[2], udp->remote_ip[3],
    udp->remote_port);
  DBG("%s %s\n", connStr, what);
#else
  connStr[0] = 0;
#endif
}

//Max amount of connections
#define MAX_CONN 6
static CDNSConnData connData[MAX_CONN];

//Listening connection data
static struct espconn cdnsConn;
static esp_udp cdnsUdp;
//static CaptiveDNS CDNS;

static char CDNSdomainName[64];
static unsigned char CDNSresolvedIP[4];

//static void ICACHE_FLASH_ATTR cdnsSentCb(void *arg);
static void ICACHE_FLASH_ATTR cdnsRecvCb(void *arg, char *data, unsigned short len);

#if 0  // that was TCP stuff

// Retires a connection for re-use
void ICACHE_FLASH_ATTR cdnsRetireConn(CDNSConnData *conn) {
  if (conn->conn && conn->conn->reverse == conn)
    conn->conn->reverse = NULL; // break reverse link

  // log information about the request we handled
  uint32 dt = conn->startTime;
  if (dt > 0) dt = (system_get_time() - dt) / 1000;
  //if (conn->conn && conn->url)
if (conn->conn)
#if 0
    DBG("HTTP %s %s from %s -> %d in %ums, heap=%ld\n",
      conn->requestType == HTTPD_METHOD_GET ? "GET" : "POST", conn->url, conn->priv->from,
      conn->priv->code, dt, (unsigned long)system_get_free_heap_size());
#else
    DBG("CDNS %s %s: %d, %ums, h=%ld\n",
      conn->requestType == HTTPD_METHOD_GET ? "GET" : "POST", conn->url,
      conn->priv->code, dt, (unsigned long)system_get_free_heap_size());
#endif
	if(conn->buffer!=NULL)os_free(conn->buffer);
	conn->dnsHeader=NULL;
/ *
  conn->conn = NULL; // don't try to send anything, the SDK crashes...
  if (conn->cgi != NULL) conn->cgi(conn); // free cgi data
  if (conn->post->buff != NULL) os_free(conn->post->buff);
  conn->cgi = NULL;
  conn->post->buff = NULL;
  conn->post->multipartBoundary = NULL;*/
}

void ICACHE_FLASH_ATTR cdnsConnectCb(void *arg) {
  debugConn(arg, "cdnsConnectCb");
  struct espconn *conn = arg;

  // Find empty conndata in pool
  int i;
  for (i = 0; i<MAX_CONN; i++) if (connData[i].conn == NULL) break;
  //DBG("Con req, conn=%p, pool slot %d\n", conn, i);
  if (i == MAX_CONN) {
    os_printf("CDNS: conn pool overflow!\n");
    espconn_disconnect(conn);
    return;
  }

#if 0
  int num = 0;
  for (int j = 0; j<MAX_CONN; j++) if (connData[j].conn != NULL) num++;
  DBG("%sConnect (%d open)\n", connStr, num + 1);
#endif

//  connData[i].priv = &connPrivData[i];
  connData[i].conn = conn;
  conn->reverse = connData+i;
  //connData[i].priv->headPos = 0;

/*
  esp_udp *udp = conn->proto.udp;
  os_sprintf(connData[i].priv->from, "%d.%d.%d.%d:%d", tcp->remote_ip[0], tcp->remote_ip[1],
      tcp->remote_ip[2], tcp->remote_ip[3], tcp->remote_port);
  connData[i].post = &connPostData[i];
  connData[i].post->buff = NULL;
  connData[i].post->buffLen = 0;
  connData[i].post->received = 0;
  connData[i].post->len = -1;*/
  connData[i].startTime = system_get_time();

  espconn_regist_recvcb(conn, cdnsRecvCb);
/*  espconn_regist_reconcb(conn, cdnsReconCb);
  espconn_regist_disconcb(conn, cdnsDisconCb);
  espconn_regist_sentcb(conn, cdnsSentCb);

  espconn_set_opt(conn, ESPCONN_REUSEADDR | ESPCONN_NODELAY);*/
}


void ICACHE_FLASH_ATTR cdnsDisconCb(void *arg) {
  debugConn(arg, "cdnsDisconCb");
  struct espconn* pCon = (struct espconn *)arg;
  CDNSConnData *conn = (CDNSConnData *)pCon->reverse;
  if (conn == NULL) return; // aborted connection
  cdnsRetireConn(conn);
}

// Callback indicating a failure in the connection. "Recon" is probably intended in the sense
// of "you need to reconnect". Sigh... Note that there is no DisconCb after ReconCb
void ICACHE_FLASH_ATTR cdnsReconCb(void *arg, sint8 err) {
  debugConn(arg, "cdnsReconCb");
  struct espconn* pCon = (struct espconn *)arg;
  CDNSConnData *conn = (CDNSConnData *)pCon->reverse;
  if (conn == NULL) return; // aborted connection
  DBG("%s***** reset, err=%d\n", connStr, err);
  cdnsRetireConn(conn);
}
#endif // that was TCP stuff

//Callback called when there's data available on a socket.
static void ICACHE_FLASH_ATTR cdnsRecvCb(void *arg, char *data, unsigned short len) {
  debugConn(arg, "cdnsRecvCb");
  struct espconn* pCon = arg;
  remot_info *premot = NULL;
  if(espconn_get_connection_info(pCon,&premot,0)==ESPCONN_OK){
  CDNSConnData *conn = (CDNSConnData *)pCon->reverse;
  if (conn == NULL) {
	  DBG("CDNS: aborted connection");
	  return; 
  }
  conn->conn->proto.udp->remote_port = premot->remote_port;
  conn->conn->proto.udp->remote_ip[0] = premot->remote_ip[0];
  conn->conn->proto.udp->remote_ip[1] = premot->remote_ip[1];
  conn->conn->proto.udp->remote_ip[2] = premot->remote_ip[2];
  conn->conn->proto.udp->remote_ip[3] = premot->remote_ip[3];
  //Send the response.
  conn->currentPacketSize=len;
  conn->buffer = os_malloc(conn->currentPacketSize * sizeof(char)+1);
    os_sprintf(conn->buffer, data);
	conn->buffer[conn->currentPacketSize]='\0';
    conn->dnsHeader = (DNSHeader*) conn->buffer;

  cdnsProcessNextRequest(conn);
  }
}
/*
  char sendBuff[MAX_SENDBUFF_LEN];
  httpdSetOutputBuffer(conn, sendBuff, sizeof(sendBuff));

  //This is slightly evil/dirty: we abuse conn->post->len as a state variable for where in the http communications we are:
  //<0 (-1): Post len unknown because we're still receiving headers
  //==0: No post data
  //>0: Need to receive post data
  //ToDo: See if we can use something more elegant for this.

  for (int x = 0; x<len; x++) {
    if (conn->post->len<0) {
      //This byte is a header byte.
      if (conn->priv->headPos != MAX_HEAD_LEN) conn->priv->head[conn->priv->headPos++] = data[x];
      conn->priv->head[conn->priv->headPos] = 0;
      //Scan for /r/n/r/n. Receiving this indicate the headers end.
      if (data[x] == '\n' && (char *)os_strstr(conn->priv->head, "\r\n\r\n") != NULL) {
        //Indicate we're done with the headers.
        conn->post->len = 0;
	conn->post->multipartBoundary = NULL;
        //Reset url data
        conn->url = NULL;
        //Iterate over all received headers and parse them.
        char *p = conn->priv->head;
        while (p<(&conn->priv->head[conn->priv->headPos - 4])) {
          char *e = (char *)os_strstr(p, "\r\n"); //Find end of header line
          if (e == NULL) break;     //Shouldn't happen.
          e[0] = 0;           //Zero-terminate header
          httpdParseHeader(p, conn);  //and parse it.
          p = e + 2;            //Skip /r/n (now /0/n)
        }
        //If we don't need to receive post data, we can send the response now.
        if (conn->post->len == 0) {
          httpdProcessRequest(conn);
        }
      }
    }
    else if (conn->post->len != 0) {
      //This byte is a POST byte.
      conn->post->buff[conn->post->buffLen++] = data[x];
      conn->post->received++;
      if (conn->post->buffLen >= conn->post->buffSize || conn->post->received == conn->post->len) {
        //Received a chunk of post data
        conn->post->buff[conn->post->buffLen] = 0; //zero-terminate, in case the cgi handler knows it can use strings
        //Send the response.
        httpdProcessRequest(conn);
        conn->post->buffLen = 0;
      }
    }
  }*/

/*
//Callback called when the data on a socket has been successfully sent.
static void ICACHE_FLASH_ATTR cdnsSentCb(void *arg) {
  debugConn(arg, "cdnsSentCb");
  struct espconn* pCon = (struct espconn *)arg;
  CDNSConnData *conn = (CDNSConnData *)pCon->reverse;
  if (conn == NULL) return; // aborted connection
  if (conn->dnsHeader == NULL) { //Marked for destruction?
    //os_printf("Closing 0x%p/0x%p->0x%p\n", arg, conn->conn, conn);
    espconn_disconnect(conn->conn); // we will get a disconnect callback
  }
}

*/

char cdnsStart(const char* domainName, const uint16_t port)
{
	struct ip_info info;
	int i;
 wifi_get_ip_info(SOFTAP_IF, &info);
 CDNSresolvedIP[0]= ip4_addr1_16(&info.ip.addr);
 CDNSresolvedIP[1]= ip4_addr2_16(&info.ip.addr);
 CDNSresolvedIP[2]= ip4_addr3_16(&info.ip.addr);
 CDNSresolvedIP[3]= ip4_addr4_16(&info.ip.addr);
/*
  CDNS.ttl = htonl(60);
  CDNS.errorReplyCode = DNSReplyCode::NonExistentDomain;*/
  cdnsConn.type = ESPCONN_UDP;
  cdnsConn.state = ESPCONN_NONE;
  cdnsUdp.local_port = port;
  cdnsConn.proto.udp = &cdnsUdp;
  for (i = 0; i<MAX_CONN; i++) {
    connData[i].conn = NULL;
  }

//  CDNS.port = port;
  os_sprintf(CDNSdomainName, domainName);
/*
  CDNSresolvedIP[0] = info->;
  CDNSresolvedIP[1] = resolvedIP[1];
  CDNSresolvedIP[2] = resolvedIP[2];
  CDNSresolvedIP[3] = resolvedIP[3];
  downcaseAndRemoveWwwPrefix(CDNSdomainName);*/
  DBG("CDNS init, conn=%p\n", &cdnsConn);
//for tcp :   espconn_regist_connectcb(&cdnsConn, cdnsConnectCb);
	//for udp:
	if(espconn_create(&cdnsConn)){
  espconn_regist_recvcb(&cdnsConn, cdnsRecvCb);
//for tcp :     espconn_accept(&cdnsConn);
  DBG("CDNS server created\n");
	}
//  espconn_tcp_set_max_con_allow(&httpdConn, MAX_CONN);/*   uint8_t sock = WiFiClass::getSocket();
/*
    if (sock != NO_SOCKET_AVAIL)
    {
        ServerDrv::startServer(port, sock, UDP_MODE);
        WiFiClass::_server_port[sock] = port;
        CDNS.sock = sock;
        CDNS.port = port;
        return 1;
    }*/
    return 0;
}

void cdnsSetErrorReplyCode(CDNSConnData *conn,uint8 replyCode)
{
  conn->errorReplyCode = replyCode;
}

void cdnsSetTTL(CDNSConnData *conn, const uint32_t ttl)
{
  conn->ttl = htonl(ttl);
}

/* not used on httpd - why use here ? ...
void cdnsStop()
{
  CDNS.udp.stop();
}*/

void cdnsDowncaseAndRemoveWwwPrefix(char*domainName){
	int i;
//  domainName.toLowerCase();
i=0;
while(domainName[i]){
	if (domainName[i]>='A' && domainName[i]<='Z'){
		domainName[i]+='a'-'A';
	}
	i++;
}
//  domainName.replace("www.", "");
if(os_strncmp(domainName,"www.",4)==0){
i=0;
while(domainName[i+4]){
		domainName[i]=domainName[i+4];
}
	i++;
	}
	domainName[i]='\0';
}

void cdnsProcessNextRequest(CDNSConnData *conn){
    if (conn->dnsHeader->QR == DNS_QR_QUERY &&
        conn->dnsHeader->OPCode == DNS_OPCODE_QUERY &&
        cdnsRequestIncludesOnlyOneQuestion(conn) /*&&
        ((os_strcmp(conn.domainName,"*")==0) || 
         os_strcmp(cdnsGetDomainNameWithoutWwwPrefix(conn), conn.domainName )==0))*/
       )
    {
      cdnsReplyWithIP(conn);
    }
    else if (conn->dnsHeader->QR == DNS_QR_QUERY)
    {
      cdnsReplyWithCustomCode(conn);
    }
}

char cdnsRequestIncludesOnlyOneQuestion(CDNSConnData *conn)
{
  return ntohs(conn->dnsHeader->QDCount) == 1 &&
         conn->dnsHeader->ANCount == 0 &&
         conn->dnsHeader->NSCount == 0 &&
         conn->dnsHeader->ARCount == 0;
}

/*

 no need, we always return meta ip
  
char* cdnsGetDomainNameWithoutWwwPrefix(CDNSConnData *conn)
{
  char parsedDomainName [64];
  unsigned char *start = conn.buffer + 12;
  if (*start == 0)
  {
    return parsedDomainName;
  }
  int pos = 0;
  while(true)
  {
    unsigned char labelLength = *(start + pos);
    for(int i = 0; i < labelLength; i++)
    {
      parsedDomainName[pos] += (char)*(start + pos+1);
    pos++;
    }
    pos++;
    if (*(start + pos) == 0)
    {
      downcaseAndRemoveWwwPrefix(parsedDomainName);
      return parsedDomainName;
    }
    else
    {
      parsedDomainName[pos]= ".";
    }
  }
}
*/

void cdnsReplyWithIP(CDNSConnData *conn)
{
	char buffer[128];
	int l=0;
  conn->dnsHeader->QR = DNS_QR_RESPONSE;
  conn->dnsHeader->ANCount = conn->dnsHeader->QDCount;
  conn->dnsHeader->QDCount = conn->dnsHeader->QDCount; 
  //_dnsHeader->RA = 1;  
	l+=os_sprintf(buffer,"%d",conn->currentPacketSize);
// 192 -> pointer ; 12 : offset at 0x00c ; 01: type A query ; 01 class IN
	l+=os_sprintf(buffer+l,"%c%c%c%c%c%c",192,12,0,1,0,1);
// ttl - fixed on 4 chars
	os_sprintf(buffer+l,(char*)conn->ttl);
	l+=4;
	os_sprintf(buffer+l,"%c%c%c%c%c%c",0,4,CDNSresolvedIP[0],CDNSresolvedIP[1],CDNSresolvedIP[2],CDNSresolvedIP[3]);
    // tcp : sint8 status = espconn_sent(conn->conn, (uint8*)buffer, l);
    sint16 status = espconn_sendto(conn->conn,(uint8*)buffer, l);
    if (status != 0) {
      DBG("ERROR! CDNS: espconn_sent returned %d, trying to send %d\n",status, l);
    }
//    conn->priv->sendBuffLen = 0;
  }
/*
  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  _udp.write(_buffer, _currentPacketSize);

  _udp.write((uint8_t)192); //  answer name is a pointer
  _udp.write((uint8_t)12);  // pointer to offset at 0x00c

  _udp.write((uint8_t)0);   // 0x0001  answer is type A query (host address)
  _udp.write((uint8_t)1);

  _udp.write((uint8_t)0);   //0x0001 answer is class IN (internet address)
  _udp.write((uint8_t)1);
 
  _udp.write((unsigned char*)&_ttl, 4);

  // Length of RData is 4 bytes (because, in this case, RData is IPv4)
  _udp.write((uint8_t)0);
  _udp.write((uint8_t)4);
  _udp.write(_resolvedIP, sizeof(_resolvedIP));
  _udp.endPacket();



  #ifdef DEBUG
    DEBUG_OUTPUT.print("DNS responds: ");
    DEBUG_OUTPUT.print(_resolvedIP[0]);
    DEBUG_OUTPUT.print(".");
    DEBUG_OUTPUT.print(_resolvedIP[1]);
    DEBUG_OUTPUT.print(".");
    DEBUG_OUTPUT.print(_resolvedIP[2]);
    DEBUG_OUTPUT.print(".");
    DEBUG_OUTPUT.print(_resolvedIP[3]);
    DEBUG_OUTPUT.print(" for ");
    DEBUG_OUTPUT.println(getDomainNameWithoutWwwPrefix());
  #endif
}
*/


void cdnsReplyWithCustomCode(CDNSConnData *conn)
{
	uint8 buffer[128];
  conn->dnsHeader->QR = DNS_QR_RESPONSE;
  conn->dnsHeader->RCode = (unsigned char) conn->errorReplyCode;
  conn->dnsHeader->QDCount = 0;
	os_memcpy(buffer,conn->buffer, sizeof(DNSHeader));
    int8 status = espconn_sent(conn->conn, buffer, sizeof(DNSHeader));
    if (status != 0) {
      DBG("ERROR! CDNS: espconn_sent returned %d, trying to send %d \n",status, sizeof(DNSHeader));
    }
}

