#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#include "bson_msg.h"
#include "bson_parser.h"
#include "logger.h"
#include "sensor_types.h"
#include "utils.h"

static struct logger* l = NULL;
/* Forward declarations */
int serialize_chat_message(const struct PRTP_packet* msg, char* buf);
void init_bson_msg()
{
  l = init_logger(stdout, stderr, stderr, "BSON Message");
}

void shutdown_bson_msg()
{
  shutdown_logger(l);
}

/* Bson_iotmsg is a set of functions to extract the PRTP_packet meaning from bson values */
// int bson_iotmsg_string(struct PRTP_packet** msg, char* parent, char* key, char* value)
// {
//   int ret = -1;

//   if( !*msg ) {
//     log_error(l, "BSON parser: type field must be first.\n");
//     return ret;
//   }

//   if(parent) {
//     /* Make sure we are in "sids" array:) */
//     if( strcmp(parent, "sids") == 0 ) {
//       if( iotmsg_add_sid(*msg, value) != NULL ) ret = 0;
//     }

//     if( strcmp(parent, "data") == 0 )
//         if( strcmp(key, "sid") == 0 ){
//            log_error(l, "Inside sid parsing\n");
//           ret = iotmsg_set_sid(*msg, value);
//         }

//     /* Empty parent and "sid" key means we are in
//      * sids array and in one of the objects */
//     if( strcmp(parent, "") == 0 )
//       if( strcmp(key, "sid") == 0 )
//         if( iotmsg_add_sid(*msg, value) != NULL ) ret = 0;
//   }
//   /* No parent means we are in the top-level of the message */
//   else {
//     /* Sid here should be the one in update message */
//     if( strcmp(key, "sid") == 0 )
//       ret = iotmsg_set_sid(*msg, value);
//   }

//   return ret;
// }
// int bson_iotmsg_string(struct PRTP_packet** msg, char* parent, char* key, char* value)
// {
//   int ret = -1;

//   if( !*msg ) {
//     log_error(l, "BSON parser: type field must be first.\n");
//     return ret;
//   }

//   /* Handle CHAT_MESSAGE type */
//   if( (*msg)->type == CHAT_MESSAGE ) {
//     if(parent && strcmp(parent, "data") == 0) {
//       if( strcmp(key, "from") == 0 ) {
//         (*msg)->data.chat.from_client_id = strdup(value);
//         return 0;
//       }
//       else if( strcmp(key, "to") == 0 ) {
//         (*msg)->data.chat.to_client_id = strdup(value);
//         return 0;
//       }
//       else if( strcmp(key, "message") == 0 ) {
//         (*msg)->data.chat.message_text = strdup(value);
//         return 0;
//       }
//     }
//   }

//   /* Original code continues below... */
//   if(parent) {
//     if( strcmp(parent, "sids") == 0 ) {
//       if( iotmsg_add_sid(*msg, value) != NULL ) ret = 0;
//     }

//     if( strcmp(parent, "data") == 0 )
//         if( strcmp(key, "sid") == 0 ){
//            log_error(l, "Inside sid parsing\n");
//           ret = iotmsg_set_sid(*msg, value);
//         }

//     if( strcmp(parent, "") == 0 )
//       if( strcmp(key, "sid") == 0 )
//         if( iotmsg_add_sid(*msg, value) != NULL ) ret = 0;
//   }
//   else {
//     if( strcmp(key, "sid") == 0 )
//       ret = iotmsg_set_sid(*msg, value);
//   }

