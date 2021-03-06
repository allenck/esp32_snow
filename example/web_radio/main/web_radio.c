/* web_radio.c  */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "http.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "hal_i2s.h"
#include "wm8978.h"
#include "http_parser.h"
#include "url_parser.h"
#include "spiram_fifo.h"
#include "mp3_decode.h"
#include "webserver.h"

#define TAG "web radio"

static int header_value_callback(http_parser* a, const char *at, size_t length){
	for(int i=0;i<length;i++)
		putchar(at[i]);
    printf("\r\n");
	return 0;
}
static int body_callback(http_parser* a, const char *at, size_t length){
    // static uint32_t cnt=0;
    // printf("cnt:%d\n",cnt++);
    spiRamFifoWrite(at,length);
    return 0;
}
static int body_done_callback (http_parser* a){
  	ESP_LOGI(TAG,"body done");
    return 0;
}
static int begin_callback (http_parser* a){
  	ESP_LOGI(TAG,"message begin");
    return 0;
}
static int header_complete_callback(http_parser* a){
    putchar('\n');
	ESP_LOGI(TAG,"header completed");
    return 0;
}

static http_parser_settings settings =
{   .on_message_begin = begin_callback
    ,.on_header_field = 0
    ,.on_header_value = header_value_callback
    ,.on_url = 0
    ,.on_status = 0
    ,.on_body = body_callback
    ,.on_headers_complete = header_complete_callback
    ,.on_message_complete = body_done_callback
    ,.on_chunk_header = 0
    ,.on_chunk_complete = 0
};


void web_radio_task(void* pvParameters){
    struct webserver_params* params = NULL;
    if(pvParameters != NULL)
    {
     params = (struct webserver_params*)pvParameters;
     params->settings = &settings;
     ESP_LOGI(TAG, "web page:%s station:%s", params->html, params->station);
    }
    spiRamFifoInit();
    xTaskCreate(mp3_decode_task, "mp3_decode_task", 8192, pvParameters, 5, &params->decodeTaskHandle);
    //start a http request
    //http_client_get("http://icecast.omroep.nl/3fm-sb-mp3", &settings,NULL);
    if(params != NULL)
    {
        esp_err_t rtn;
        if((rtn =http_client_get(params->station, params->settings, NULL)) != ESP_OK)
        {
            ESP_LOGE(TAG, "http_client_get returned %d", rtn);
            params->err = rtn;
        }
    }
    else
        http_client_get("http://dg-rbb-http-dus-dtag-cdn.cast.addradio.de/rbb/antennebrandenburg/live/mp3/128/stream.mp3", &settings,NULL);
    xEventGroupSetBits(params->eventGroup, BIT2); // signal http_client_get done
    ESP_LOGI(TAG,"get completed!");
    vTaskDelete(NULL);
}
