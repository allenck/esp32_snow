/* Standard includes. */
#include "string.h"
#include "esp_err.h"
/* lwIP core includes */
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include <netinet/in.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
/* Utils includes. */
#include "esp_log.h"
#include "event.h"
#include "http.h"
#include "webserver.h"
#include "cJSON.h"
#include <dirent.h>
#include "wm8978.h"
#include "web_radio.h"
#include "stream.h"
#include "flash_util.h"

#define TAG "webserver:"


#define GPIO_OUTPUT_IO_0    CONFIG_LED_PIN
#define GPIO_OUTPUT_PIN_SEL  ((1<<GPIO_OUTPUT_IO_0))


static uint32_t http_url_length=0;
static uint32_t http_body_length=0;

static char* http_body=NULL;
static char* http_url=NULL;

static int32_t socket_fd, client_fd;
#define RES_HEAD "HTTP/1.1 200 OK\r\nContent-type: %s\r\nTransfer-Encoding: chunked\r\n\r\n"
static char chunk_len[15];


const static char not_find_page[] = "<!DOCTYPE html>"
      "<html>\n"
      "<head>\n"
      "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
      "	 <link href=\"https://cdn.bootcss.com/bootstrap/4.0.0-alpha.6/css/bootstrap.min.css\" rel=\"stylesheet\">\n"
      "  <script src=\"https://cdn.bootcss.com/bootstrap/4.0.0-alpha.6/js/bootstrap.min.js\"></script>\n"
      "<title>WhyEngineer-ESP32</title>\n"
      "</head>\n"
      "<body>\n"
      "<h1 class=\"container\">Page not Found!</h1>\n"
      "</body>\n"
      "</html>\n";

static int header_value_callback(http_parser* a, const char *at, size_t length){
	// for(int i=0;i<length;i++)
	// 	putchar(at[i]);
	return 0;
}
static int body_callback(http_parser* a, const char *at, size_t length){
    // static uint32_t cnt=0;
    // printf("cnt:%d\n",cnt++);
    //spiRamFifoWrite(at,length);
    http_body=realloc(http_body,http_body_length+length);
    memcpy(http_body+http_body_length,at,length);
    http_body_length+=length;
  //   for(int i=0;i<length;i++)
		// putchar(at[i]);
    return 0;
}
static int url_callback(http_parser* a, const char *at, size_t length){
    // static uint32_t cnt=0;
    // printf("cnt:%d\n",cnt++);
    //spiRamFifoWrite(at,length);
    http_url=realloc(http_url,http_url_length+length);
    memcpy(http_url+http_url_length,at,length);
    http_url_length+=length;
  //   for(int i=0;i<length;i++)
		// putchar(at[i]);
    return 0;
}
static void chunk_end(int socket){
	write(socket, "0\r\n\r\n", 7);
}

typedef struct
{
  char* url;
  void(*handle)(http_parser* a,char*url,char* body);
}HttpHandleTypeDef;

char* web_indexPage = "/sdcard/www/index.html";

void web_index(http_parser* a,char*url,char* body);
void led_ctrl(http_parser* a,char*url,char* body);
void load_logo(http_parser* a,char*url,char* body);
void load_esp32(http_parser* a,char*url,char* body);
void rest_readdir(http_parser* a,char*url,char* body);
void rest_readwav(http_parser* a,char*url,char* body);
void load_3d_show(http_parser* a,char*url,char* body);
void process_cmd(http_parser* a,char*url,char* body);
void process_station(http_parser* a,char*url,char* body);
void load_stations(http_parser* a,char*url,char* body);
void query_station(http_parser* a,char*url,char* body);

static void not_found();
static struct webserver_params* params = NULL;

