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

#include <math.h>
#include <stdio.h>
#include <string.h>

#define WAKE_TIME RTIMER_SECOND / 10
#define SLEEP_CYCLE 9
#define SLEEP_SLOT RTIMER_SECOND / 10
#define NUM_SEND 2

#define RECEIVER_TYPE 1
#define RSSI_THRESHOLD -69

linkaddr_t dest_addr;

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
  int light_readings[10];
} data_packet_with_light_reading_struct;

static struct rtimer rt;
static struct pt pt;
static data_packet_struct data_packet;
static data_packet_with_light_reading_struct data_packet_with_light_reading;
unsigned long curr_timestamp;
static int print_counter;

static bool is_link_good = false;
static bool is_contacted = false;

PROCESS(nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&nbr_discovery_process);

void receive_packet_callback(const void *data, uint16_t len,
                             const linkaddr_t *src, const linkaddr_t *dest) {
  if (len == sizeof(data_packet_with_light_reading) && is_link_good) {
    static data_packet_with_light_reading_struct received_light_data;
    memcpy(&received_light_data, data, len);
    // printf("Light: ");
    for (int i = 0; i < 10; i++) {
      if (received_light_data.light_readings[i] == -1)
        break;
      printf("[%d] Light: %d\n", print_counter,
             received_light_data.light_readings[i]);
      // printf("%d", received_light_data.light_readings[i]);
      // if (i != 9)
      //   printf(", ");
      // printf("\n");
    }
    print_counter++;
    // printf("\n");
  }

  if (len == sizeof(data_packet)) {
    static data_packet_struct received_packet_data;
    memcpy(&received_packet_data, data, len);

    if (received_packet_data.device_type != data_packet.device_type) {
      int rssi = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
      // printf("RSSI: %d\n", rssi);

      if (!is_contacted) {
        is_contacted = true;
        printf("%lu DETECT %lu\n", RTIMER_NOW() / RTIMER_SECOND,
               received_packet_data.src_id);
      }

      if (rssi >= RSSI_THRESHOLD) {
        is_link_good = true;
      } else {
        is_link_good = false;
      }
    }
  }
}

// Scheduler function for the sender of neighbour discovery packets
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

      // printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", data_packet.seq,
      // curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp %
      // CLOCK_SECOND)*1000) / CLOCK_SECOND);

      NETSTACK_NETWORK.output(&dest_addr); // Packet transmission

      // wait for WAKE_TIME before sending the next packet
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

      // get a value that is uniformly distributed between 0 and 2*SLEEP_CYCLE
      // the average is SLEEP_CYCLE
      NumSleep = 7 - 1;
      // printf(" Sleep for %d slots \n",NumSleep);

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
  data_packet_with_light_reading.src_id = node_id;
  data_packet_with_light_reading.seq = 0;
  data_packet.src_id = node_id;
  data_packet.seq = 0;
  data_packet.device_type = RECEIVER_TYPE;
  nullnet_set_input_callback(receive_packet_callback);
  linkaddr_copy(&dest_addr, &linkaddr_null);

  printf("CC2650 neighbour discovery\n");
  printf("Node %d will be sending packet of size %d Bytes\n", node_id,
         (int)sizeof(data_packet_struct));

  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1,
             (rtimer_callback_t)sender_scheduler, NULL);

  PROCESS_END();
}