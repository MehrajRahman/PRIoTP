#ifndef MESSAGES_H
#define MESSAGES_H

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

#include "void_data.h"

#define BUFSIZE 1600

enum IOTMSG_TYPE {
  LIST,
  LIST_RESPONSE,
  SUBSCRIBE,
  SUBSCRIBE_ACK,
  UPDATE,
  UPDATE_ACK,
  UPDATE_NACK,
  KEEP_ALIVE,
  UNSUBSCRIBE
};

/* Nodes are for lists in iotmsgs
 * iotmsg_node is a base structure
 * all sids lists are pointers to iotmsg_node */
struct iotmsg_node {
  struct iotmsg_node* next;
  char* id;
};

/* Used in both list and list_response */
struct iotmsg_list_node {
    struct iotmsg_node node;
};

struct iotmsg_subscribe_node {
    struct iotmsg_node node;
    bool reliable;
};

struct iotmsg_subscribe_ack_node {
    struct iotmsg_node node;
    uint32_t status;
};

/* Base srtp_packet structure */
struct srtp_packet {
  uint32_t version:2;
  uint32_t type:4;
  bool end_marker;
  bool reliable;
  bool fragmented;
  bool utilize_timestamp;
  uint16_t seq_no;
  uint32_t timestamp;

  //not part of original srtp packet will remove once refactored
  uint32_t frag_no;
  uint32_t frag_total;

  struct void_data options;
  struct void_data data;
};

struct srtp_packet* create_iotmsg(enum IOTMSG_TYPE type);
void free_iotmsg(struct srtp_packet*);
void iotmsg_copy_sids(struct srtp_packet* dest, const struct iotmsg_node* src);
struct iotmsg_node* iotmsg_add_sid(struct srtp_packet* msg, const char* id);

/* Message type specific setters for reliable field
 * Set_sid is for subsribe message
 * Update_set is for update message */
int iotmsg_set_sid_reliable(struct srtp_packet* msg, bool reliable);
int iotmsg_update_set_reliable(struct srtp_packet* msg, bool reliable);

/* Used for update and update_ack message to set and get the sequence number and sid */
int iotmsg_set_seq(struct srtp_packet* msg, int seq);
int iotmsg_set_sid(struct srtp_packet* msg, const char* id);
int iotmsg_get_seq_no(const struct srtp_packet* msg);

/* Used for update messages to set the fragmentation numbers */
int iotmsg_set_frag_no(struct srtp_packet* msg, int no);
int iotmsg_set_frag_total(struct srtp_packet* msg, int total);

int iotmsg_get_frag_no(const struct srtp_packet* msg);
int iotmsg_get_frag_total(const struct srtp_packet* msg);

/* To set the status of subscription in subscribe ack */
int iotmsg_set_status(struct srtp_packet* msg, int status);

/* To allocate buffer for data and set the whole buffer */
int iotmsg_alloc_data(struct srtp_packet* msg);
int iotmsg_set_data(struct srtp_packet* msg, const struct void_data* data);
/*int iotmsg_set_data_range(struct srtp_packet* msg, const void* data, uint32_t offset, uint32_t length);
*/

/* To copy one value into already allocated data buffer */
int iotmsg_copy_data(struct srtp_packet* msg, int offset, const struct void_data* value);
int send_iotmsg(int sd, const struct srtp_packet* msg);


int iotmsg_update_end_marker(struct srtp_packet* msg, bool end_marker);
int iotmsg_update_utilize_timestamp(struct srtp_packet* msg, bool utilize_timestamp);
int iotmsg_update_fragmented(struct srtp_packet* msg, bool fragmented);
int iotmsg_set_timestamp(struct srtp_packet* msg, int timestamp);
int iotmsg_set_sensor_type(struct srtp_packet* msg, uint32_t type);

//helper function for core srtp protocol
uint32_t generate_ntp_timestamp();
uint16_t generate_random_sequence_no();

#endif
