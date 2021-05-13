# Name of the app executable to build
DXAPI_LIB_FULL=lib$(DXAPI_LIB).a

TEST_APP=dxapi-cpp-unit-tests

BUILD_TARGETS=$(DXAPI_LIB_FULL) $(TEST_APP)
BUILD_TARGET_PATHS=$(DXAPI_LIB_PATH) $(BINDIR)/$(TEST_APP)
CLEAN_TARGETS=clean-$(DXAPI_LIB_FULL) clean-$(TEST_APP)

# Names of C/C++ source files to build, without path/extension
OBJ_LIB=data_reader data_writer loader_manager session_handler symbol_translator tickstream_cache tickdb\
 tickstream_impl tickstream_properties qqlstate schema instrument_identity interval\
 data_reader_http tickcursor_http tickcursor_subscriptions tickdb_http tickdb_stream_requests tickloader_http uri\
 get_server_time_request list_streams_request load_streams_request selection_options_impl stream_options_impl\
 list_entities_request lock_stream_request periodicity_request schema_request timerange_request bg_proc_info_impl\
 tickdb_class_descriptor validate_qql_request bg_process_request xml_request charbuffer\
 platform streams tcp qpc_timer tinyxml2
 
# schema_change_request  

OBJ_TEST_APP=unit-tests-main tests-common\
  test-xmloutput session-handler tickstream-basic srw-locks misc-strings data-reader-unchunk timebase_starter
 

OBJ=$(OBJ_LIB) $(OBJ_TEST_APP)

# Source files inside $(SRCDIR)
SRCDIRS=tickdb tickdb/http tickdb/http/xml tickdb/http/xml/stream tickdb/http/xml/cursor platform util io xml/tinyxml2 ../../dxapi-unit-tests ../../

# Include directories
INCLUDES=include/native src/dxapi/native

# Library files
# LIBS=lib1 lib2
LIBS=

# Common preprocessor flags
COMMONPFLAGS=

PLATFORM=x64

#build Release by default
DEBUG?=0

ifeq ($(DEBUG),1)
	# C preprocessor flags to define
	PFLAGS=$(COMMONPFLAGS) _DEBUG DEBUG
	# C/C++ compiler flags
	CFLAGS=-O0 -g
    DXAPI_LIB=dxapi-$(PLATFORM)d
else
	# Note that NDEBUG will disable assertions
	PFLAGS=$(COMMONPFLAGS) NDEBUG
	CFLAGS=-O3
    DXAPI_LIB=dxapi-$(PLATFORM)
endif

# Example: rebuild with clang in Debug mode
# CC=clang;make clean;make DEBUG=1

#==============================================================================

# Binary files are put here
BINDIR=bin

# Temporary object files are put here
OBJDIR=obj

# C/C++ source code files and internal includes are here
SRCDIR=src/dxapi/native

#==============================================================================

# Default C/C++ compiler
CC?=clang

#==============================================================================

# Choose C++ compiler from C compiler
ifeq ($(CC),gcc)
	CXX=g++
endif
ifeq ($(CC),clang)
	CXX=clang++
endif

EMPTY:=
SPACE:= $(EMPTY) $(EMPTY)

CWARNINGS=-Wall -Wno-multichar -Wno-unused-function -Wno-unused-value
CINCLUDES=-I. $(INCLUDES:%=-I%)

# Some more advanced perf. flags
# FL = -m64 -msse2 -fno-exceptions -fno-unwind-tables

CCFLAGS=-fPIC $(CFLAGS) $(CINCLUDES) $(CWARNINGS) $(PFLAGS:%=-D %)
CXXFLAGS=-std=c++11 $(CCFLAGS)

DEFLIBS=stdc++ c m pthread
# Popular libs: rt-realtime extensions, pcap-packet capture, pthread-Posix threads
LDFLAGS=$(patsubst %, -l%, $(DEFLIBS) $(LIBS))


OUTDIRS=$(OBJDIR) $(BINDIR)
VPATH=$(SRCDIR):$(SRCDIRS:%=$(SRCDIR)/%)


#==============================================================================

build: dirs $(BUILD_TARGET_PATHS)

clean: $(CLEAN_TARGETS)
	-rm obj/*.o

$(OUTDIRS):
	mkdir $@

dirs: $(OUTDIRS)

$(OBJDIR)/%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $< 

$(OBJDIR)/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

.PHONY: build clean dirs
#==============================================================================

DXAPI_LIB_PATH=$(BINDIR)/$(DXAPI_LIB_FULL)
DXAPI_LIB_OBJ_PATHS=$(OBJ_LIB:%=$(OBJDIR)/%.o)

TEST_APP_OBJ_PATHS=$(OBJ_TEST_APP:%=$(OBJDIR)/%.o)

$(DXAPI_LIB_PATH): $(OBJ_LIB:%=$(OBJDIR)/%.o)
	$(AR) rcs $@ $^

$(BINDIR)/$(TEST_APP): $(TEST_APP_OBJ_PATHS) $(DXAPI_LIB_PATH)
	$(CXX) -o $@ $(TEST_APP_OBJ_PATHS) $(LDFLAGS) -L$(BINDIR)/ -l$(DXAPI_LIB)


$(CLEAN_TARGETS):
	-rm $(@:clean-%=$(BINDIR)/%)

.PHONY: $(CLEAN_TARGETS)

