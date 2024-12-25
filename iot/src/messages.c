#include "messages.h"
#include "utils.h"
#include "bson_msg.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>

#include "sensor_types.h"

static struct logger* l = NULL;

void init_messages()
{
  l = init_logger(stdout, stderr, stderr, "Messages");
}

void shutdown_messages()
{
  shutdown_logger(l);
}

int send_iotmsg(int sd, const struct srtp_packet* msg) {
  char buf[BUFSIZE];
  size_t len = 0;
  len = serialize_iotmsg(msg, buf, BUFSIZE);
  return send(sd, buf, len, 0);
}

struct srtp_packet* create_iotmsg(enum IOTMSG_TYPE type)
{
  struct srtp_packet* result = NULL;

  switch(type) {
  case LIST:
    result = xalloc(sizeof(struct srtp_packet));
    break;
  case LIST_RESPONSE:
    result = xalloc(sizeof(struct srtp_packet));
    break;
  case SUBSCRIBE:
    result = xalloc(sizeof(struct srtp_packet));
    break;
  case SUBSCRIBE_ACK:
    result = xalloc(sizeof(struct srtp_packet));
    break;
  case UPDATE:
    result = xalloc(sizeof(struct srtp_packet));
    break;
  case UPDATE_ACK:
    result = xalloc(sizeof(struct srtp_packet));
    break;
  case UPDATE_NACK:
    result = xalloc(sizeof(struct srtp_packet));
    break;
  case KEEP_ALIVE:
    result = xalloc(sizeof(struct srtp_packet));
    break;
  case UNSUBSCRIBE:
    result = xalloc(sizeof(struct srtp_packet));
    break;
  }

  if( !result ) return NULL;

  result->type = type;

  return result;
}


void free_iotsids(struct srtp_packet* msg)
{
  struct iotmsg_node* sid, *next;

  switch(msg->type) {
  case LIST_RESPONSE:
    for( sid = (msg)->data.blob; sid != NULL; sid = next ) {
      next = sid->next;
      free(sid->id);
      free(sid);
    }
    break;
  case SUBSCRIBE:
    free( (msg)->data.blob );
    break;
  case SUBSCRIBE_ACK:
    for( sid = (msg)->data.blob; sid != NULL; sid = next ) {
      next = sid->next;
      free(sid->id);
      free(sid);
    }
    break;
  case UPDATE:
    free( (msg)->data.blob );
    break;
  case UPDATE_ACK:
    free( (msg)->data.blob );
    break;
  case UPDATE_NACK:
    free( (msg)->data.sid );
    break;
  }
}


/* Free srtp_packet structure. */
void free_iotmsg(struct srtp_packet* msg)
{
  free_iotsids(msg);
  free(msg);
}

void iotmsg_copy_sids(struct srtp_packet* dest, const struct iotmsg_node* src)
{
  const struct iotmsg_node* sid;

  for( sid = src; sid != NULL; sid = sid->next ) {
    iotmsg_add_sid(dest, sid->id);
  }
}

int iotmsg_set_sid(struct srtp_packet* msg, const char* id)
{
  int ret = -1;
  struct srtp_packet* upd_msg;
  struct srtp_packet* ack_msg;
  struct srtp_packet* nack_msg;

  switch(msg->type) {
  case UPDATE:
    upd_msg = msg;
    upd_msg->data.sid = xalloc(strlen(id) + 1);
    strcpy(upd_msg->data.sid, id);
    ret = 0;
    break;

  case UPDATE_ACK:
    ack_msg = msg;
    ack_msg->data.sid = xalloc(strlen(id) + 1);
    strcpy(ack_msg->data.sid, id);
    ret = 0;
    break;

  case UPDATE_NACK:
    nack_msg = msg;
    nack_msg->data.sid = xalloc(strlen(id) + 1);
    strcpy(nack_msg->data.sid, id);
    ret = 0;
    break;
  }
  return ret;
}

struct iotmsg_node* iotmsg_add_sid(struct srtp_packet* msg, const char* id)
{
  struct iotmsg_node* node = NULL;
  struct srtp_packet* sub_msg;
  struct srtp_packet* sack_msg;
  struct srtp_packet* list_msg;

  switch(msg->type) {
  case LIST_RESPONSE:
    list_msg =  msg;
    node = xalloc(sizeof(struct iotmsg_list_node));
    node->id = xalloc(strlen(id) + 1);
    strcpy(node->id, id);

    node->next = list_msg->data.blob;
    list_msg->data.blob = node;
    break;

  case SUBSCRIBE:
    sub_msg =  msg;
    node = xalloc(sizeof(struct iotmsg_subscribe_node));
    node->id = xalloc(strlen(id) + 1);
    strcpy(node->id, id);

    node->next = sub_msg->data.blob;
    sub_msg->data.blob = node;
    break;

  case SUBSCRIBE_ACK:
    sack_msg =  msg;
    node = xalloc(sizeof(struct iotmsg_subscribe_ack_node));
    node->id = xalloc(strlen(id) + 1);
    strcpy(node->id, id);

    node->next = sack_msg->data.blob;
    sack_msg->data.blob = node;
    break;
  }

