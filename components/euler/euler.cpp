#if !CONFIG_MPU9250
#include "mpu6050.h"
#endif
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
#include "cJSON.h"
#include <dirent.h>
#include "euler.h"
#include <math.h>
#include "imuUpdate.h"
#include "websocket.h"

#if CONFIG_MPU9250
#include "MPU.hpp"        // main file, provides the class itself
#include "mpu/math.hpp"   // math helper for dealing with MPU data
#include "mpu/types.hpp"  // MPU data types and definitions
#if defined CONFIG_MPU_I2C
#include "I2Cbus.hpp"
static I2C_t& i2c                     = i2c0;  // i2c0 or i2c1
static constexpr gpio_num_t SDA       = GPIO_NUM_19;
static constexpr gpio_num_t SCL       = GPIO_NUM_18;
static constexpr uint32_t CLOCK_SPEED = 400000;  // 400 KHz
#elif defined CONFIG_MPU_SPI
#include "SPIbus.hpp"
static SPI_t& spi                     = hspi;  // hspi or vspi
static constexpr int MOSI             = 22;
static constexpr int MISO             = 21;
static constexpr int SCLK             = 23;
static constexpr int CS               = 16;
static constexpr uint32_t CLOCK_SPEED = 1000000;  // 1MHz
#endif
static MPU_t MPU;
#endif

#define TAG "euler"


EulerTypeDef euler_data;
static int16_t gyro_offset[3];


static void gyro_calib();
static void sensor_data_update();
static float CLMAP(float a,float min,float max);
static void  euler2quat();
static void quat2euler();
static void quaternion_init();
static void euler_data_update();


static void update_task(void *pvParameters){
#if CONFIG_MPU9250
        
        MPU.initialize();  // this will initialize the chip and set default configurations
#else
	MPU6050_Initialize();
#endif
	gyro_calib();
	quaternion_init();
	while(1){
		euler_data_update();
		vTaskDelay(2);//500hz
	}
}

extern "C" void euler_task( void *pvParameters ){
	//memset()
#if CONFIG_MPU9250
	MPU.setBus(i2c0);  // set communication bus, for SPI -> pass 'hspi'
        MPU.setAddr(mpud::MPU_I2CADDRESS_AD0_LOW);  // set address or handle, for SPI -> pass 'mpu_spi_handle'
	MPU.setInterruptEnabled(mpud::INT_EN_RAWDATA_READY);  // enable INT pin
	if(MPU.testConnection()!=ESP_OK){
		ESP_LOGI(TAG,"MPU9250 connect failed");
	}else{
		ESP_LOGI(TAG,"mpu9250 connect ok!!!");
	}
#else
	if(MPU6050_TestConnection()!=ESP_OK){
		ESP_LOGI(TAG,"MPU6050 connect failed");
	}else{
		ESP_LOGI(TAG,"mpu6050 connect ok!!!");
	}
#endif
    xTaskCreate(&update_task, "euler_update_task", 4096, NULL, 10, NULL);

    cJSON *root=NULL;
    char* out=NULL;
    while(1){
		//sensor_data_update();
    root=cJSON_CreateObject();
    cJSON_AddNumberToObject(root,"roll",(int)(euler_data.euler[0]*57320));
    cJSON_AddNumberToObject(root,"pitch",(int)(euler_data.euler[1]*57320));
    cJSON_AddNumberToObject(root,"yaw",(int)(euler_data.euler[2]*57320));
    out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    err_t rslt = WS_write_data(out,strlen(out));
    if(rslt != ERR_OK)
     ESP_LOGI(TAG,"WS_write_data returned %d", rslt);
    free(out);
		ESP_LOGI(TAG,"\nroll:%f,pitch:%f,yaw:%f",euler_data.euler[0]*57.32,euler_data.euler[1]*57.32,euler_data.euler[2]*57.32);
		vTaskDelay(100);
	}
	//vTaskSuspend(NULL);
}



