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


#define WAKE_TIME RTIMER_SECOND/10
#define SLEEP_CYCLE 9
#define SLEEP_SLOT RTIMER_SECOND/10
#define NUM_SEND 2

#define RSSI_TO_DISTANCE_CONVERSION 55
#define PATH_LOSS 2.7
#define ATTENUATION 1
#define TX 5

#define PROXIMITY_DISTANCE_THRESHOLD 3.0
#define PROXIMITY_CONTACT_TIME (RTIMER_SECOND * 15)
#define PROXIMITY_ABSENT_TIME (RTIMER_SECOND * 30)

#define DETECT_INTERVAL 15
#define ABSENT_INTERVAL 30

#define SENDER_TYPE 0
#define RECEIVER_TYPE 1

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
} data_light_packet_struct;

static struct rtimer rt;
static struct pt pt;
static data_packet_struct data_packet;
static data_light_packet_struct data_light_packet;
unsigned long curr_timestamp;

static bool in_proximity = false;
static rtimer_clock_t first_contact_time, last_contact_time;

// Define variables to keep track of the last printed time
static unsigned long last_detect_time = 0;
static unsigned long last_absent_time = 0;

PROCESS(nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&nbr_discovery_process);

void receive_packet_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
  if (len == sizeof(data_packet)) {
    static data_packet_struct received_packet_data;
    memcpy(&received_packet_data, data, len);
    if (received_packet_data.device_type != data_packet.device_type) {
      // Print the details of the received packet
      //    printf("Received neighbour discovery packet %lu with rssi %d from %ld", received_packet_data.seq, (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI),received_packet_data.src_id);
      //    printf("\n");
      int rssi = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
      unsigned long time_diff_rtimer = 0;
      unsigned long time_diff_seconds = 0;
      printf("RSSI: %d\n", rssi);

      if (rssi >= -66) {
        if (!in_proximity) {
          in_proximity = true;
          first_contact_time = RTIMER_NOW();
        }
        last_contact_time = RTIMER_NOW();
        time_diff_rtimer = last_contact_time - first_contact_time;
        time_diff_seconds = time_diff_rtimer / RTIMER_SECOND;
        printf("Time difference since first contact: %ld\n", time_diff_seconds);
      } else {
        in_proximity = false;
        first_contact_time = RTIMER_NOW();
        if ((RTIMER_NOW() - last_contact_time) >= PROXIMITY_ABSENT_TIME) {
          unsigned long elapsed_time = (RTIMER_NOW() - last_absent_time) / RTIMER_SECOND;
          if (elapsed_time >= ABSENT_INTERVAL) {
            printf("%lu ABSENT %lu\n", last_contact_time / RTIMER_SECOND, received_packet_data.src_id);
            last_absent_time = RTIMER_NOW();
          }
        }
      }
    }

  } else if (len == sizeof(data_light_packet) && in_proximity) {
    unsigned long time_diff_rtimer = last_contact_time - first_contact_time;
    //unsigned long time_diff_seconds = time_diff_rtimer / RTIMER_SECOND;
    static data_light_packet_struct received_light_data;
    memcpy(&received_light_data, data, len);

    if (time_diff_rtimer >= PROXIMITY_CONTACT_TIME) {
      unsigned long elapsed_time = (last_detect_time == 0) ? DETECT_INTERVAL : (last_contact_time - last_detect_time) / RTIMER_SECOND;
      if (elapsed_time >= DETECT_INTERVAL) {
        printf("%lu DETECT %lu\n", first_contact_time/RTIMER_SECOND, received_light_data.src_id);
        printf("Light: Reading ");
        for (int i = 0; i < 10; i++) {
          printf("%d", received_light_data.light_readings[i]);
          if (i != 9) printf(", ");
        }
        printf("\n");
        last_detect_time = last_contact_time;
      }
    }
  }
}

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

PROCESS_THREAD(nbr_discovery_process, ev, data) {
  PROCESS_BEGIN();
  data_light_packet.src_id = node_id;
  data_light_packet.seq = 0;
  data_packet.src_id = node_id;
  data_packet.seq = 0;
  data_packet.device_type = RECEIVER_TYPE; // 0 for sender, 1 for receiver
  nullnet_set_input_callback(receive_packet_callback);
  linkaddr_copy(&dest_addr, &linkaddr_null);

  printf("CC2650 neighbour discovery\n");
  printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(data_packet_struct));

  rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

  PROCESS_END();
}