const HttpHandleTypeDef http_handle[]={
	{"/",web_index},
	{"/api/led/",led_ctrl},
	{"/static/logo.png",load_logo},
	{"/static/esp32.png",load_esp32},
    {"/api/readdir/",rest_readdir},
    {"/api/readwav/",rest_readwav},
    {"/3d_show.html",load_3d_show},
    {"/api/command/", process_cmd},
    {"/api/setStation/", process_station},
    {"/stations.json", load_stations},
    {"/api/queryStation/", query_station},
};
static void return_file(char* filename){
	uint32_t r;
	char* read_buf=malloc(1024);
  	FILE* f = fopen(filename, "r");
  	if(f==NULL){
  		ESP_LOGE(TAG,"file not found: %s", filename);
  		return;
  	}
  	while(1){
    	r=fread(read_buf,1,1024,f);
    	if(r>0){
    		sprintf(chunk_len,"%x\r\n",r);
    		write(client_fd, chunk_len, strlen(chunk_len));
	    	//printf("%s",dst_buf);
	    	write(client_fd, read_buf, r);
	    	write(client_fd, "\r\n", 2);
    	}else
    		break;
    }
    fclose(f);
  	chunk_end(client_fd);
}
static void not_found(){
    ESP_LOGE(TAG, "url not found");
	char *request;
  	asprintf(&request,RES_HEAD,"text/html");//html
  	write(client_fd, request, strlen(request));
  	free(request);
  	sprintf(chunk_len,"%x\r\n",strlen(not_find_page));
  	write(client_fd, chunk_len, strlen(chunk_len));
  	write(client_fd, not_find_page, strlen(not_find_page));
  	write(client_fd,"\r\n",2);
  	chunk_end(client_fd);
}
void load_logo(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"image/png");//html
  	write(client_fd, request, strlen(request));
  	free(request);
  	return_file("/sdcard/www/static/logo.png");
}
void load_3d_show(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"text/html");//html
  	write(client_fd, request, strlen(request));
  	free(request);
  	return_file("/sdcard/www/3d_show.html");
}
void process_cmd(http_parser* a,char*url,char* body)
{
 uint16_t voll = WM8978_Read_Reg(52);
 uint16_t volr = WM8978_Read_Reg(53);

 ESP_LOGI(TAG,"volume_ctrl called %s %s", url, body);
 char *request;
 asprintf(&request,RES_HEAD,"application/json");//json
 write(client_fd, request, strlen(request));
 free(request);
 cJSON *root=NULL;
 root= cJSON_Parse(http_body);
 uint16_t cmd=cJSON_GetObjectItem(root,"cmd")->valueint;
 cJSON_Delete(root);
 switch(cmd)
 {
 case 1024: // volume up
     WM8978_HPvol_Set(voll + 10, volr+10);
     break;
 case 2048: // volume down
     WM8978_HPvol_Set(voll - 10, volr-10);
     break;
 case 512:
     // volume off
     WM8978_HPvol_Set(0, 0);
     break;
 default:
     ESP_LOGE(TAG, "unrecognized command %d", cmd);
 }
 root=NULL;
 root=cJSON_CreateObject();
 if(root==NULL){
     ESP_LOGI(TAG,"cjson root create failed\n");
     return NULL;
 }
 cJSON_AddNumberToObject(root,"err",0);
 // cJSON_AddStringToObject(root,"cuid","esp32_whyengineer");
 // cJSON_AddStringToObject(root,"token",access_token);
 // cJSON_AddNumberToObject(root, "rate", 8000);
 // cJSON_AddNumberToObject(root, "channel", 1);
 // cJSON_AddNumberToObject(root, "len", len);
 // cJSON_AddStringToObject(root,"speech",speech);
 voll = WM8978_Read_Reg(52);
 volr = WM8978_Read_Reg(53);
 cJSON_AddNumberToObject(root, "voll",voll );
 cJSON_AddNumberToObject(root, "volr",volr & 0xff );

 char* out = cJSON_PrintUnformatted(root);
 ESP_LOGI(TAG, "out:%s", out);
 sprintf(chunk_len,"%x\r\n",strlen(out));
 write(client_fd, chunk_len, strlen(chunk_len));
 write(client_fd, out, strlen(out));
     free(out);
 write(client_fd,"\r\n",2);
 chunk_end(client_fd);
 //send(client,out,strlen(out),MSG_WAITALL);
 //printf("handle_return: %s\n", out);
 cJSON_Delete(root);

}
void process_station(http_parser* a,char*url,char* body)
{
    ESP_LOGI(TAG,"set station called %s %s", url, body);
    char *request;
    asprintf(&request,RES_HEAD,"application/json");//json
    write(client_fd, request, strlen(request));
    free(request);
    cJSON *root=NULL;
    root= cJSON_Parse(http_body);

    char *station=cJSON_GetObjectItem(root,"station")->valuestring;
    ESP_LOGI(TAG, "received request: %s", station);
   // http_client_get(station, params->settings, NULL);

    EventBits_t bits;
    bits = xEventGroupGetBits(params->eventGroup);
    xEventGroupClearBits( params->eventGroup, BIT0 | BIT2);

    if(params->decodeTaskHandle == NULL)
        ESP_LOGE(TAG, "decodeTaskHandle is NULL!");
    //vTaskDelete(params->decodeTaskHandle);
    xEventGroupWaitBits(params->eventGroup,BIT1, true,false, 600/ portTICK_RATE_MS);
    vTaskDelete(params->webRadioTaskHandle);
    params->station = (char*)malloc(strlen(station+1));
    strcpy(params->station, station);
    xTaskCreate(web_radio_task, "web_radio_task", 4096, params, 5, &params->webRadioTaskHandle);
    xEventGroupWaitBits(params->eventGroup,BIT2, true,false, 600/ portTICK_RATE_MS);

   ESP_ERROR_CHECK( write_flash("web_radio", "station", station));

    char* out = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "out:%s", out);
    sprintf(chunk_len,"%x\r\n",strlen(out));
    write(client_fd, chunk_len, strlen(chunk_len));
    write(client_fd, out, strlen(out));
        free(out);
    write(client_fd,"\r\n",2);
    chunk_end(client_fd);
    //send(client,out,strlen(out),MSG_WAITALL);
    //printf("handle_return: %s\n", out);
    cJSON_Delete(root);
}
void query_station(http_parser* a,char*url,char* body)
{
    uint16_t voll = WM8978_Read_Reg(52);
    uint16_t volr = WM8978_Read_Reg(53);

    ESP_LOGI(TAG,"volume_ctrl called %s %s", url, body);
    char *request;
    asprintf(&request,RES_HEAD,"application/json");//json
    write(client_fd, request, strlen(request));
    free(request);
    cJSON *root=NULL;
    root=cJSON_CreateObject();
    if(root==NULL){
        ESP_LOGI(TAG,"cjson root create failed\n");
        return NULL;
    }
    cJSON_AddNumberToObject(root,"err",0);
    voll = WM8978_Read_Reg(52);
    volr = WM8978_Read_Reg(53);
    cJSON_AddNumberToObject(root, "voll",voll );
    cJSON_AddNumberToObject(root, "volr",volr & 0xff );
    cJSON_AddStringToObject(root, "url", params->station);

    char* out = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "out:%s", out);
    sprintf(chunk_len,"%x\r\n",strlen(out));
    write(client_fd, chunk_len, strlen(chunk_len));
    write(client_fd, out, strlen(out));
        free(out);
    write(client_fd,"\r\n",2);
    chunk_end(client_fd);
    //send(client,out,strlen(out),MSG_WAITALL);
    //printf("handle_return: %s\n", out);
    cJSON_Delete(root);
}
void load_esp32(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"image/png");//html
  	write(client_fd, request, strlen(request));
  	free(request);
  	return_file("/sdcard/www/static/esp32.png");
}
void web_index(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"text/html");//html
  	write(client_fd, request, strlen(request));
  	free(request);
  	return_file(web_indexPage);
}
void rest_readdir(http_parser* a,char*url,char* body){
    ESP_LOGI(TAG,"rest_readdir called %s %s", url, body);
    char *request;
    asprintf(&request,RES_HEAD,"application/json");//json
    write(client_fd, request, strlen(request));
    free(request);
    cJSON *root = cJSON_CreateArray();
    cJSON *prev=NULL;
    cJSON *item=NULL;
    int i=0;
    struct dirent *pDirEntry = NULL; 
    DIR          *pDir      = NULL;
    pDir=opendir("/sdcard/");
    if(pDir==NULL){
        ESP_LOGE(TAG,"Opendir Failed");
        //xEventGroupWaitBits(eth_event_group,ETH_DISCONNECTED_BIT,pdTRUE,pdTRUE,portMAX_DELAY);
    }else{
        do{
            item=cJSON_CreateObject();
            pDirEntry = readdir(pDir);
            if(pDirEntry==NULL)
                break;
            //printf("node:%d\ttype:%d\tfilename:%s\n",pDirEntry->d_ino,,pDirEntry->d_name);
            cJSON_AddStringToObject(item,"filename",pDirEntry->d_name);
            cJSON_AddNumberToObject(item,"type",pDirEntry->d_type);
            if (i==0){
                root->child = item;
            }else{
                prev->next = item;
                item->prev = prev;
            }
            prev =item;
            i++;

        }while(1);
        closedir(pDir);
    }
    char* out = cJSON_PrintUnformatted(root);
    sprintf(chunk_len,"%x\r\n",strlen(out));
    write(client_fd, chunk_len, strlen(chunk_len));
    write(client_fd, out, strlen(out));
    write(client_fd,"\r\n",2);
    chunk_end(client_fd);
    //send(client,out,strlen(out),MSG_WAITALL);
    printf("handle_return: %s\n", out);
    cJSON_Delete(root);
}
void rest_readwav(http_parser* a,char*url,char* body){
    ESP_LOGI(TAG,"rest_readwav called %s %s", url, body);
    char *request;
    asprintf(&request,RES_HEAD,"audio/x-wav");//json
    write(client_fd, request, strlen(request));
    free(request);
    cJSON *root=NULL;
    root= cJSON_Parse(http_body);
    char* name=cJSON_GetObjectItem(root,"filename")->valuestring;
    return_file(name);
    cJSON_Delete(root);
}
void load_stations(http_parser* a,char*url,char* body)
{
    ESP_LOGI(TAG,"load_stations called %s %s", url, body);
    char *request;
    asprintf(&request,RES_HEAD,"text/json");//json
    write(client_fd, request, strlen(request));
    free(request);

    return_file("/sdcard/www/stations.json");
    }

