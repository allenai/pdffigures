CC=g++ -std=c++11

# Is 0 or 1 depending on whether leptonica is in pkg-config
LEPT_IN_PKG_CONFIG := $(shell pkg-config --exists lept && echo $$?)

ifeq ($(LEPT_IN_PKG_CONFIG), 0)
	LIBS=`pkg-config --libs poppler lept`
	CFLAGS=-c -Wall `pkg-config --cflags poppler lept`
else
# Leptonic was not found in pkg-config. This occurs for some older versions of lepontica
# in the Debian package that do not have pkg-config setup so we need to add it in manually.
# This setting will work if leptonica was installed with apt-get, but you might
# need to change these lines if leptonica was installed elsewhere on you system.
	LIBS=`pkg-config --libs poppler` -llept
	CFLAGS=-c -Wall `pkg-config --cflags poppler` -I/usr/leptonica
endif

DEBUG_FLAGS=-g
RELEASE_FLAGS=-g -O3

DEBUG =? 0
ifeq (0, $(DEBUG))
	CFLAGS += $(RELEASE_FLAGS)
else
	CFLAGS += $(DEBUG_FLAGS)
endif

OBJECTS=PDFUtils.o TextUtils.o ExtractCaptions.o BuildCaptions.o ExtractRegions.o ExtractFigures.o pdffigures.o

pdffigures: $(OBJECTS)
	$(CC) -o pdffigures $(OBJECTS) $(LIBS)

.cpp.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *o pdffigures