  return node;
}

int iotmsg_set_sid_reliable(struct srtp_packet* msg, bool reliable)
{
  struct iotmsg_subscribe_node* sid;

  if( msg->type != SUBSCRIBE )
    return -1;

  sid = (struct iotmsg_subscribe_node*)(msg)->data.blob;
  sid->reliable = (reliable ? 1 : 0);

  return 0;
}

int iotmsg_update_set_reliable(struct srtp_packet* msg, bool reliable)
{
  msg->reliable =  (reliable ? 1 : 0);

  return 0;
}


int iotmsg_update_end_marker(struct srtp_packet* msg, bool end_marker)
{

  msg->end_marker =(end_marker ? 1 : 0);

  return 0;
}

int iotmsg_update_fragmented(struct srtp_packet* msg, bool fragmented)
{
  msg->fragmented = (fragmented ? 1 : 0);

  return 0;
}

int iotmsg_update_utilize_timestamp(struct srtp_packet* msg, bool utilize_timestamp)
{

  msg->utilize_timestamp = (utilize_timestamp ? 1 : 0);

  return 0;
}

int iotmsg_set_timestamp(struct srtp_packet* msg, uint32_t timestamp)
{
 msg->timestamp = timestamp;
 return 0;
}

int iotmsg_set_seq(struct srtp_packet* msg, uint32_t seq)
{
   msg->seq_no = seq;
   return 0;
}



int iotmsg_get_seq_no(const struct srtp_packet* msg)
{
  // int ret = -1;
  return msg->seq_no;
}

int iotmsg_set_frag_no(struct srtp_packet* msg, uint32_t no) {
  int ret = -1;
  switch(msg->type) {
  case UPDATE:
    (msg)->frag_no = no;
    ret = 0;
    break;
  }
  return ret;
}

int iotmsg_get_frag_no(const struct srtp_packet* msg) {
  int ret = -1;
  switch(msg->type) {
  case UPDATE:
    ret = (msg)->frag_no;
    break;
  }
  return ret;
}

int iotmsg_set_frag_total(struct srtp_packet* msg, uint32_t total) {
  int ret = -1;
  switch(msg->type) {
  case UPDATE:
    (msg)->frag_total = total;
    ret = 0;
    break;
  }
  return ret;
}

int iotmsg_set_sensor_type(struct srtp_packet* msg, uint32_t type) {
  int ret = -1;
  switch(msg->type) {
  case UPDATE:
    msg->data.sensor_type = type;
    ret = 0;
    break;
  }
  return ret;
}


int iotmsg_get_frag_total(const struct srtp_packet* msg) {
  int ret = -1;
  switch(msg->type) {
  case UPDATE:
    ret = (msg)->frag_total;
    break;
  }
  return ret;
}

int iotmsg_set_status(struct srtp_packet* msg, int status)
{
  if( msg->type != SUBSCRIBE_ACK )
    return -1;

  ((struct iotmsg_subscribe_ack_node*)(msg)->data.blob)->status = status;

  return 0;
}

int iotmsg_alloc_data(struct srtp_packet* msg)
{
  struct srtp_packet* upd_msg =  msg;

  if( msg->type != UPDATE )
    return -1;

  sensor_alloc_data(upd_msg->data.sensor_type, &(upd_msg->data));

  return (upd_msg->data.blob == NULL)? -1 : 0;
}

int iotmsg_set_data(struct srtp_packet* msg, const struct void_data* data)
{
  struct srtp_packet* upd_msg =  msg;

  if( msg->type != UPDATE )
    return -1;

  log_debug( l, "iot message set data sending update. %s %d\n", upd_msg->data.sid ,  upd_msg->data.sensor_type );

  return sensor_set_data(upd_msg->data.sensor_type, &(upd_msg->data), data);
}

/*int iotmsg_set_data_range(struct srtp_packet* msg, const void* data, uint32_t offset, uint32_t length)
{
  struct srtp_packet* upd_msg =  msg;
  int rval = -1;
  if ( msg->type != UPDATE)
    return -1;
  rval = sensor_set_data_range(upd_msg->sensor_type, &(upd_msg->data), data, offset, length);
  return rval;
}
*/
int iotmsg_copy_data(struct srtp_packet* msg, int offset, const struct void_data* value)
{
  int ret = -1;
  struct srtp_packet* upd_msg =  msg;

  if( msg->type != UPDATE )
    return ret;

  ret = sensor_copy_offset_data(upd_msg->data.sensor_type, offset, &(upd_msg->data), value);

  return ret;
}

uint32_t generate_ntp_timestamp() {
    time_t current_time = time(NULL); // Get current time in seconds

    // NTP timestamp: 32 bits - 16 bits for seconds, 16 bits for fractions
    uint32_t ntp_seconds = (uint32_t)(current_time - 2208988800U); // Adjust for 1900 epoch
    uint32_t ntp_fraction = 0; // No nanosecond precision here

    return (ntp_seconds << 16) | (ntp_fraction >> 16); // Combine seconds and fraction
}

uint16_t generate_random_sequence_no() {
    return (uint16_t)(rand() & 0xFFFF);  // Mask to get a 16-bit value
}