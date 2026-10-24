#DEBUG=1
#BENCH=1
STRIP ?= /bin/true

# BEWARE that you can not use GraphTFT together with
# Text2Skin if you use Imlib2! (That's why I actually implemented ImageMagick)
# TBD: is this still true?

# External image lib to use: graphicsmagick, imagemagick, imlib2 or none.
# imagemagick needs ImageMagick 6, the Magick++ API of ImageMagick 7 differs.
IMAGELIB ?= graphicsmagick


# DO NOT EDIT BELOW THIS LINE UNLESS YOU KNOW WHAT YOU'RE DOING
# -------------------------------------------------------------
#

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.
# IMPORTANT: the presence of this macro is important for the Make.config
# file. So it must be defined, even if it is not used here!
#
PLUGIN = text2skin

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'const char \*cText2SkinPlugin::VERSION *=' $(PLUGIN).c | awk '{ print $$5 }' | sed -e 's/[";]//g')

### The C++ compiler and options:

CXX      ?= g++
CXXFLAGS ?= -fPIC -Wall -Werror=overloaded-virtual

### The directory environment:

VDRDIR = ../../..
LIBDIR = ../../lib
TMPDIR = /tmp

### Make sure that necessary options are included (VDR >= 1.7.13):

-include $(VDRDIR)/Make.global

### Allow user defined options to overwrite defaults:

-include $(VDRDIR)/Make.config

### The version number of VDR's plugin API (taken from VDR's "config.h"):

APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' \
                         $(VDRDIR)/config.h)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The object files (add further files here):

OBJS = $(PLUGIN).o loader.o display.o render.o common.o bitmap.o \
       file.o i18n.o theme.o cache.o setup.o status.o scroller.o screen.o \
       menu.o font.o \
       \
       xml/skin.o xml/parser.o xml/string.o xml/object.o xml/function.o \
       xml/type.o xml/display.o xml/xml.o

### Includes and Defines (add further entries here):

ifeq ($(IMAGELIB), imagemagick)
	DEFINES += -DHAVE_IMAGEMAGICK
	INCLUDES += $(shell pkg-config --cflags ImageMagick++)
	LIBS += $(shell pkg-config --libs ImageMagick++)
else ifeq ($(IMAGELIB), graphicsmagick)
	DEFINES += -DHAVE_IMAGEMAGICK # yep, really HAVE_IMAGEMAGICK here
	INCLUDES += $(shell pkg-config --cflags GraphicsMagick++)
	LIBS += $(shell pkg-config --libs GraphicsMagick++)
else ifeq ($(IMAGELIB), imlib2)
	DEFINES += -DHAVE_IMLIB2
	INCLUDES += $(shell pkg-config --cflags imlib2)
	LIBS += $(shell pkg-config --libs imlib2)
	OBJS += quantize.o
endif

ifdef DEBUG
	CXXFLAGS += -g -fno-inline
	DEFINES += -DDEBUG
else
	CXXFLAGS += -O3
ifdef BENCH
	DEFINES += -DBENCH
endif
endif

INCLUDES += -I$(VDRDIR)/include -I.

DEFINES += -D_GNU_SOURCE -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

### The main target:

all: libvdr-$(PLUGIN).so i18n

### Implicit rules:

%.o: %.c
	@echo CC $@
	$(Q)$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
LOCALEDIR = $(VDRDIR)/locale
I18Npo    = $(notdir $(wildcard $(PODIR)/*.po))
I18Npot   = $(PODIR)/$(PLUGIN).pot

$(I18Npot): $(wildcard *.c)
	@echo GT $@
	$(Q)xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address=http://projects.vdr-developer.org/projects/show/plg-text2skin -o $@ $^

%.po: $(I18Npot)
	@echo PO $@
	$(Q)msgmerge -U --no-wrap --no-location --backup=none -q $@ $<
	@touch $@

$(LOCALEDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.po
	@mkdir -p $(dir $@)
	@echo MO $@
	$(Q)msgfmt -c -o $@ $<

.PHONY: i18n
i18n: $(I18Npo:%.po=$(LOCALEDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo)

### Targets:

libvdr-$(PLUGIN).so: $(OBJS)
	@echo LD $@
	$(Q)$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@
	@echo IN $@ $(LIBDIR)/$@.$(APIVERSION)
	$(Q)cp --remove-destination $@ $(LIBDIR)/$@.$(APIVERSION)
	$(Q)$(STRIP) $(LIBDIR)/$@.$(APIVERSION)

dist: $(addprefix $(PODIR)/,$(I18Npo)) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) \
	     --exclude debian --exclude CVS --exclude .svn $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.so $(I18Npot) *.tgz core* *~
