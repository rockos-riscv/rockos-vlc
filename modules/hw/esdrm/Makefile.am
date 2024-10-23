esdrmdir = $(pluginsdir)/esdrm

libesdrm_filters_plugin_la_SOURCES = hw/esdrm/vlc_esdrm.c hw/esdrm/vlc_esdrm.h

libesdrm_filters_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libesdrm_filters_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBVA_CFLAGS) $(AVCODEC_CFLAGS)
libesdrm_filters_plugin_la_LIBADD = $(AM_LIBADD) $(LIBVA_LIBS) $(AVCODEC_LIBS)

esdrm_LTLIBRARIES = libesdrm_filters_plugin.la
