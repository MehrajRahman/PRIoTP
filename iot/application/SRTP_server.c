#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <signal.h>

#include "../src/subscriptions.h"
#include "../src/bson_parser.h"
#include "../src/sensor_parser.h"
#include "../src/logger.h"
#include "../src/utils.h"
#include "../src/sensor_logger.h"
#include "../src/clients_config.h"

#define HOSTNAME_SIZE 255
#define FILENAME_SIZE 255

static struct logger* l = NULL;
static bool nack_support = true;
void usage()
{
  printf("\n Usage: SRTP_server -i <ip address> -p <sensor publish port> -s <client subscribe port> -l <sensor list> -c <clients config>\n\n");
}

int parse_parameters(int argc, char *argv[],
                     char* hostname, in_port_t* sensor_port, in_port_t* client_port,
                     char* sensor_list, char* clients_config)
{
  int c = 0;

  while((c = getopt(argc,argv,"hi:p:s:l:c:")) != -1){
    switch(c){
    case 'i':
      if( strlen(optarg) < HOSTNAME_SIZE )
        strcpy(hostname, optarg);
      else
      {
        log_error(l, "Error: given hostname/ip_address is too long.\n");
        return -1;
      }
      break;
    case 'p':
      if( (*sensor_port=atoi(optarg)) == 0 )
      {
        log_error(l, "Error: invalid sensor publish port. Give a number between 1 and 65535.\n");
        return -1;
      }
      break;
    case 's':
      if( (*client_port=atoi(optarg)) == 0 )
      {
        log_error(l, "Error: invalid client subscribe port. Give a number between 1 and 65535.\n");
        return -1;
      }
      break;

    case 'l':
      if( strlen(optarg) < FILENAME_SIZE )
        strcpy(sensor_list, optarg);
      else
      {
        log_error(l, "Error: given sensor list filename is too long.\n");
        return -1;
      }
      break;

    case 'c':
      if( strlen(optarg) < FILENAME_SIZE )
        strcpy(clients_config, optarg);
      else
      {
        log_error(l, "Error: given sensor list filename is too long.\n");
        return -1;
      }
      break;

    case 'h':
    default:
      usage();
      return -1;
      break;
    }
  }

  return 0;
}

int on_client_msg(struct client_node* node, const struct srtp_packet *msg, struct srtp_packet** response, int len)
{
  struct iotmsg_node* sid;
  struct sensor_node* sensor;
  struct srtp_packet* sack_msg;
  const struct srtp_packet* sub_msg;
  struct srtp_packet* list_resp_msg;
  *response = NULL;

  if( msg->type == LIST ) {
    list_resp_msg = create_iotmsg(LIST_RESPONSE);
    write_sensor_list( list_resp_msg );
    *response = list_resp_msg;
    return 0;
  }

  if( msg->type == SUBSCRIBE ) {
    sub_msg = msg;
    sack_msg = create_iotmsg(SUBSCRIBE_ACK);
    for( sid = sub_msg->data.blob; sid != NULL;
         sid = sid->next ) {
      iotmsg_add_sid( sack_msg, sid->id );
      sensor = get_sensor( sid->id );
      if( sensor != NULL ) {
        if( !subscription_exists( node, sensor ) ) {
          /* Reliable is now parsed */
          add_subscription( node, sensor, ((struct iotmsg_subscribe_node*)sid)->reliable );
          iotmsg_set_status( sack_msg, SUBSCRIBE_OK );
        }
        else
          iotmsg_set_status( sack_msg, SUBSCRIBE_ALREADY_EXISTS );
      }
      else {
        iotmsg_set_status( sack_msg, SUBSCRIBE_NOT_FOUND );
      }
    }
    *response = sack_msg;
    return 0;
  }

  if( msg->type == KEEP_ALIVE ) {
    log_debug(l, "Got a keep-alive from client.\n");
    gettimeofday(&node->last_seen, 0);
    return 0;
  }

  if( msg->type == UPDATE_ACK ) {
    log_debug(l, "Got update acknowledgement from client.\n");
    sensor = get_sensor( (msg)->data.sid );
    if( sensor ) subscription_on_ack(node, sensor, (msg)->seq_no, len);
    return 0;
  }

  if ( msg->type == UPDATE_NACK && nack_support ) {
    log_debug(l, "Got negative update acknowledgement from client.\n");
    sensor = get_sensor( (msg)->data.sid );
    if ( sensor ) subscription_on_nack(node, sensor);
    return 0;
  }

  return -1;
}

