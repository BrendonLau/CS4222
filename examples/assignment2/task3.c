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
#define INTERIM_STATE 4
#define LIGHT_THRESHOLD 30000
#define GYRO_THRESHOLD 15000
#define MOTION_THRESHOLD 15000

// Functions
static void init_mpu_reading(void);
static void init_opt_reading(void);
static bool is_movement();
static void get_light_reading(void);
static bool is_light_different();
static void print_statistics();

// Variables
static int state = IDLE_STATE;
static struct etimer buzzer_timer;
static struct etimer wait_timer;
static struct etimer delay_timer;
static struct etimer total_active_timer;

// Cur states
static int gX, gY, gZ;
static int accX, accY, accZ;
static int curr_light;

// Prev state of gyros
static int prev_gX, prev_gY, prev_gZ;

// Prev state of the light
static int previous_light;
static bool is_first_run = true;
static bool is_state_running = false;

// Initialise device readings
static void init_mpu_reading(void)
{
  mpu_9250_sensor.configure(SENSORS_ACTIVE, MPU_9250_SENSOR_TYPE_ALL);
}

static void init_opt_reading(void)
{
  SENSORS_ACTIVATE(opt_3001_sensor);
}

static bool is_movement()
{
  gX = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_X);
  gY = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Y);
  gZ = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Z);
  accX = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_X);
  accY = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Y);
  accZ = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Z);

  int accel_magn_squared = accX * accX + accY * accY + accZ * accZ;
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

static void get_light_reading()
{
  int value = opt_3001_sensor.value(0);
  if (value != CC26XX_SENSOR_READING_ERROR)
  {
    curr_light = value;
  }
  init_opt_reading();
}

static bool is_light_different()
{
  get_light_reading();

  bool isAboveThreshold = abs(curr_light - previous_light) > LIGHT_THRESHOLD;
  previous_light = curr_light;

  return isAboveThreshold;
}

static void print_statistics()
{
  printf("prev light:%d, curr light: %d\n", previous_light, curr_light);
  printf("prev gX gY gZ: %d %d %d, cur gX gY gZ: %d %d %d\n", prev_gX, prev_gY, prev_gZ, gX, gY, gZ);
  printf("curr accX accY accZ: %d %d %d\n", accX, accY, accZ);
}

PROCESS_THREAD(state_change, ev, data)
{
  PROCESS_BEGIN();

  // Initialize buzzer and sensors
  buzzer_init();
  init_opt_reading();
  init_mpu_reading();
  printf("Program start\n\n");

  // Let timer warm up to get reading
  etimer_set(&delay_timer, CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&delay_timer));

  // Initialise variables
  get_light_reading();
  previous_light = curr_light;
  prev_gX = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_X);
  prev_gY = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Y);
  prev_gZ = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Z);

  print_statistics();

  while (1)
  {
    etimer_set(&delay_timer, CLOCK_SECOND / 4);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&delay_timer));

    bool is_light_change_detected = is_light_different();
    bool is_motion_detected = is_movement();

    if (state == IDLE_STATE && is_motion_detected)
    {
      state = INTERIM_STATE;
      printf("detected movement\n");
      printf("IDLE -> INTERIM\n");
    }
    else if (state == INTERIM_STATE && is_light_change_detected)
    {
      state = BUZZ_STATE;
      is_first_run = true;
      printf("detected light change\n");
      printf("INTERIM -> BUZZ\n");
    }
    else if (state == BUZZ_STATE)
    {
      if (!is_state_running)
      {
        buzzer_start(2069);
        etimer_set(&buzzer_timer, CLOCK_SECOND * 2);
        is_state_running = true;
      }

      if (!is_first_run && is_light_change_detected)
      {
        if (buzzer_state())
        {
          buzzer_stop();
        }
        is_state_running = false;
        state = IDLE_STATE;
        printf("detected light\n");
        printf("BUZZ -> IDLE\n");
      }
      else if (etimer_expired(&buzzer_timer))
      {
        if (buzzer_state())
        {
          buzzer_stop();
        }
        is_state_running = false;
        state = WAIT_STATE;
        printf("BUZZ -> WAIT\n");
      }
    }
    else if (state == WAIT_STATE)
    {
      if (!is_state_running)
      {
        etimer_set(&wait_timer, CLOCK_SECOND * 4);
        is_state_running = true;
      }

      if (!is_first_run && is_light_change_detected)
      {
        state = IDLE_STATE;
        is_state_running = false;
        printf("detected light\n");
        printf("WAIT -> IDLE\n");
      }
      else if (etimer_expired(&wait_timer))
      {
        state = BUZZ_STATE;
        is_state_running = false;
        is_first_run = false;
        printf("WAIT -> BUZZ\n");
      }
    }
    PROCESS_PAUSE();
  }
  PROCESS_END();
}
