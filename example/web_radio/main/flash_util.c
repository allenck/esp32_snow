/* flash_util.c */

#include "flash_util.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "string.h"
#include "esp_log.h"

#define STORAGE_NAMESPACE "storage"

esp_err_t init_flash()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    return err;
}

esp_err_t read_flash(char *_namespace, char* key, char** pValue)
{
    ESP_LOGI("read_flash", "namespace:%s key:%s ", _namespace, key);

    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(_namespace, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    // obtain required memory space to store blob being read from NVS
    err = nvs_get_blob(my_handle, key, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    printf("%s:\n", key);
    if (required_size == 0) {
       printf("Nothing saved yet!\n");
    } else {
       char* value = malloc(required_size);
       err = nvs_get_blob(my_handle, key, value, &required_size);
       if (err != ESP_OK) {
           free(value);
           return err;
       }
    //       for (int i = 0; i < required_size / sizeof(uint32_t); i++) {
    //           printf("%d: %d\n", i + 1, run_time[i]);
    //       }
       //free(pValue);
       *pValue = value;
    }
    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t write_flash(char *_namespace, char * key, char* pValue)
{
    ESP_LOGI("write_flash", "namespace:%s key:%s value:%s", _namespace,key,pValue);

    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(_namespace, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, key, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    //ESP_LOGI("write_flash", "required size %d", required_size);

    // Read previously saved blob if available
    char* value = malloc(required_size);
    if (required_size > 0) {
        err = nvs_get_blob(my_handle, key, value, &required_size);
        if (err != ESP_OK) {
            free(value);
            return err;
        }
    }

    // Write value including previously saved blob if available
    required_size = strlen(pValue)+1;
//    run_time[required_size / sizeof(uint32_t) - 1] = xTaskGetTickCount() * portTICK_PERIOD_MS;
    char* saveValue = malloc(strlen(pValue)+1);
    strcpy(saveValue, pValue);
    err = nvs_set_blob(my_handle, key, saveValue, required_size);
    //ESP_LOGI("write_flash", "write required size %d, key:'%s' value:'%s' return %d", required_size, key, saveValue, err);
    free(saveValue);

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}
