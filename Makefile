CC=g++ -std=c++11

UNAME := $(shell uname)
ifeq ($(UNAME),Linux)
# The older version of lepontica that is in the Debian package does not have 
# pkg-config setup so we need to add it in manually, this setting will work 
# if leptonica was installed with apt-get
	LIBS=`pkg-config --libs poppler` -llept
	CFLAGS=-c -Wall `pkg-config --cflags poppler` -I/usr/leptonica
else
	LIBS=`pkg-config --libs poppler lept`
	CFLAGS=-c -Wall `pkg-config --cflags poppler lept`
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
