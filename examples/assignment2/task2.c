#include "contiki.h"
#include "board-peripherals.h"
#include "sys/etimer.h"
#include "sys/rtimer.h"
#include "buzzer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

PROCESS(state_change, "State change");
AUTOSTART_PROCESSES(&state_change);

// Constants
#define BUZZ_STATE 1
#define WAIT_STATE 2
#define IDLE_STATE 3
#define LIGHT_THRESHOLD 1
#define GYRO_THRESHOLD 10000 // need help identifying actual threshold
#define MOTION_THRESHOLD 10000 // need help identifying actual threshold

// Functions
static void init_mpu_reading(void);
static void init_opt_reading(void);
static bool is_movement();
static int get_light_reading(void);
static bool is_light_different();
static void print_statistics();

// Variables
static int state = IDLE_STATE;
static struct etimer buzzer_timer;
static struct etimer wait_timer;
static struct etimer delay_timer;
static struct etimer total_active_timer;

// Cur states
int gX, gY, gZ;
int accX, accY, accZ;

// Prev state of gyros
static int prev_gX, prev_gY, prev_gZ;
// Prev state of the light
static int previous_light;

// Initialise device readings
static void init_mpu_reading(void) {
  mpu_9250_sensor.configure(SENSORS_ACTIVE, MPU_9250_SENSOR_TYPE_ALL);
}

static void init_opt_reading(void) {
  SENSORS_ACTIVATE(opt_3001_sensor);
}

static bool is_movement() {
  gX = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_X);
  gY = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Y);
  gZ = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Z);
  accX = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_X);
  accY = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Y);
  accZ = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Z);

  int accel_magn_squared = accX * accX + accY * accY + accZ*accZ;
  int gyroXDiff = abs(gX - prev_gX);
  int gyroYDiff = abs(gY - prev_gY);
  int gyroZDiff = abs(gZ - prev_gZ);
  
  bool is_accel_diff = accel_magn_squared > (MOTION_THRESHOLD * MOTION_THRESHOLD);
  bool is_gyro_diff = gyroXDiff > GYRO_THRESHOLD || gyroYDiff > GYRO_THRESHOLD || gyroZDiff > GYRO_THRESHOLD;

  prev_gX = gX;
  prev_gY = gY;
  prev_gZ = gZ;

  return is_accel_diff || is_gyro_diff;
}

static int get_light_reading() {
  int value = opt_3001_sensor.value(0);
  if(value != CC26XX_SENSOR_READING_ERROR) {
    //printf("OPT: Light=%d.%02d lux\n", value / 100, value % 100);
    return value;
  } else {
    printf("OPT: Light Sensor's Warming Up\n\n");
  }
  init_opt_reading();
  return -1;
}

static bool is_light_diff() {
  int value = get_light_reading();
  bool isAboveThreshold = abs(value - previous_light) > LIGHT_THRESHOLD;
  if (isAboveThreshold) {
    previous_light = value;
  }
  return isAboveThreshold;
}

static void print_statistics() {
    printf("prev light:%d, curr light: %d\n", previous_light, get_light_reading());
    printf("prev gX gY gZ: %d %d %d, cur gX gY gZ: %d %d %d\n", prev_gX, prev_gY, prev_gZ, gX, gY, gZ);
    printf("curr accX accY accZ: %d %d %d\n", accX, accY, accZ);
}


PROCESS_THREAD(state_change, ev, data) {
  PROCESS_BEGIN();
  //Initialize buzzer and sensors
  buzzer_init();
  init_opt_reading();
  init_mpu_reading();
  printf("Program start\n\n");

  //Let timer warm up to get reading
  etimer_set(&delay_timer, CLOCK_SECOND*3);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&delay_timer));

  // initialise variables
  previous_light = get_light_reading();
  prev_gX = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_X);
  prev_gY = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Y);
  prev_gZ = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Z);

  while(1) {
    // print_statistics();

    // if significant motion or light change is detected, go into buzz state
    if((is_movement() || is_light_diff()) && state == IDLE_STATE) {
      printf("detected movement\n");
      printf("IDLE -> BUZZ\n");
      // print_statistics();

      // Initialise active timer
      // total active state lasts for 16s
      etimer_set(&total_active_timer, CLOCK_SECOND*16); 

      state = BUZZ_STATE;
      buzzer_start(2069);
      etimer_set(&buzzer_timer, CLOCK_SECOND*2); // buzz for 2 seconds
    }

    if(state == BUZZ_STATE && etimer_expired(&buzzer_timer) && !etimer_expired(&total_active_timer)) {
      buzzer_stop();
      state = WAIT_STATE;

      printf("BUZZ -> WAIT\n");
      // print_statistics();
      etimer_set(&wait_timer, CLOCK_SECOND*2); // wait for 2 seconds
    }

    if(state == WAIT_STATE && etimer_expired(&wait_timer) && !etimer_expired(&total_active_timer)) {
      printf("WAIT -> BUZZ\n");
      // print_statistics();

      state = BUZZ_STATE;
      buzzer_start(2069);

      etimer_set(&buzzer_timer, CLOCK_SECOND*2); // set buzz for 2 seconds
    }

    if (etimer_expired(&total_active_timer)) {
        state = IDLE_STATE;
        printf("WAIT/BUZZ -> IDLE\n");
        if (buzzer_state()) {
          buzzer_stop();
        }
    }

    PROCESS_PAUSE();
  }
  PROCESS_END();
}


