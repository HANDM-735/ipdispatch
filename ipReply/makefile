TARGET := ipReply

# 平台设置
#PLATFORM=PC
#PLATFORM=ARMA9
#PLATFORM=ARM64

ifeq ($(PLATFORM),PC)
CC=gcc
CXX=g++
AR=ar
STRIP=strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
LIB_ROOT = /usr/local
LIBS := -lpthread -lm -ldl -lrt
else ifeq ($(PLATFORM),ARMA9)
CC=arm-none-linux-gnueabihf-gcc
CXX=arm-none-linux-gnueabihf-g++
AR=arm-none-linux-gnueabihf-ar
STRIP=arm-none-linux-gnueabihf-strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
LIBS := -lpthread -lm -ldl -lrt
else ifeq ($(PLATFORM),ARM64)
CC=aarch64-rockchip1031-linux-gnu-gcc
CXX=aarch64-rockchip1031-linux-gnu-g++
AR=aarch64-rockchip1031-linux-gnu-ar
STRIP=aarch64-rockchip1031-linux-gnu-strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
LIBS := -lpthread -lm -ldl -lrt
endif

CPPFLAGS += $(addprefix -I,$(INCLUDES))
CPPFLAGS += -MMD

# 设置头文件路径和库路径
INCLUDES := .
INCLUDES += ../include
INCLUDES += $(LIB_ROOT)/include
LIBPATH := .
LIBPATH += ./lib
LIBPATH += $(LIB_ROOT)/lib
RM := rm -f

# 以下部分无需修改
CSRCS := $(wildcard *.c) $(wildcard libjson/*.c) $(wildcard libxml/*.c) $(wildcard libmci/*.c)
CPPSRCS := $(wildcard *.cpp) $(wildcard libjson/*.cpp) $(wildcard libxml/*.cpp) $(wildcard libmci/*.cpp)
CCSRCS := $(wildcard *.cc) $(wildcard libjson/*.cc) $(wildcard libxml/*.cc) $(wildcard libmci/*.cc)
COBJS := $(patsubst %.c,%.o,$(CSRCS))
CPPOBJS := $(patsubst %.cpp,%.o,$(CPPSRCS))
CCOBJS := $(patsubst %.cc,%.o,$(CCSRCS))
CDEPS := $(patsubst %.o,%.d,$(COBJS))
CPPDEPS := $(patsubst %.o,%.d,$(CPPOBJS))
CCDEPS := $(patsubst %.o,%.d,$(CCOBJS))

.PHONY: all deps objs clean rebuild
all : $(TARGET)
deps : $(CDEPS) $(CPPDEPS) $(CCDEPS)
	$(CXX) $(CPPFLAGS) -MM $(CSRCS) $(CPPSRCS) $(CCSRCS)
objs : $(COBJS) $(CPPOBJS) $(CCOBJS)
clean :
	@$(RM) *.o
	@$(RM) *.d
	@$(RM) $(TARGET)
rebuild: clean all

-include $(CDEPS) $(CPPDEPS) $(CCDEPS)
$(TARGET) : $(COBJS) $(CPPOBJS) $(CCOBJS)
	$(CXX) -rdynamic -o $(TARGET) $(COBJS) $(CPPOBJS) $(CCOBJS) $(addprefix -L,$(LIBPATH)) $(LIBS)
	$(STRIP) $(TARGET)
	@$(RM) *.o
	@$(RM) *.d