//   return ret;
// }
int bson_iotmsg_boolean(struct PRTP_packet** msg, char* parent, char* key, bool value)
{
  int ret = -1;
  /* For setting data value for device sensors */
  char zero = 0, one = 1;
  struct void_data d_zero = {sizeof(char), NULL};
  struct void_data d_one = {sizeof(char), NULL};
  d_zero.blob = &zero;
  d_one.blob = &one;

  if( !*msg ) {
    log_error(l, "BSON parser: type field must be first.\n");
    return ret;
  }

  if(parent) {
    /* Empty parent and "reliable" key means we are in
     * sids array and in one of the objects */
    if( strcmp(parent, "") == 0 )
      if( strcmp(key, "reliable") == 0 ) {
        ret = iotmsg_set_sid_reliable(*msg, value);
      }

    if( strcmp(parent, "data") == 0 )
      if( strcmp(key, "dev") == 0 ) {
        ret = iotmsg_set_data(*msg, value? &d_one: &d_zero);
      }
  }
  /* In the message "reliable" means it should be update message */
  else {
    if( strcmp(key, "reliable") == 0 ) {
      ret = iotmsg_update_set_reliable(*msg, value);
    }
  }

  log_debug(l, "BSON parser: debug .\n");

  if( strcmp(key, "end_marker") == 0 ) {
       ret = iotmsg_update_end_marker(*msg, value);
   }
  if( strcmp(key, "utilize_timestamp") == 0 ) {
       ret = iotmsg_update_utilize_timestamp(*msg, value);
  }
  if( strcmp(key, "fragmented") == 0 ) {
       ret = iotmsg_update_fragmented(*msg, value);
  }

   log_debug(l, "BSON parser: debug . %d %s \n", ret, key);
  return ret;
}

int bson_iotmsg_double(struct PRTP_packet** msg, char* parent, char* key, double value)
{
  int ret = -1;
  struct void_data val;
  val.len = sizeof(double);
  val.blob = &value;

  if( !*msg ) {
    log_error(l, "BSON parser: type field must be first.\n");
    return ret;
  }

  if(parent) {
    if( strcmp(parent, "data") == 0 ) {
      if( strcmp(key, "gps1") == 0 ) {
        log_error(l, "Inside gps1 parsing.\n");
        iotmsg_alloc_data(*msg);
        return iotmsg_copy_data(*msg, 0, &val);
      }
      if( strcmp(key, "gps2") == 0 ) {
        return iotmsg_copy_data(*msg, 1, &val);
      }
    }
  }

  return ret;
}
/* Fixed bson_iotmsg_string() function - replace in bson_msg.c */
/* In bson_msg.c - Update bson_iotmsg_string() to handle CHAT_USER_LIST */

int bson_iotmsg_string(struct PRTP_packet** msg, char* parent, char* key, char* value)
{
  int ret = -1;

  if( !*msg ) {
    log_error(l, "BSON parser: type field must be first.\n");
    return ret;
  }

  /* Handle CHAT_MESSAGE and CHAT_ROOM_JOIN types */
  if( (*msg)->type == CHAT_MESSAGE || (*msg)->type == CHAT_ROOM_JOIN ) {
    if(parent && strcmp(parent, "data") == 0) {
      if( strcmp(key, "from") == 0 ) {
        (*msg)->data.chat.from_client_id = strdup(value);
        return 0;
      }
      else if( strcmp(key, "to") == 0 ) {
        (*msg)->data.chat.to_client_id = strdup(value);
        return 0;
      }
      else if( strcmp(key, "message") == 0 ) {
        (*msg)->data.chat.message_text = strdup(value);
        return 0;
      }
    }
  }

  /* Handle UPDATE messages */
  if( (*msg)->type == UPDATE ) {
    if(parent && strcmp(parent, "data") == 0) {
      if( strcmp(key, "sid") == 0 ) {
        log_error(l, "Inside sid parsing\n");
        ret = iotmsg_set_sid(*msg, value);
        return ret;
      }
    }
  }

  /* Handle CHAT_USER_LIST - it uses "users" array like "sids" */
  if( (*msg)->type == CHAT_USER_LIST ) {
    if(parent) {
      if( strcmp(parent, "users") == 0 ) {
        if( iotmsg_add_sid(*msg, value) != NULL ) ret = 0;
        return ret;
      }
    }
  }

  /* Handle other message types with sid fields */
  if(parent) {
    if( strcmp(parent, "sids") == 0 ) {
      if( iotmsg_add_sid(*msg, value) != NULL ) ret = 0;
    }

    if( strcmp(parent, "") == 0 ) {
      if( strcmp(key, "sid") == 0 ) {
        if( iotmsg_add_sid(*msg, value) != NULL ) ret = 0;
      }
    }
  }
  else {
    /* No parent means top-level */
    if( strcmp(key, "sid") == 0 ) {
      ret = iotmsg_set_sid(*msg, value);
    }
  }

  return ret;
}
/* Fixed bson_iotmsg_int32() function - replace in bson_msg.c */

