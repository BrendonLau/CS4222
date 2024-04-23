#include "contiki.h"
#include "sys/rtimer.h"
#include "sys/etimer.h"
#include "board-peripherals.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"
#include "lib/random.h"
#include "net/linkaddr.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "node-id.h"

// From nbr.c
#define WAKE_TIME RTIMER_SECOND/10
#define SLEEP_CYCLE 9
#define SLEEP_SLOT RTIMER_SECOND/10
#define NUM_SEND 2

#define LINK_THRESHOLD -66
#define READING_LENGTH 10
#define LIGHT_DEFAULT -1

#define PROXIMITY_CONTACT_TIME (RTIMER_SECOND * 15)
#define SENDER_TYPE 0
#define DETECT_INTERVAL 15

linkaddr_t dest_addr;

// From nbr.c
typedef struct {
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;
  int device_type;
} data_packet_struct;

typedef struct {
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;
  int light_readings[READING_LENGTH];
} data_light_packet_struct;

static struct etimer interval_timer;

static struct rtimer rt;
static struct pt pt;
static data_packet_struct data_packet;
static data_light_packet_struct data_light_packet;
unsigned long curr_timestamp;
static bool has_detected = false;
static bool has_good_link = false;

static int light_readings[READING_LENGTH] = {0};
static int num_light_readings = 0;


PROCESS(nbr_discovery_process, "cc2650 neighbour discovery process");
PROCESS(light_reading_process, "Light Reading Process");
AUTOSTART_PROCESSES(&nbr_discovery_process, &light_reading_process);


static void init_opt_reading(void);
static void get_light_reading(void);

/**
* LIGHT READING FROM ASSIGNMENT 2
*/
static void get_light_reading() {
  int value;
  value = opt_3001_sensor.value(0);

  if(value != CC26XX_SENSOR_READING_ERROR) {
    light_readings[num_light_readings % READING_LENGTH] = value;
    num_light_readings++;
  }
  init_opt_reading();
}

static void init_opt_reading(void) {
  for (int i = 0; i < READING_LENGTH; i++) {
    light_readings[i] = LIGHT_DEFAULT;
  }
  SENSORS_ACTIVATE(opt_3001_sensor);
}

PROCESS_THREAD(light_reading_process, ev, data) {
  PROCESS_BEGIN();

  printf("Collecting light for an interval of 3 seconds\n");
  etimer_set(&interval_timer, CLOCK_SECOND * 3);

  while (1) {
    get_light_reading();
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&interval_timer));
    etimer_reset(&interval_timer);
  }

  PROCESS_END();
}

/**
* END OF LIGHT READING FROM ASSIGNMENT 2
*/


// Modified from nbr.c
void receive_packet_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
  if (len != sizeof(data_packet)) {
    return;
  }

  static data_packet_struct received_packet_data;
  memcpy(&received_packet_data, data, len);

  if (received_packet_data.device_type == data_packet.device_type) {
    return;
  }

  if (!has_detected) {
    printf("%lu DETECT %lu\n", RTIMER_NOW()/RTIMER_SECOND, received_packet_data.src_id);
    has_detected = true;
  }
  int rssi = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  printf("RSSI: %d\n", rssi);

  // If link quality is weak
  if (rssi < LINK_THRESHOLD) {
    has_good_link = false;
    return;
  }

  if (!has_good_link) {
    has_good_link = true;
  }

  memcpy(&data_light_packet.light_readings, light_readings, sizeof(light_readings));
  data_light_packet.timestamp = curr_timestamp;
  data_light_packet.seq++;
  nullnet_buf = (uint8_t *)&data_light_packet;
  nullnet_len = sizeof(data_light_packet);
  NETSTACK_NETWORK.output(&dest_addr);

  // reset the light readings
  for (int i = 0; i < READING_LENGTH; i++) {
    light_readings[i] = LIGHT_DEFAULT;
  }
  
  return;
}
// Assimilated from nbr.c
// Scheduler function for the sender of neighbour discovery packets
char sender_scheduler(struct rtimer *t, void *ptr) {

  static uint16_t i = 0;

  static int NumSleep=0;

  // Begin the protothread
  PT_BEGIN(&pt);

  // Get the current time stamp
  curr_timestamp = clock_time();
  printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp, curr_timestamp / CLOCK_SECOND,
         ((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);

  while(1){

    // radio on
    NETSTACK_RADIO.on();

    // send NUM_SEND number of neighbour discovery beacon packets
    for(i = 0; i < NUM_SEND; i++){

      // Initialize the nullnet module with information of packet to be trasnmitted
      nullnet_buf = (uint8_t *)&data_packet; //data transmitted
      nullnet_len = sizeof(data_packet); //length of data transmitted

      data_packet.seq++;

      curr_timestamp = clock_time();

      data_packet.timestamp = curr_timestamp;

      //printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", data_packet.seq, curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND);

      NETSTACK_NETWORK.output(&dest_addr); //Packet transmission


      // wait for WAKE_TIME before sending the next packet
      if(i != (NUM_SEND - 1)){

        rtimer_set(t, RTIMER_TIME(t) + WAKE_TIME, 1, (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);

      }

    }

    // sleep for a random number of slots
    if(SLEEP_CYCLE != 0){

      // radio off
      NETSTACK_RADIO.off();

      // get a value that is uniformly distributed between 0 and 2*SLEEP_CYCLE
      // the average is SLEEP_CYCLE
      NumSleep = random_rand() % (2 * SLEEP_CYCLE + 1);
      //printf(" Sleep for %d slots \n",NumSleep);

      // NumSleep should be a constant or static int
      for(i = 0; i < NumSleep; i++){
        rtimer_set(t, RTIMER_TIME(t) + SLEEP_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
        PT_YIELD(&pt);
      }

    }
  }

  PT_END(&pt);
}

// Modified from nbr.c
PROCESS_THREAD(nbr_discovery_process, ev, data) {
  PROCESS_BEGIN();
  data_light_packet.src_id = node_id;
  data_light_packet.seq = 0;
  data_packet.src_id = node_id;
  data_packet.device_type = SENDER_TYPE; // 0 for sender, 1 for receiver
  data_packet.seq = 0;
  nullnet_set_input_callback(receive_packet_callback);
  linkaddr_copy(&dest_addr, &linkaddr_null);

  printf("CC2650 neighbour discovery\n");
  printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(data_packet_struct));

  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

  PROCESS_END();
}