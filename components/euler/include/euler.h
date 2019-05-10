#ifndef EULER_H
#define EULER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
	float accel[3];
	float gyro[3];
	float euler[3];
	float q[4];
}EulerTypeDef;

void euler_task( void *pvParameters );


#ifdef __cplusplus
}
#endif
#endif
