// Common definitions for both sender and receiver
#define WAKE_TIME RTIMER_SECOND / 10
#define SLEEP_CYCLE 9
#define SLEEP_SLOT RTIMER_SECOND / 10
#define NUM_SEND 2

#define LIGHT_DEFAULT -1
#define LIGHT_READING_LEN 10
#define RSSI_THRESHOLD -69

// For sender
#define SENDER_TYPE 0
#define SENDER_PRIME 13

// For receiver
#define RECEIVER_TYPE 1
#define RECEIVER_PRIME 7

// From nbr.c
typedef struct {
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;
  int device_type; // 0 for sender, 1 for receiver
} data_packet_struct;

typedef struct {
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;
  int light_readings[LIGHT_READING_LEN];
} data_light_packet_struct;
