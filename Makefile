PRGFILE = tn3270
SRCS    = telnet.cc 3270.cc

LIBS		= -lssl
CLEAN           = 
INCDIRS		=

CXX=g++
CXXFLAGS=-g -Wall
OBJS=$(SRCS:.cc=.o)

all: $(PRGFILE)

depend:
	@makedepend -Iinclude -Y $(SRCS) 2> /dev/null
	@echo "okay"

.SUFFIXES: .cc

.cc.o:
	@echo compiling $*.cc ...
	$(CXX) -c $(CXXFLAGS) $(DEFINES) $(INCDIRS) $*.cc

$(PRGFILE): $(OBJS)
	@echo linking $(PRGFILE) ...
	@$(CXX) $(OBJS) -o $(PRGFILE) $(LIBDIRS) $(LIBS)
	@echo Ok

certificate.pem:
	openssl req -x509 -newkey rsa:2048 -nodes -days 3650 -out certificate.pem -keyout private.pem

clean:
	rm -f *~ DEADJOE *.o core* $(PRGFILE)