void led_ctrl(http_parser* a,char*url,char* body){
	ESP_LOGI(TAG,"led_ctrl called %s %s", url, body);
	char *request;
  	asprintf(&request,RES_HEAD,"application/json");//json
  	write(client_fd, request, strlen(request));
  	free(request);
  	cJSON *root=NULL;
	root= cJSON_Parse(http_body);
	uint8_t led=cJSON_GetObjectItem(root,"led")->valueint;
	cJSON_Delete(root);
	gpio_set_level(GPIO_OUTPUT_IO_0,!led);
  	root=NULL;
	root=cJSON_CreateObject();
	if(root==NULL){
		ESP_LOGI(TAG,"cjson root create failed\n");
		return NULL;
	}
	cJSON_AddNumberToObject(root,"err",0);
	// cJSON_AddStringToObject(root,"cuid","esp32_whyengineer");
	// cJSON_AddStringToObject(root,"token",access_token);
	// cJSON_AddNumberToObject(root, "rate", 8000);
	// cJSON_AddNumberToObject(root, "channel", 1);
	// cJSON_AddNumberToObject(root, "len", len);
	// cJSON_AddStringToObject(root,"speech",speech);
	char* out = cJSON_PrintUnformatted(root);
	sprintf(chunk_len,"%x\r\n",strlen(out));
	write(client_fd, chunk_len, strlen(chunk_len));
	write(client_fd, out, strlen(out));
        free(out);
  	write(client_fd,"\r\n",2);
  	chunk_end(client_fd);
	//send(client,out,strlen(out),MSG_WAITALL);
	//printf("handle_return: %s\n", out);
	cJSON_Delete(root);
}