int bson_iotmsg_int32(struct PRTP_packet** msg, char* parent, char* key, int32_t value)
{
  struct void_data val;
  val.len = sizeof(int32_t);
  val.blob = &value;

  /* Handle type field first */
  if (strncmp(key, "type", 4) == 0) {
    if(*msg) {
      log_error(l, "BSON parser: PRTP_packet type field appears twice!\n");
      return -1;
    }
    else {
      *msg = create_iotmsg(value);
      return 0;
    }
  }

  if( !*msg ) {
    log_error(l, "BSON parser: type field must be first.\n");
    return -1;
  }

  /* Handle CHAT_MESSAGE and CHAT_ROOM_JOIN timestamp */
  if( ((*msg)->type == CHAT_MESSAGE || (*msg)->type == CHAT_ROOM_JOIN) && 
      parent && strcmp(parent, "data") == 0 ) {
    if( strcmp(key, "timestamp") == 0 ) {
      (*msg)->data.chat.timestamp = value;
      return 0;
    }
  }

  /* Handle common fields */
  if (strncmp(key, "seq_no", 6) == 0)
    return iotmsg_set_seq(*msg, value);

  if (strncmp(key, "timestamp", 9) == 0)
    return iotmsg_set_timestamp(*msg, value);

  if (strncmp(key, "frag_no", 7) == 0) {
    return iotmsg_set_frag_no(*msg, value);
  }
  
  if (strncmp(key, "frag_total", 10) == 0) {
    return iotmsg_set_frag_total(*msg, value);
  }

  if(parent) {
    if( strcmp(parent, "") == 0 ) {
      if( strcmp(key, "status") == 0 )
        return iotmsg_set_status(*msg, value);
    }

    if( strcmp(parent, "data") == 0 ) {
      if (strcmp(key, "sensor_type") == 0) {
        log_error(l, "Inside sensor type parsing.\n");
        int ret = iotmsg_set_sensor_type(*msg, value);
        if (ret == 0) {
          iotmsg_alloc_data(*msg);
        }
        log_error(l, "Inside sensor type parsing end.\n");
        return ret;
      }

      if( strcmp(key, "temp1") == 0 ) {
        log_error(l, "Inside temp1 parsing.\n");
        return iotmsg_copy_data(*msg, 0, &val);
      }

      if( strcmp(key, "temp2") == 0 ) {
        return iotmsg_copy_data(*msg, 1, &val);
      }
    }
  }

  return -1;
}
int bson_iotmsg_binary(struct PRTP_packet** msg, char* parent, char* key, void* data)
{
  int ret = -1;
  struct void_data val;
  val.len = *(uint32_t*)data;
  /* There is also a subtype (1 byte) */
  val.blob = (char*)data + 1 + sizeof(uint32_t);

  if( !*msg ) {
    log_error(l, "BSON parser: type field must be first.\n");
    return ret;
  }

  if(parent) {
    if( strcmp(parent, "data") == 0 ) {
      if( strcmp(key, "cam") == 0 ) {
        ret = iotmsg_set_data(*msg, &val);
      }
    }
  }

  return ret;
}

#define MOVE_CUR do{ doc_len += ow_len; if(cur) cur += ow_len; } while(0); 
#define MOVE_CUR_IN MOVE_CUR do{ in_doc_len += ow_len;} while(0); 
#define MOVE_CUR_IN2 MOVE_CUR_IN do{ in_doc_len2 += ow_len;} while(0); 

#define WRITE_ZERO_CUR do{ doc_len++; if(cur) {*cur = '\0'; cur++;}} while(0);
#define WRITE_ZERO_CUR_IN WRITE_ZERO_CUR do{ in_doc_len++; } while(0);
#define WRITE_ZERO_CUR_IN2 WRITE_ZERO_CUR_IN do{ in_doc_len2++; } while(0);

int serialize_list(const struct PRTP_packet* msg, char* buf)
{
  uint32_t doc_len = 0, ow_len;
  char* cur = buf;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", LIST ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

    log_debug(l, "BSON msg: serializing.\n");
  WRITE_ZERO_CUR
  write_bson_doclen( buf, doc_len );

  return doc_len;
}

