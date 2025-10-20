#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

AM_CPPFLAGS += $(OPENSSL_CFLAGS)
# Define the name of the library and its source
lib_LTLIBRARIES = libeauth.la
libeauth_la_SOURCES = eauth.lib.c
libeauth_la_LDFLAGS = -version-info 0:0:0

# Define the client binary and its dependencies
bin_PROGRAMS = eauth_client
eauth_client_SOURCES = eauth_client.c
eauth_client_LDADD = libeauth.la $(OPENSSL_LIBS)

# Define the server binary and its dependencies
bin_PROGRAMS += eauth_server
eauth_server_SOURCES = eauth_server.c
eauth_server_LDADD = libeauth.la $(OPENSSL_LIBS)

# Header files to distribute
include_HEADERS = eauth.h

