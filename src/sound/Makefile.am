AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# Extention library
noinst_LIBRARIES = \
	libalarm_sound.a

libalarm_sound_a_SOURCES = \
	alarm-sound.c \
	alarm-sound-config.c \
	incbin.c

# C compiler options
libalarm_sound_a_CFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-fpie \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/include \
	@LIBSYSTEMD_CFLAGS@ \
	-D_GNU_SOURCE


# configure option 
if ENABLE_ADDRESS_SANITIZER
CFLAGS   += -fsanitize=address
endif

if ENABLE_GCOV
CFLAGS   += -coverage
endif

CLEANFILES = *.gcda *.gcno