int serialize_unsubscribe(const struct PRTP_packet* msg, char* buf)
{
  uint32_t doc_len = 0, ow_len;
  char* cur = buf;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", UNSUBSCRIBE ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  log_debug(l, "BSON msg: serializing.\n");
  WRITE_ZERO_CUR
  write_bson_doclen( buf, doc_len );

  return doc_len;
}
int serialize_chat_user_list(const struct PRTP_packet* msg, char* buf)
{
  uint32_t doc_len = 0, in_doc_len = 0, ow_len;
  char* cur = buf;
  char* tmp;
  struct iotmsg_node* node;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", CHAT_USER_LIST ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  ow_len = write_bson_array( cur, "users" ); MOVE_CUR
  tmp = cur;
  in_doc_len = 0;
  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR_IN

  for( node = msg->data.blob; node != NULL; node = node->next )
  {
    ow_len = write_bson_string( cur, "", node->id ); MOVE_CUR_IN
  }
  WRITE_ZERO_CUR_IN
  write_bson_doclen( tmp, in_doc_len );

  WRITE_ZERO_CUR
  write_bson_doclen( buf, doc_len );

  return doc_len;
}
int serialize_list_response(const struct PRTP_packet* msg, char* buf)
{
  /* Can be used for itoa and writing array according to BSON spec
   * Now writing array with empty keys
   * int i;
  */
  uint32_t doc_len = 0, in_doc_len = 0, ow_len;
  char* cur = buf;
  char* tmp;
  struct iotmsg_node* node;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", LIST_RESPONSE ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  ow_len = write_bson_array( cur, "sids" ); MOVE_CUR
  tmp = cur;
  in_doc_len = 0;
  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR_IN

  for( node = msg->data.blob; node != NULL; node = node->next )
  {
    ow_len = write_bson_string( cur, "", node->id ); MOVE_CUR_IN
  }
  WRITE_ZERO_CUR_IN
  write_bson_doclen( tmp, in_doc_len );

  WRITE_ZERO_CUR
  write_bson_doclen( buf, doc_len );

  return doc_len;
}

int serialize_subscribe(const struct PRTP_packet* msg, char* buf)
{
  /* Can be used for itoa and writing array according to BSON spec
   * Now writing array with empty keys
   * int i;
  */

  log_debug(l, "---Sending subscribe by serializing PRTP packet to bson bsg.------\n");
  uint32_t doc_len = 0, in_doc_len = 0, in_doc_len2 = 0, ow_len;
  char* cur = buf;
  char* tmp, *tmp2;
  struct iotmsg_node* node;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", SUBSCRIBE ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  ow_len = write_bson_array( cur, "sids" ); MOVE_CUR
  in_doc_len = 0;
  tmp = cur;
  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR_IN
  for( node = msg->data.blob; node != NULL; node = node->next )
  {
    ow_len = write_bson_object( cur, "" ); MOVE_CUR_IN
    in_doc_len2 = 0;
    tmp2 = cur;
    ow_len = write_bson_doclen( cur , 0 ); MOVE_CUR_IN2
    ow_len = write_bson_string( cur, "sid", node->id ); MOVE_CUR_IN2
    ow_len = write_bson_boolean( cur, "reliable", ((struct iotmsg_subscribe_node*)node)->reliable ); MOVE_CUR_IN2
    WRITE_ZERO_CUR_IN2
    write_bson_doclen( tmp2, in_doc_len2 );
  }
  WRITE_ZERO_CUR_IN
  write_bson_doclen( tmp, in_doc_len );

  WRITE_ZERO_CUR
  write_bson_doclen( buf, doc_len );

  return doc_len;
}

int serialize_subscribe_ack(const struct PRTP_packet* msg, char* buf)
{
  /* Can be used for itoa and writing array according to BSON spec
   * Now writing array with empty keys
   * int i;
  */
  uint32_t doc_len = 0, in_doc_len = 0, in_doc_len2 = 0, ow_len;
  char* cur = buf;
  char *tmp, *tmp2;
  struct iotmsg_node* node;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", SUBSCRIBE_ACK ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  ow_len = write_bson_array( cur, "sids" ); MOVE_CUR


  in_doc_len = 0;
  tmp = cur;
  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR_IN
  for( node = msg->data.blob; node != NULL; node = node->next )
  {
    ow_len = write_bson_object( cur, "" ); MOVE_CUR_IN
    in_doc_len2 = 0;
    tmp2 = cur;
    ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR_IN2
    ow_len = write_bson_string( cur, "sid", node->id ); MOVE_CUR_IN2
    ow_len = write_bson_int( cur, "status", ((struct iotmsg_subscribe_ack_node*)node)->status ); MOVE_CUR_IN2
    WRITE_ZERO_CUR_IN2
    write_bson_doclen( tmp2, in_doc_len2 );
  }
  WRITE_ZERO_CUR_IN
  write_bson_doclen( tmp, in_doc_len );

  WRITE_ZERO_CUR
  write_bson_doclen( buf, doc_len );

  return doc_len;
}