static int body_done_callback (http_parser* a){
	http_body=realloc(http_body,http_body_length+1);
    http_body[http_body_length]='\0';
  	ESP_LOGI(TAG,"url=%s,body:%s",http_url,http_body);
    for(int i=0;i<sizeof(http_handle)/sizeof(http_handle[0]);i++)
    {
        if(strcmp(http_handle[i].url,http_url)==0)
        {
            http_handle[i].handle(a,http_url,http_body);
            free(http_url);
            free(http_body);
            http_url=NULL;
            http_body=NULL;
            http_url_length=0;
            http_body_length=0;
            return 0;
        }
  	}
    not_found();
        free(http_url);
	free(http_body);
	http_url=NULL;
	http_body=NULL;
	http_url_length=0;
	http_body_length=0;
// char *request;
  	// asprintf(&request,RES_HEAD,HTML);
  	// write(client_fd, request, strlen(request));
   //  free(request);
   //  sprintf(chunk_len,"%x\r\n",strlen(not_find_page));
   //  //ESP_LOGI(TAG,"chunk_len:%s,length:%d",chunk_len,strlen(http_index_hml));
   //  write(client_fd, chunk_len, strlen(chunk_len));
   //  write(client_fd, http_index_hml, strlen(http_index_hml));
   //  write(client_fd, "\r\n", 2);
   //  chunk_end(client_fd);
    //close( client_fd );	
    return 0;
}
static int begin_callback (http_parser* a){
  	//ESP_LOGI(TAG,"message begin");
    return 0;
}
static int header_complete_callback(http_parser* a){
	http_url=realloc(http_url,http_url_length+1);
    http_url[http_url_length]='\0';
	ESP_LOGI(TAG,"url:%s",http_url);
    return 0;
}

