## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libpubsub.la
libpubsub_la_CFLAGS  = -Wall -pedantic
libpubsub_la_SOURCES = ../src/logger.c ../src/utils.c ../src/messages.c ../src/bson_parser.c ../src/bson_msg.c ../src/sensor_types.c \
					  ../src/validator.c ../src/sensor_logger.c ../src/transport.c ../src/fragment_buffer.c ../src/congestion_control.c

bin_PROGRAMS = SRTP_server SRTP_client
SRTP_server_SOURCES = SRTP_server.c ../src/sensors.h ../src/sensors.c ../src/clients.h ../src/clients.c \
	../src/sensor_parser.h ../src/sensor_parser.c ../src/subscriptions.h ../src/subscriptions.c ../src/clients_config.h ../src/clients_config.c
SRTP_server_CFLAGS  = -Wall -pedantic
SRTP_server_LDADD = libpubsub.la

SRTP_client_SOURCES = SRTP_client.c ../src/client_module.c ../src/active_flow.c
SRTP_client_LDADD = libpubsub.la
SRTP_client_CFLAGS  = -Wall -pedantic