// int serialize_update(const struct PRTP_packet* msg, char* buf)
// {
//   uint32_t doc_len = 0, ow_len, in_doc_len;
//   char* cur = buf, *tmp;

//   ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
//   ow_len = write_bson_int( cur, "type", UPDATE ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
//   ow_len = write_bson_int( cur, "seq_no", msg->seq_no ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "end_marker", true ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR

//   ow_len = write_bson_int( cur, "frag_no", msg->frag_no ); MOVE_CUR
//   ow_len = write_bson_int( cur, "frag_total", msg->frag_total ); MOVE_CUR

//   if(msg->utilize_timestamp || msg->reliable) {
//     ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
//   }

//   in_doc_len = 0;
//   ow_len = write_bson_object( cur, "data" ); MOVE_CUR
//   tmp = cur;
//   ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR_IN
//   /* XXX Not following the specification here
//    * In there it says: float for temperature (using two ints)
//    * string for GPS (using two doubles)
//    */

// //   log_debug(l, "-----------Sending Updates to client. Setting payload--data---. %lf %lf\n",
// //   ((double*)(msg->data.blob))[0], ((double*)(msg->data.blob))[1]);


//   switch( msg->data.sensor_type )
//   {
//   case TEMP:
//        ow_len = write_bson_int( cur, "sensor_type", msg->data.sensor_type); MOVE_CUR_IN
//        ow_len = write_bson_string( cur, "sid", msg->data.sid ); MOVE_CUR_IN

//     ow_len = write_bson_int( cur, "temp1", ((int*)(msg->data.blob))[0] ); MOVE_CUR_IN
//     ow_len = write_bson_int( cur, "temp2", ((int*)(msg->data.blob))[1] ); MOVE_CUR_IN
//     break;
//   case DEVICE:
//        ow_len = write_bson_int( cur, "sensor_type", msg->data.sensor_type); MOVE_CUR_IN
//        ow_len = write_bson_string( cur, "sid", msg->data.sid ); MOVE_CUR_IN

//     ow_len = write_bson_boolean( cur, "dev", ((char*)(msg->data.blob))[0] != 0 ); MOVE_CUR_IN
//     break;
//   case GPS:
//        ow_len = write_bson_int( cur, "sensor_type", msg->data.sensor_type); MOVE_CUR_IN
//        ow_len = write_bson_string( cur, "sid", msg->data.sid ); MOVE_CUR_IN

//     ow_len = write_bson_double( cur, "gps1", ((double*)(msg->data.blob))[0] ); MOVE_CUR_IN
//     ow_len = write_bson_double( cur, "gps2", ((double*)(msg->data.blob))[1] ); MOVE_CUR_IN
//     break;
//   case CAMERA:
//        ow_len = write_bson_int( cur, "sensor_type", msg->data.sensor_type); MOVE_CUR_IN
//        ow_len = write_bson_string( cur, "sid", msg->data.sid ); MOVE_CUR_IN

//     ow_len = write_bson_binary( cur, "cam", &(msg->data) ); MOVE_CUR_IN
//     break;
//   default:
//     break;
//   }

//   WRITE_ZERO_CUR_IN
//   write_bson_doclen( tmp, in_doc_len );

//   WRITE_ZERO_CUR
//   write_bson_doclen( buf, doc_len );

//   return doc_len;
// }