static void euler_data_update(){
	sensor_data_update();
	IMUupdate();
	quat2euler();
}
static void quat2euler(){
	float q0,q1,q2,q3;
  	float pitch,roll,yaw;
  	q0=euler_data.q[0];
  	q1=euler_data.q[1];
  	q2=euler_data.q[2];
  	q3=euler_data.q[3];
  	//roll=asin(CLMAP(2 * (q2 * q3 + q0 * q1) , -1.0f , 1.0f));
  	//pitch=-atan2(2 * (q1 * q3 - q0* q2) , 1- 2 * (q2 * q2+ q1 * q1));
  	//yaw=atan2(my*arm_cos_f32(roll)+mx*arm_sin_f32(roll)*arm_sin_f32(pitch)-mz*arm_sin_f32(roll)*arm_cos_f32(pitch),mx*arm_cos_f32(pitch)-mz*arm_sin_f32(pitch))*57.3;
  	//yaw = -(0.9 * (-yaw + gz*0.002*57.3) + 5.73* atan2(mx*cos(roll) + my*sin(roll)*sin(pitch) + mz*sin(roll)*cos(pitch), my*cos(pitch) - mz*sin(pitch)));
  	//yaw=atan2(2 * (q0 * q2 + q3 * q1) , 1 - 2 * (q1 * q1 + q2 * q2))*57.3; 
  	//yaw=atan2(mx,my)*57.3;
  
  	//Data.euler[0] = roll; 
  	//Data.euler[1]  = pitch;
  	//Data.euler[2] =0.9*(yaw1+gz*57.3*0.002)+0.1*yaw;
  	//Data.euler[2] = atan2(2 * (q0 * q2 + q3 * q1) , 1 - 2 * (q1 * q1 + q2 * q2)); 
  	//Data.euler[2]=atan2(2*(q1*q2 + q0*q3),q0*q0+q1*q1-q2*q2-q3*q3);
  	//Data.euler[2] =0.9*(yaw-gz*57.3*0.002)+0.1*yaw;
  	//Data.euler[2]=yaw;
  	roll=atan2(2*(q0*q1+q2*q3),1-2*(q1*q1+q2*q2));
  	pitch=asin(CLMAP(2*(q0*q2-q1*q3),-1.0f,1.0f));
  	yaw=atan2(2*(q0*q3+q1*q2),1-2*(q3*q3+q2*q2));
  
  	euler_data.euler[0]=roll; 
  	euler_data.euler[1]=pitch;
  	euler_data.euler[2]=yaw; 
}
static void euler2quat(){
	float recipNorm;
    float fCosHRoll = cos(euler_data.euler[0] * .5f);
    float fSinHRoll = sin(euler_data.euler[0] * .5f);
    float fCosHPitch = cos(euler_data.euler[1] * .5f);
    float fSinHPitch = sin(euler_data.euler[1] * .5f);
    float fCosHYaw = cos(euler_data.euler[2] * .5f);
    float fSinHYaw = sin(euler_data.euler[2]* .5f);
    float q0,q1,q2,q3;

    /// Cartesian coordinate System
    q0 = fCosHRoll * fCosHPitch * fCosHYaw + fSinHRoll * fSinHPitch * fSinHYaw;
    q1 = fSinHRoll * fCosHPitch * fCosHYaw - fCosHRoll * fSinHPitch * fSinHYaw;
    q2 = fCosHRoll * fSinHPitch * fCosHYaw + fSinHRoll * fCosHPitch * fSinHYaw;
    q3 = fCosHRoll * fCosHPitch * fSinHYaw - fSinHRoll * fSinHPitch * fCosHYaw;

    //  q0 = fCosHRoll * fCosHPitch * fCosHYaw - fSinHRoll * fSinHPitch * fSinHYaw;
    //  q1 = fCosHRoll * fSinHPitch * fCosHYaw - fSinHRoll * fCosHPitch * fSinHYaw;
    //  q2 = fSinHRoll * fCosHPitch * fCosHYaw + fCosHRoll * fSinHPitch * fSinHYaw;
    //  q3 = fCosHRoll * fCosHPitch * fSinHYaw + fSinHRoll * fSinHPitch * fCosHYaw;
        
    recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;
  
    euler_data.q[0]=q0;
    euler_data.q[1]=q1;
    euler_data.q[2]=q2;
    euler_data.q[3]=q3;      
}
static void quaternion_init(){
	float roll,pitch;
	sensor_data_update();
	roll=atan2(euler_data.accel[1],euler_data.accel[2]);
    pitch=-atan2(euler_data.accel[0],euler_data.accel[2]);
    euler_data.euler[0]=roll;
	euler_data.euler[1]=pitch;
	euler_data.euler[2]=0.0;
	euler2quat();
}
static void sensor_data_update(){
	int16_t buf[6];
#if CONFIG_MPU9250
        MPU.acceleration(&buf[0], &buf[1], &buf[2]);
        MPU.rotation(&buf[3], &buf[4], &buf[5]);
#else
		MPU6050_GetRawAccelGyro(buf);
#endif

	euler_data.accel[0]=buf[0]/32768.0*2.0*9.8;  //m2/s
	euler_data.accel[1]=buf[1]/32768.0*2.0*9.8;
	euler_data.accel[2]=buf[2]/32768.0*2.0*9.8;
	euler_data.gyro[0]=(buf[3]-gyro_offset[0])/32768.0*250/57.32;//radio
	euler_data.gyro[1]=(buf[4]-gyro_offset[1])/32768.0*250/57.32;
	euler_data.gyro[2]=(buf[5]-gyro_offset[2])/32768.0*250/57.32;
}
static void gyro_calib(){
	int gyro_tmp[3]={0};
	int16_t tmp[6];
	for(int i=0;i<30;i++){
#if CONFIG_MPU9250
        MPU.acceleration(&tmp[0], &tmp[1], &tmp[2]);
        MPU.rotation(&tmp[3], &tmp[4], &tmp[5]);
#else
		MPU6050_GetRawAccelGyro(tmp);
#endif
		gyro_tmp[0]+=tmp[3];
		gyro_tmp[1]+=tmp[4];
		gyro_tmp[2]+=tmp[5];
		vTaskDelay(100);
	}
	gyro_offset[0]=gyro_tmp[0]/30;
	gyro_offset[1]=gyro_tmp[1]/30;
	gyro_offset[2]=gyro_tmp[2]/30;
}
float CLMAP(float a,float min,float max)
{
  if(a<min)
    a=min;
  else if(a>max)
    a=max;
  else
    a=a;
  return a;
}
