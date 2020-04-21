SUBDIRS= \
 pppoe

sbin_PROGRAMS = rfc4938client

rfc4938client_CXXFLAGS =    \
 $(libemane_CFLAGS)       

rfc4938client_LDFLAGS =     \
 $(libemane_LIBS)

rfc4938client_SOURCES =     \
 rfc4938.c                  \
 rfc4938_messages.c         \
 rfc4938_debug.c            \
 rfc4938_config.c           \
 rfc4938_parser.c           \
 rfc4938_io.c               \
 rfc4938_neighbor_manager.c \
 rfc4938_transport.cc

EXTRA_DIST=                 \
 pppoe_types.h              \
 rfc4938_messages.h         \
 rfc4938_debug.h            \
 rfc4938_config.h           \
 rfc4938_parser.h           \
 rfc4938.h                  \
 rfc4938_io.h               \
 rfc4938_neighbor_manager.h \
 rfc4938_transport.h        \
 rfc4938_transport_types.h  \
 rfc4938_worker_types.h     \
 rfc4938_types.h