int serialize_update(const struct PRTP_packet* msg, char* buf)
{
  uint32_t doc_len = 0, ow_len, in_doc_len;
  char* cur = buf, *tmp;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", UPDATE ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", msg->seq_no ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", true ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR

  ow_len = write_bson_int( cur, "frag_no", msg->frag_no ); MOVE_CUR
  ow_len = write_bson_int( cur, "frag_total", msg->frag_total ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  in_doc_len = 0;
  ow_len = write_bson_object( cur, "data" ); MOVE_CUR
  tmp = cur;
  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR_IN
  
  /* Access UPDATE-specific fields through the union */
  switch( msg->data.update.sensor_type )
  {
  case TEMP:
    ow_len = write_bson_int( cur, "sensor_type", msg->data.update.sensor_type); MOVE_CUR_IN
    ow_len = write_bson_string( cur, "sid", msg->data.update.sid ); MOVE_CUR_IN
    ow_len = write_bson_int( cur, "temp1", ((int*)(msg->data.update.blob.blob))[0] ); MOVE_CUR_IN
    ow_len = write_bson_int( cur, "temp2", ((int*)(msg->data.update.blob.blob))[1] ); MOVE_CUR_IN
    break;
    
  case DEVICE:
    ow_len = write_bson_int( cur, "sensor_type", msg->data.update.sensor_type); MOVE_CUR_IN
    ow_len = write_bson_string( cur, "sid", msg->data.update.sid ); MOVE_CUR_IN
    ow_len = write_bson_boolean( cur, "dev", ((char*)(msg->data.update.blob.blob))[0] != 0 ); MOVE_CUR_IN
    break;
    
  case GPS:
    ow_len = write_bson_int( cur, "sensor_type", msg->data.update.sensor_type); MOVE_CUR_IN
    ow_len = write_bson_string( cur, "sid", msg->data.update.sid ); MOVE_CUR_IN
    ow_len = write_bson_double( cur, "gps1", ((double*)(msg->data.update.blob.blob))[0] ); MOVE_CUR_IN
    ow_len = write_bson_double( cur, "gps2", ((double*)(msg->data.update.blob.blob))[1] ); MOVE_CUR_IN
    break;
    
  case CAMERA:
    ow_len = write_bson_int( cur, "sensor_type", msg->data.update.sensor_type); MOVE_CUR_IN
    ow_len = write_bson_string( cur, "sid", msg->data.update.sid ); MOVE_CUR_IN
    /* Pass the blob structure, not the union */
    ow_len = write_bson_binary( cur, "cam", &(msg->data.update.blob) ); MOVE_CUR_IN
    break;
    
  default:
    break;
  }

  WRITE_ZERO_CUR_IN
  write_bson_doclen( tmp, in_doc_len );

  WRITE_ZERO_CUR
  write_bson_doclen( buf, doc_len );

  return doc_len;
}


// int serialize_update_ack(const struct PRTP_packet* msg, char* buf)
// {

//   log_debug( l, "-------------Before Serializing Update acknowledgment.--------------\n");

//   uint32_t doc_len = 0, ow_len;
//   char* cur = buf;

//   ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
//   ow_len = write_bson_int( cur, "type", UPDATE_ACK ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
//   ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

//   if(msg->utilize_timestamp || msg->reliable) {
//     ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
//   }

//   ow_len = write_bson_string( cur, "sid", msg->data.sid ); MOVE_CUR
//   doc_len++; if(cur) *cur = '\0';
//   write_bson_doclen( buf, doc_len );

//   log_debug( l, "-------------After Serializing Update acknowledgment.--------------\n");

//   return doc_len;
// }
int serialize_update_ack(const struct PRTP_packet* msg, char* buf)
{
  log_debug( l, "-------------Before Serializing Update acknowledgment.--------------\n");

  uint32_t doc_len = 0, ow_len;
  char* cur = buf;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", UPDATE_ACK ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  /* For UPDATE_ACK, use msg->data.sid directly (not through update) */
  ow_len = write_bson_string( cur, "sid", msg->data.sid ); MOVE_CUR
  doc_len++; if(cur) *cur = '\0';
  write_bson_doclen( buf, doc_len );

  log_debug( l, "-------------After Serializing Update acknowledgment.--------------\n");

  return doc_len;
}

// int serialize_update_nack(const struct PRTP_packet* msg, char* buf)
// {
//   uint32_t doc_len = 0, ow_len;
//   char* cur = buf;
//   ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
//   ow_len = write_bson_int( cur, "type", UPDATE_NACK ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
//   ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
//   ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

//   if(msg->utilize_timestamp || msg->reliable) {
//     ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
//   }

//   ow_len = write_bson_string( cur, "sid", msg->data.sid ); MOVE_CUR
//   doc_len++; if(cur) *cur = '\0';
//   write_bson_doclen( buf, doc_len );

//   return doc_len;
// }

int serialize_update_nack(const struct PRTP_packet* msg, char* buf)
{
  uint32_t doc_len = 0, ow_len;
  char* cur = buf;
  
  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", UPDATE_NACK ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  /* For UPDATE_NACK, use msg->data.sid directly (not through update) */
  ow_len = write_bson_string( cur, "sid", msg->data.sid ); MOVE_CUR
  doc_len++; if(cur) *cur = '\0';
  write_bson_doclen( buf, doc_len );

  return doc_len;
}

int serialize_keep_alive(const struct PRTP_packet* msg, char* buf)
{
  uint32_t doc_len = 0, ow_len;
  char* cur = buf;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", KEEP_ALIVE ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  doc_len++; if(cur) *cur = '\0';
  write_bson_doclen( buf, doc_len );

  return doc_len;
}
/* From struct PRTP_packet to BSON buf
 * maxlen - the size of allocated buffer
 */
/* Fixed serialize_iotmsg function - replace lines 883-916 in bson_msg.c */
int serialize_iotmsg(const struct PRTP_packet* msg, char* buf, uint32_t maxlen)
{
  switch(msg->type) {
  case LIST:
    if( serialize_list(msg, NULL) > maxlen ) return -1;
    else return serialize_list(msg, buf);

  case LIST_RESPONSE:
    if( serialize_list_response(msg, NULL) > maxlen ) return -1;
    else return serialize_list_response(msg, buf);

  case SUBSCRIBE:
    if( serialize_subscribe(msg, NULL) > maxlen ) return -1;
    else return serialize_subscribe(msg, buf);

  case SUBSCRIBE_ACK:
    if( serialize_subscribe_ack(msg, NULL) > maxlen ) return -1;
    else return serialize_subscribe_ack(msg, buf);

  case UPDATE:
    if( serialize_update(msg, NULL) > maxlen ) return -1;
    else return serialize_update(msg, buf);

  case UPDATE_ACK:
    if( serialize_update_ack(msg, NULL) > maxlen ) return -1;
    else return serialize_update_ack(msg, buf);

  case UPDATE_NACK:
    if( serialize_update_nack(msg, NULL) > maxlen ) return -1;
    else return serialize_update_nack(msg, buf);

  case KEEP_ALIVE:
    if( serialize_keep_alive(msg, NULL) > maxlen ) return -1;
    else return serialize_keep_alive(msg, buf);

  case UNSUBSCRIBE:
    if( serialize_unsubscribe(msg, NULL) > maxlen ) return -1;
    else return serialize_unsubscribe(msg, buf);

  case CHAT_MESSAGE:
    if( serialize_chat_message(msg, NULL) > maxlen ) return -1;
    else return serialize_chat_message(msg, buf);

  case CHAT_ROOM_JOIN:
    if( serialize_chat_room_join(msg, NULL) > maxlen ) return -1;
    else return serialize_chat_room_join(msg, buf);

  case CHAT_USER_LIST:
    if( serialize_chat_user_list(msg, NULL) > maxlen ) return -1;
    else return serialize_chat_user_list(msg, buf);

  default:
    return -1;
  }

  return -1;
}











/* Parse chat message from BSON */
int bson_iotmsg_chat_string(struct PRTP_packet** msg, char* parent, char* key, char* value)
{
  int ret = -1;

  if( !*msg ) {
    log_error(l, "BSON parser: type field must be first.\n");
    return ret;
  }

  if( (*msg)->type != CHAT_MESSAGE ) return ret;

  if(parent && strcmp(parent, "data") == 0) {
    if( strcmp(key, "from") == 0 ) {
      (*msg)->data.chat.from_client_id = strdup(value);
      ret = 0;
    }
    else if( strcmp(key, "to") == 0 ) {
      (*msg)->data.chat.to_client_id = strdup(value);
      ret = 0;
    }
    else if( strcmp(key, "message") == 0 ) {
      (*msg)->data.chat.message_text = strdup(value);
      ret = 0;
    }
  }

  return ret;
}

int bson_iotmsg_chat_int32(struct PRTP_packet** msg, char* parent, char* key, int32_t value)
{
  int ret = -1;

  if( !*msg ) return ret;
  if( (*msg)->type != CHAT_MESSAGE ) return ret;

  if(parent && strcmp(parent, "data") == 0) {
    if( strcmp(key, "timestamp") == 0 ) {
      (*msg)->data.chat.timestamp = value;
      ret = 0;
    }
  }

  return ret;
}

/* Serialize chat message to BSON */
int serialize_chat_message(const struct PRTP_packet* msg, char* buf)
{
  uint32_t doc_len = 0, in_doc_len = 0, ow_len;
  char* cur = buf;
  char* tmp;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", CHAT_MESSAGE ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  /* Write chat data */
  in_doc_len = 0;
  ow_len = write_bson_object( cur, "data" ); MOVE_CUR
  tmp = cur;
  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR_IN
  
  ow_len = write_bson_string( cur, "from", msg->data.chat.from_client_id ); MOVE_CUR_IN
  
  if(msg->data.chat.to_client_id) {
    ow_len = write_bson_string( cur, "to", msg->data.chat.to_client_id ); MOVE_CUR_IN
  }
  
  ow_len = write_bson_string( cur, "message", msg->data.chat.message_text ); MOVE_CUR_IN
  ow_len = write_bson_int( cur, "timestamp", msg->data.chat.timestamp ); MOVE_CUR_IN

  WRITE_ZERO_CUR_IN
  write_bson_doclen( tmp, in_doc_len );

  WRITE_ZERO_CUR
  write_bson_doclen( buf, doc_len );

  return doc_len;
}

int serialize_chat_room_join(const struct PRTP_packet* msg, char* buf)
{
  // Same as serialize_chat_message but with CHAT_ROOM_JOIN type
  uint32_t doc_len = 0, in_doc_len = 0, ow_len;
  char* cur = buf;
  char* tmp;

  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR
  ow_len = write_bson_int( cur, "type", CHAT_ROOM_JOIN ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "end_marker", msg->end_marker ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "reliable", msg->reliable ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "fragmented", msg->fragmented ); MOVE_CUR
  ow_len = write_bson_boolean( cur, "utilize_timestamp", msg->utilize_timestamp ); MOVE_CUR
  ow_len = write_bson_int( cur, "seq_no", generate_random_sequence_no() ); MOVE_CUR

  if(msg->utilize_timestamp || msg->reliable) {
    ow_len = write_bson_int( cur, "timestamp", generate_ntp_timestamp() ); MOVE_CUR
  }

  in_doc_len = 0;
  ow_len = write_bson_object( cur, "data" ); MOVE_CUR
  tmp = cur;
  ow_len = write_bson_doclen( cur, 0 ); MOVE_CUR_IN
  
  ow_len = write_bson_string( cur, "from", msg->data.chat.from_client_id ); MOVE_CUR_IN
  ow_len = write_bson_int( cur, "timestamp", msg->data.chat.timestamp ); MOVE_CUR_IN

  WRITE_ZERO_CUR_IN
  write_bson_doclen( tmp, in_doc_len );

  WRITE_ZERO_CUR
  write_bson_doclen( buf, doc_len );

  return doc_len;
}
/* Update serialize_iotmsg() to handle CHAT_MESSAGE */
// int serialize_iotmsg(const struct PRTP_packet* msg, char* buf, uint32_t maxlen)
// {
//   switch(msg->type) {
//   case LIST:
//     if( serialize_list(msg, NULL) > maxlen ) return -1;
//     else return serialize_list(msg, buf);

//   /* ... existing cases ... */

//   case CHAT_MESSAGE:
//     if( serialize_chat_message(msg, NULL) > maxlen ) return -1;
//     else return serialize_chat_message(msg, buf);

//   /* ... rest of cases ... */
//   }
//   return -1;
// }

/* Update bson_iotmsg_string() to handle chat messages */
// int bson_iotmsg_string(struct PRTP_packet** msg, char* parent, char* key, char* value)
// {
//   int ret = -1;

//   if( !*msg ) {
//     log_error(l, "BSON parser: type field must be first.\n");
//     return ret;
//   }

//   /* Existing handlers for other message types... */

//   /* Handle chat message strings */
//   if( (*msg)->type == CHAT_MESSAGE ) {
//     return bson_iotmsg_chat_string(msg, parent, key, value);
//   }

//   return ret;
// }

/* Update bson_iotmsg_int32() similarly */
// int bson_iotmsg_int32(struct PRTP_packet** msg, char* parent, char* key, int32_t value)
// {
//   /* ... existing code ... */

//   /* Handle chat message integers */
  

//   return -1;
// }