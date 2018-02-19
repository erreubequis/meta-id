#include <esp8266.h>
#include "httpclient.h"

const char* template = "<methodCall><methodName>wp.editPost</methodName><params><param><value>1</value></param><param><value>meta_esp</value></param>"
						"<param><value>ffRK yy9f 8exY 6In0 ofZw P7Dd</value></param><param><value>6033</value></param>"
						"<param><value><struct><member><name>custom_fields</name><value><array><data><value>"
						"<struct><member><name>key</name><value><string>upload_data</string></value></member>"
						"<member><name>value</name><value><string>%s</string></value></member>"
						"</struct></value></data></array></value></member></struct></value></param></params></methodCall>" ;
#if 1
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { ; } while(0)
#endif

void wpXmlRpcCb(char * response_body, int http_status, char * response_headers, int body_size){
	DBG("xml callback : %d - %s  - %d\n",http_status, response_headers, body_size);
	}

void wpXmlRpcSend(char*data){
	char buff[1024];
	os_sprintf(buff,template,buff);
	http_raw_request("www.meta-id.info", 80, 0, "/xmlrpc.php", buff, "Content-Type: text/xml\r\n", wpXmlRpcCb);

}
