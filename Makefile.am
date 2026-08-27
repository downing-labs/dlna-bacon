# This file is part of MiniDLNA.
#
# MiniDLNA is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# MiniDLNA is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with MiniDLNA; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

AM_CFLAGS = -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
AM_CPPFLAGS = -I$(top_srcdir)/src

# po/ (gettext translations) intentionally not built -- see configure.ac

sbin_PROGRAMS = minidlnad
minidlnad_SOURCES = src/minidlna.c src/upnphttp.c src/upnpdescgen.c src/upnpsoap.c \
			src/upnpreplyparse.c src/minixml.c src/clients.c \
			src/getifaddr.c src/process.c src/upnpglobalvars.c \
			src/options.c src/minissdp.c src/uuid.c src/upnpevents.c \
			src/sql.c src/utils.c src/metadata.c src/scanner.c src/monitor.c \
			src/tivo_utils.c src/tivo_beacon.c src/tivo_commands.c \
			src/playlist.c src/image_utils.c src/albumart.c src/log.c \
			src/containers.c src/avahi.c src/webui.c src/tagutils/tagutils.c

if HAVE_KQUEUE
minidlnad_SOURCES += src/kqueue.c src/monitor_kqueue.c
else
minidlnad_SOURCES += src/select.c
endif

if HAVE_INOTIFY
minidlnad_SOURCES += src/monitor_inotify.c
endif

if HAVE_VORBISFILE
vorbislibs = -lvorbis -logg
else
if NEED_OGG
flacogglibs = -logg
endif
endif

if TIVO_SUPPORT
if HAVE_AVAHI
avahilibs = -lavahi-client -lavahi-common
endif
endif

minidlnad_LDADD = \
	@LIBJPEG_LIBS@ \
	@LIBID3TAG_LIBS@ \
	@LIBSQLITE3_LIBS@ \
	@LIBAVFORMAT_LIBS@ \
	@LIBEXIF_LIBS@ \
	-lFLAC $(flacogglibs) $(vorbislibs) $(avahilibs)

SUFFIXES = .tmpl .

.tmpl:
	sed -e s@:SBINDIR:@${sbindir}@ <$< >$@

GENERATED_FILES = \
	src/linux/minidlna.init.d.script

TEMPLATES = \
	src/linux/minidlna.init.d.script.tmpl

CLEANFILES = $(GENERATED_FILES)

EXTRA_DIST = $(TEMPLATES)
noinst_DATA = $(GENERATED_FILES)