/* Main server loop, polls given sockets, calls appropriate reads */
int server_loop(int sensor_sd, int client_sd)
{
  struct pollfd fds[2];
  int timeout_msecs = RETRANSMIT_TIMEOUT;
  int ret = 0, res;
  struct srtp_packet* recv_msg;
  struct srtp_packet* send_msg;
  struct client_node* client = NULL;
  struct sensor_node* sensor = NULL;

  fds[0].fd = sensor_sd;
  fds[1].fd = client_sd;
  fds[0].events = POLLIN;
  fds[1].events = POLLIN;

  for(;;)
  {
    ret = poll( fds, 2, timeout_msecs );
    if( ret > 0 ) {
      if( fds[0].revents & POLLIN ) {
        if( (res = read_sensor(sensor_sd, &sensor)) != -1 ) {  //read from sensors
          mark_updated_subscriptions(sensor);  //load all subscribed client node  after received from sensor list (&sensor)
          if( res == 1 ) {
            /* Loggers are now added in sensors.c for consistency */
          }
        //log_debug(l, "Received %s bytes from sensor\n", (sensor->data.blob));
//          log_error(l, sensor->data);

          sensor_log(sensor->id, sensor->seq_no, sensor->type, &(sensor->data));
          if( sensor->type == CAMERA ) {
            if( strncmp((char*)(sensor->data.blob), "NO_MOTION", 9 ) != 0 )
              sensor_dump(sensor->id, &(sensor->data));
          }
        }
      }
      if( fds[1].revents & POLLIN ) {
        recv_msg = NULL;
        if( (res = read_client(client_sd, &client, &recv_msg)) != -1 ) {
          send_msg = NULL;
          if( on_client_msg(client, recv_msg, &send_msg, res) == -1 )
            log_error(l, "Error constructing a response for the client.\n");
          else if( send_msg != NULL ) {

            send_client_message( NULL, send_msg, client );
            free_iotmsg(send_msg);
          }
          free_iotmsg(recv_msg);
        }
      }
    }
    if( ret == 0 ) {
      /* Either do it here and calculate the timeout according to
       * the next client going to expire or... */
      /* prune_expired_clients(); */
    }
    /* ... do it here with more overhead */
    prune_expired_clients();
    send_updates(&timeout_msecs); //this is sending sensor data to subscribed client

    if( ret == -1 ) {
      log_error(l, "Poll errors.\n");
      exit(EXIT_FAILURE);
    }
  }
}

void init_systems()
{
  /* Initialize loggers, lots of boiler-plate code but allows
   * fine-grained control over logging */
  l = init_logger(stdout, stderr, stderr, "Server");
  init_clients();
  init_transport();
  init_subscriptions();
  // init_messages();
  init_sensor_types();
  init_sensors();
  init_bson_parser();
  init_sensor_parser();
  init_sensor_logger("./server_sensor_log");
}

void shutdown_systems()
{
  shutdown_logger(l);
  shutdown_clients();
  shutdown_transport();
  shutdown_subscriptions();
  // shutdown_messages();
  shutdown_sensor_types();
  shutdown_sensors();
  shutdown_bson_parser();
  shutdown_sensor_parser();
  shutdown_sensor_logger();
}

void signal_handler( int signum )
{
  log_print(l, "Shutdown.\n");
  shutdown_systems();
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
  char hostname[HOSTNAME_SIZE] = "localhost";
  char sensor_list[FILENAME_SIZE] = "sensor.list";
  char clients_config[FILENAME_SIZE] = "clients.conf";
  in_port_t sensor_port = 5000, client_port = 5001;
  int sensor_sd, client_sd;
  init_systems();
  if( signal(SIGINT, signal_handler) == SIG_IGN )
    signal(SIGINT, SIG_IGN);

  if( parse_parameters(argc, argv, hostname, &sensor_port, &client_port,
                       sensor_list, clients_config) < 0 ) exit(EXIT_FAILURE);

  log_print(l, "Will bind to: %s.\n", hostname);
  log_print(l, "Port for sensor data: %u.\n", sensor_port);
  log_print(l, "Port for client subscriptions: %u.\n", client_port);

  populate_sensor_list(sensor_list);
  read_clients_config(clients_config);

  sensor_sd = sensor_socket(hostname, sensor_port);
  if( sensor_sd == -1 ) exit(EXIT_FAILURE);

  client_sd = client_socket(hostname, client_port);
  if( client_sd == -1 ) exit(EXIT_FAILURE);

  server_loop(sensor_sd, client_sd);

  /* close(sensor_sd); */
  shutdown_systems();
  return EXIT_SUCCESS;
}

