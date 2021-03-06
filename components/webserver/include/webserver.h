#ifndef WEB_SERVER_H
#define WEB_SERVER_H
#include "http_parser.h"
#include "freertos/task.h"
#include "stream.h"
#include "freertos/event_groups.h"
#include "driver/sdmmc_types.h"

void webserver_task( void *pvParameters );


struct webserver_params
{
 char * html;
 char * station;
 http_parser_settings* settings;
 TaskHandle_t decodeTaskHandle;
 TaskHandle_t webRadioTaskHandle;
 EventGroupHandle_t eventGroup;
 esp_err_t err;
 char * errorText;
 sdmmc_host_t* host;
};


#endif
