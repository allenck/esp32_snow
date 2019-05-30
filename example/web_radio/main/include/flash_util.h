#ifndef FLASH_UTIL_H
#define FLASH_UTIL_H
#include "esp_err.h"

esp_err_t init_flash();

esp_err_t read_flash(char* _namespace, char * key, char **pValue);

esp_err_t write_flash(char *_namespace, char * key, char* pValue);
#endif // FLASH_UTIL_H
