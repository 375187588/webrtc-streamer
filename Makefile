CC = $(CROSS)g++ $(foreach sysroot,$(SYSROOT),--sysroot=$(sysroot))
AR = $(CROSS)ar
CFLAGS = -Wall -pthread -g -std=c++11 -Iinc
LDFLAGS = -pthread 

# live555
ifneq ($(wildcard $(SYSROOT)/usr/include/liveMedia/liveMedia.hh),)
#	CFLAGS += -DHAVE_LIVE555
	CFLAGS += -I $(SYSROOT)/usr/include/liveMedia  -I $(SYSROOT)/usr/include/groupsock -I $(SYSROOT)/usr/include/UsageEnvironment -I $(SYSROOT)/usr/include/BasicUsageEnvironment/
	LDFLAGS += -lliveMedia -lgroupsock -lUsageEnvironment -lBasicUsageEnvironment
endif

# webrtc
WEBRTCROOT?=../webrtc
WEBRTCBUILD?=Release
WEBRTCLIBPATH=$(WEBRTCROOT)/src/$(GYP_GENERATOR_OUTPUT)/out/$(WEBRTCBUILD)

CFLAGS += -DWEBRTC_POSIX -fno-rtti -D_GLIBCXX_USE_CXX11_ABI=0
CFLAGS += -I $(WEBRTCROOT)/src -I $(WEBRTCROOT)/src/chromium/src/third_party/jsoncpp/source/include
#detect
TESTDEBUG=$(shell nm $(wildcard $(WEBRTCLIBPATH)/obj/webrtc/media/rtc_media/videocapturer.o $(WEBRTCLIBPATH)/obj/webrtc/media/librtc_media.a) | c++filt | grep std::__debug::vector >/dev/null && echo debug)
ifeq ($(TESTDEBUG),debug)
	CFLAGS += -D_GLIBCXX_DEBUG=1
endif
LDFLAGS += -lX11 -ldl -lrt  

TARGET = webrtc-server_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD)
all: $(TARGET)

WEBRTC_LIB = $(shell find $(WEBRTCLIBPATH)/obj/base -name '*.o')
WEBRTC_LIB += $(shell find $(WEBRTCLIBPATH)/obj/webrtc -name '*.o' ! -path '*test*')
WEBRTC_LIB += $(shell find $(WEBRTCLIBPATH)/obj/third_party -name '*.o')
libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a: $(WEBRTC_LIB)
	$(AR) -rcT $@ $^



src/%.o: src/%.cpp
	$(CC) -o $@ -c $^ $(CFLAGS) 

FILES = $(wildcard src/*.cpp)
$(TARGET): $(subst .cpp,.o,$(FILES)) libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f src/*.o libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a $(TARGET)