static http_parser_settings settings =
{   .on_message_begin = begin_callback
    ,.on_header_field = 0
    ,.on_header_value = header_value_callback
    ,.on_url = url_callback
    ,.on_status = 0
    ,.on_body = body_callback
    ,.on_headers_complete = header_complete_callback
    ,.on_message_complete = body_done_callback
    ,.on_chunk_header = 0
    ,.on_chunk_complete = 0
};

/* Dimensions the buffer into which input characters are placed. */

static struct sockaddr_in server, client;


int create_socket_server(in_port_t in_port, in_addr_t in_addr)
{
	int socket_fd, on;
	//struct timeval timeout = {10,0};

	server.sin_family = AF_INET;
	server.sin_port = in_port;
	server.sin_addr.s_addr = in_addr;

	if((socket_fd = socket(AF_INET, SOCK_STREAM, 0))<0) {
		perror("listen socket uninit\n");
		return -1;
	}
	on=1;
	//setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int) );
	//CALIB_DEBUG("on %x\n", on);
	if((bind(socket_fd, (struct sockaddr *)&server, sizeof(server)))<0) {
		perror("cannot bind srv socket\n");
		return -1;
	}

	if(listen(socket_fd, 1)<0) {
		perror("cannot listen");
		close(socket_fd);
		return -1;
	}

	return socket_fd;
}


void webserver_task( void *pvParameters ){
	int32_t lBytes;
	esp_err_t err;
	ESP_LOGI(TAG,"webserver start");
	uint32_t request_cnt=0;

	//(void) pvParameters;
  if(pvParameters != NULL)
  {
   //sprintf(web_indexPage,"/sdcard/www/%s", "3d_show.html");
   //web_indexPage = pvParameters;
   params = (struct webserver_params*)pvParameters;
   if(params->html == NULL)
   {
       ESP_LOGE(TAG, "http is NYLL!");
       vTaskDelete(NULL);
       return;
   }
   ESP_LOGI(TAG, "web page:%s station:%s", params->html, params->station);
   params = (struct webserver_params*)pvParameters;
   web_indexPage = params->html;
  }
	http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);
    parser.data = NULL;
    socklen_t client_size=sizeof(client);

	socket_fd = create_socket_server(htons(80),htonl(INADDR_ANY));
	if( socket_fd >= 0 ){
		/* Obtain the address of the output buffer.  Note there is no mutual
		exclusion on this buffer as it is assumed only one command console
		interface will be used at any one time. */
		char recv_buf[64];
		esp_err_t nparsed = 0;
		/* Ensure the input string starts clear. */
		for(;;){
			client_fd=accept(socket_fd,(struct sockaddr*)&client,&client_size);
			if(client_fd>0L){
				// strcat(outbuf,pcWelcomeMessage);
				// strcat(outbuf,path);
				// lwip_send( lClientFd, outbuf, strlen(outbuf), 0 );
				do{
					lBytes = recv( client_fd, recv_buf, sizeof( recv_buf ), 0 );
					if(lBytes==0){
						close( client_fd );	
						break;
					}
						
					 // for(int i=0;i<lBytes;i++)
						// putchar(recv_buf[i]);
					nparsed = http_parser_execute(&parser, &settings, recv_buf, lBytes);
					
					//while(xReturned!=pdFALSE);
					//lwip_send( lClientFd,path,strlen(path), 0 );
				}while(lBytes > 0 && nparsed >= 0);
				free(http_url);
				free(http_body);
				http_url=NULL;
				http_body=NULL;
				http_url_length=0;
				http_body_length=0;		
			}
			ESP_LOGI(TAG,"request_cnt:%d,socket:%d",request_cnt++,client_fd);
			close( client_fd );
		}
	}

}
