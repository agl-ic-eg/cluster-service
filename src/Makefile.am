AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

bin_PROGRAMS = cluster_service

cluster_service_SOURCES = \
	source/data-source.c \
	incbin.c \
	alarm-sound.c \
	data-pool-service.c \
	cluster-service-util.c \
	cluster-service.c

# options
# Additional library
cluster_service_LDADD = \
	$(top_builddir)/lib/libcluster_service_common.a \
	$(top_builddir)/lib/libcluster_service_static_configurator.a \
	-lrt -lpthread \
	@LIBSYSTEMD_LIBS@ \
	@LIBALSA_LIBS@

# C compiler options
cluster_service_CFLAGS = \
	-g \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/include \
	@LIBSYSTEMD_CFLAGS@ \
	@LIBALSA_CFLAGS@ \
	-D_GNU_SOURCE

# Linker optionscluster_service
cluster_service_LDFLAGS = 

# configure option 
if ENABLE_ADDRESS_SANITIZER
CFLAGS   += -fsanitize=address
endif

if ENABLE_GCOV
CFLAGS   += -coverage
endif

CLEANFILES = *.gcda *.gcno

if DATA_SOURCE_FAKE
SUBDIRS = source/fake-data
CFLAGS += -DDATA_SOURCE_FAKE
cluster_service_LDADD += source/fake-data/libsource_fake_data.a
endif

if DATA_SOURCE_SOCKET_CAN
SUBDIRS = source/socket-can
CFLAGS += -DDATA_SOURCE_SOCKET_CAN
cluster_service_LDADD += source/socket-can/libsource_socketcan.a
endif
