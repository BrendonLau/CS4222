#include "board-peripherals.h"
#include "contiki.h"
#include "lib/random.h"
#include "net/linkaddr.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"
#include "node-id.h"
#include "sys/etimer.h"
#include "sys/rtimer.h"
#include "task_2_group_8_typedef.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

linkaddr_t dest_addr;

static struct etimer interval_timer;

static struct rtimer rt;
static struct pt pt;
static data_packet_struct data_packet;
static data_light_packet_struct data_light_packet;
unsigned long curr_timestamp;
static bool has_detected = false;
static bool has_good_link = false;

static int light_readings[LIGHT_READING_LEN] = {0};
static int light_index = 0;

PROCESS(light_reading_process, "light reading process");
PROCESS(nbr_discovery_process, "neighbour discovery process");
AUTOSTART_PROCESSES(&light_reading_process, &nbr_discovery_process);

static void init_opt_reading(void);
static void get_light_reading(void);

/**** START OF LIGHT READING FROM ASSIGNMENT 2 ***/
static void get_light_reading() {
  int value;
  value = opt_3001_sensor.value(0);

  if (value != CC26XX_SENSOR_READING_ERROR) {
    // printf("light sense[%d]: %d\n", light_index, value);
    light_readings[light_index] = value;
    light_index++;
  }
  init_opt_reading();
}

static void init_opt_reading(void) { SENSORS_ACTIVATE(opt_3001_sensor); }

PROCESS_THREAD(light_reading_process, ev, data) {
  PROCESS_BEGIN();

  // Set the reading to default value, receiver will ignore the default value
  for (int i = 0; i < LIGHT_READING_LEN; i++) {
    light_readings[i] = LIGHT_DEFAULT;
  }
  etimer_set(&interval_timer, CLOCK_SECOND);

  while (1) {
    // Skip updating light reading if the buffer is full, immediately read the
    // light sensor after the buffer is flushed -- prevent unnecessary reading
    if (light_index >= LIGHT_READING_LEN) {
      // printf("skip updating light reading\n");
      continue;
    }
    get_light_reading();
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&interval_timer));
    etimer_reset(&interval_timer);
  }

  PROCESS_END();
}

/**** END OF LIGHT READING FROM ASSIGNMENT 2 ****/

/**** START OF NEIGHBOUR DISCOVERY FROM TASK 1 ****/
void receive_packet_callback(const void *data, uint16_t len,
                             const linkaddr_t *src, const linkaddr_t *dest) {
  if (len != sizeof(data_packet)) {
    return;
  }

  static data_packet_struct received_packet_data;
  memcpy(&received_packet_data, data, len);

  if (received_packet_data.device_type == data_packet.device_type) {
    return;
  }

  if (!has_detected) {
    printf("%lu DETECT %lu\n", RTIMER_NOW() / RTIMER_SECOND,
           received_packet_data.src_id);
    has_detected = true;
  }
  int rssi = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  printf("RSSI: %d\n", rssi);

  // If link quality is weak
  if (rssi < RSSI_THRESHOLD) {
    has_good_link = false;
    return;
  }

  if (!has_good_link) {
    has_good_link = true;
  }

  data_light_packet.timestamp = curr_timestamp;
  memcpy(&data_light_packet.light_readings, light_readings,
         sizeof(light_readings));
  data_light_packet.seq++;
  nullnet_buf = (uint8_t *)&data_light_packet;
  nullnet_len = sizeof(data_light_packet);
  NETSTACK_NETWORK.output(&dest_addr);

  printf("%ld TRANSFER %ld\n", RTIMER_NOW(), received_packet_data.src_id);
  // for(int i = 0; i < LIGHT_READING_LEN; i++) {
  //   printf("sending light reading %d\n",
  //   data_light_packet.light_readings[i]);
  // }

  // reset the light readings
  for (int i = 0; i < LIGHT_READING_LEN; i++) {
    light_readings[i] = LIGHT_DEFAULT;
  }
  light_index = 0;

  return;
}

char sender_scheduler(struct rtimer *t, void *ptr) {
  static uint16_t i = 0;
  static int NumSleep = 0;

  // Begin the protothread
  PT_BEGIN(&pt);

  // Get the current time stamp
  curr_timestamp = clock_time();
  printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp,
         curr_timestamp / CLOCK_SECOND,
         ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

  while (1) {

    // radio on
    NETSTACK_RADIO.on();

    // send NUM_SEND number of neighbour discovery beacon packets
    for (i = 0; i < NUM_SEND; i++) {

      // Initialize the nullnet module with information of packet to be
      // trasnmitted
      nullnet_buf = (uint8_t *)&data_packet; // data transmitted
      nullnet_len = sizeof(data_packet);     // length of data transmitted

      data_packet.seq++;

      curr_timestamp = clock_time();

      data_packet.timestamp = curr_timestamp;

      NETSTACK_NETWORK.output(&dest_addr); // Send packet

      if (i != (NUM_SEND - 1)) {
        rtimer_set(t, RTIMER_TIME(t) + WAKE_TIME, 1,
                   (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }
    }

    // sleep for a random number of slots
    if (SLEEP_CYCLE != 0) {

      // radio off
      NETSTACK_RADIO.off();

      NumSleep = SENDER_PRIME - 1;

      // NumSleep should be a constant or static int
      for (i = 0; i < NumSleep; i++) {
        rtimer_set(t, RTIMER_TIME(t) + SLEEP_SLOT, 1,
                   (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }
    }
  }

  PT_END(&pt);
}

PROCESS_THREAD(nbr_discovery_process, ev, data) {
  PROCESS_BEGIN();
  data_light_packet.src_id = node_id;
  data_light_packet.seq = 0;

  data_packet.src_id = node_id;
  data_packet.device_type = SENDER_TYPE;
  data_packet.seq = 0;
  nullnet_set_input_callback(receive_packet_callback);
  linkaddr_copy(&dest_addr, &linkaddr_null);

  printf("CC2650 neighbour discovery\n");
  printf("Node %d will be sending packet of size %d Bytes\n", node_id,
         (int)sizeof(data_packet_struct));

  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1,
             (rtimer_callback_t)sender_scheduler, NULL);

  PROCESS_END();
}

/**** END OF NEIGHBOUR DISCOVERY FROM TASK 1 *****/
