TARGET := ipApply

# 平台设置
#PLATFORM=PC
#PLATFORM=ARMA9
#PLATFORM=ARM64

# 设置头文件路径和库路径
INCLUDES := .
INCLUDES += ../include
INCLUDES += $(LIB_ROOT)/include
LIBPATH := .
LIBPATH += ./lib
LIBPATH += $(LIB_ROOT)/lib
RM := rm -f

ifeq ($(PLATFORM),CP_SYNC)
CC=gcc
CXX=g++
AR=ar
STRIP=strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
CPPFLAGS += -D DEV_TYP_CP_SYNC
LIBS := -Wl,-Bstatic -lboost_system -Wl,-Bdynamic -lpthread -lm -ldl -lrt -ldrv
INCLUDES += ./x86
LIBPATH += ./x86/libdriver/
BOOST_ROOT = /usr/local
INCLUDES += $(BOOST_ROOT)/include
LIBPATH += $(BOOST_ROOT)/lib

else ifeq ($(PLATFORM),ARMA9)
CC=arm-none-linux-gnueabihf-gcc
CXX=arm-none-linux-gnueabihf-g++
AR=arm-none-linux-gnueabihf-ar
STRIP=arm-none-linux-gnueabihf-strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
LIBS := -lpthread -lm -ldl -lrt
INCLUDES += ./arma9
LIBPATH += ./arma9/libdriver/

else ifeq ($(PLATFORM),CP_RK3588)
CC=aarch64-rockchip1031-linux-gnu-gcc
CXX=aarch64-rockchip1031-linux-gnu-g++
AR=aarch64-rockchip1031-linux-gnu-ar
STRIP=aarch64-rockchip1031-linux-gnu-strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
CPPFLAGS += -D DEV_TYP_CP_RK3588
LIBS := -Wl,-Bstatic -lboost_system -Wl,-Bdynamic -lpthread -lm -ldl -lrt -lrca_get_slots
INCLUDES += ./arm64
LIBPATH += ./arm64/libdriver_cp/
LIBPATH += ./arm64/librca_get_slots/
BOOST_ROOT = /usr/local/boost_arm64_none
INCLUDES += $(BOOST_ROOT)/include
LIBPATH += $(BOOST_ROOT)/lib

else ifeq ($(PLATFORM),ARM64_RDBI)
CC=aarch64-rockchip1031-linux-gnu-gcc
CXX=aarch64-rockchip1031-linux-gnu-g++
AR=aarch64-rockchip1031-linux-gnu-ar
STRIP=aarch64-rockchip1031-linux-gnu-strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
CPPFLAGS += -D DEV_TYP_RDBI
LIBS := -Wl,-Bstatic -lboost_system -Wl,-Bdynamic -lpthread -lm -ldl -lrt -ldriver
INCLUDES += ./arm64
LIBPATH += ./arm64/libdriver/
BOOST_ROOT = /home/rdci/usr1/tools/boost/boost_arm64_none
INCLUDES += $(BOOST_ROOT)/include
LIBPATH += $(BOOST_ROOT)/lib

else ifeq ($(PLATFORM),FT_SYNC)
CC=gcc
CXX=g++
AR=ar
STRIP=strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
CPPFLAGS += -D DEV_TYP_FT_SYNC
LIBS := -Wl,-Bstatic -lboost_system -Wl,-Bdynamic -lpthread -lm -ldl -lrt -ldrv
INCLUDES += ./x86
LIBPATH += ./x86/libdriver/
BOOST_ROOT = /usr/local
INCLUDES += $(BOOST_ROOT)/include
LIBPATH += $(BOOST_ROOT)/lib

else ifeq ($(PLATFORM),FT_PGB)
CC=gcc
CXX=g++
AR=ar
STRIP=strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
CPPFLAGS += -D DEV_TYP_FT_PGB
LIBS := -Wl,-Bstatic -lboost_system -Wl,-Bdynamic -lpthread -lm -ldl -lrt -ldrv
INCLUDES += ./x86
LIBPATH += ./x86/libdriver/
BOOST_ROOT = /usr/local
INCLUDES += $(BOOST_ROOT)/include
LIBPATH += $(BOOST_ROOT)/lib

else ifeq ($(PLATFORM),CPFTUDS_PGB)
CC=aarch64-rockchip1031-linux-gnu-gcc
CXX=aarch64-rockchip1031-linux-gnu-g++
AR=aarch64-rockchip1031-linux-gnu-ar
STRIP=aarch64-rockchip1031-linux-gnu-strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
CPPFLAGS += -D DEV_TYP_RDBI
LIBS := -Wl,-Bstatic -lboost_system -Wl,-Bdynamic -lpthread -lm -ldl -lrt
INCLUDES += ./arm64
LIBPATH += ./arm64/libdriver/
BOOST_ROOT = /home/rdci/usr1/tools/boost/boost_arm64_none
INCLUDES += $(BOOST_ROOT)/include
LIBPATH += $(BOOST_ROOT)/lib

else ifeq ($(PLATFORM),CPUDS_RCA)
CC=aarch64-rockchip1031-linux-gnu-gcc
CXX=aarch64-rockchip1031-linux-gnu-g++
AR=aarch64-rockchip1031-linux-gnu-ar
STRIP=aarch64-rockchip1031-linux-gnu-strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
CPPFLAGS += -D DEV_TYP_RDBI
LIBS := -Wl,-Bstatic -lboost_system -Wl,-Bdynamic -lpthread -lm -ldl -lrt
INCLUDES += ./arm64/libdriver/
LIBPATH += ./arm64/libdriver/
BOOST_ROOT = /home/rdci/usr1/tools/boost/boost_arm64_none
INCLUDES += $(BOOST_ROOT)/include
LIBPATH += $(BOOST_ROOT)/lib

else ifeq ($(PLATFORM),FTUDS_RCA)
CC=aarch64-rockchip1031-linux-gnu-gcc
CXX=aarch64-rockchip1031-linux-gnu-g++
AR=aarch64-rockchip1031-linux-gnu-ar
STRIP=aarch64-rockchip1031-linux-gnu-strip
CFLAGS = -g -O3 -Wall -std=gnu99
CPPFLAGS = -g -O3 -Wall -std=c++11
CPPFLAGS += -D DEV_TYP_RDBI
LIBS := -Wl,-Bstatic -lboost_system -Wl,-Bdynamic -lpthread -lm -ldl -lrt
INCLUDES += ./arm64/libdriver/
LIBPATH += ./arm64/libdriver/
BOOST_ROOT = /home/rdci/usr1/tools/boost/boost_arm64_none
INCLUDES += $(BOOST_ROOT)/include
LIBPATH += $(BOOST_ROOT)/lib
endif

CPPFLAGS += $(addprefix -I,$(INCLUDES))
CPPFLAGS += -MMD

# 以下部分无需修改
CSRCS := $(wildcard *.c)
CPPSRCS := $(wildcard *.cpp)
CCSRCS := $(wildcard *.cc)
COBJS := $(patsubst %.c,%.o,$(CSRCS))
CPPOBJS := $(patsubst %.cpp,%.o,$(CPPSRCS))
CCOBJS := $(patsubst %.cc,%.o,$(CCSRCS))
CDEPS := $(patsubst %.o,%.d,$(COBJS))
CPPDEPS := $(patsubst %.o,%.d,$(CPPOBJS))
CCDEPS := $(patsubst %.o,%.d,$(CCOBJS))

.PHONY: all deps objs clean rebuild
all : $(TARGET)
deps : $(CDEPS) $(CPPDEPS) $(CCDEPS)
objs : $(COBJS) $(CPPOBJS) $(CCOBJS)
clean :
	@$(RM) *.o
	@$(RM) *.d
	@$(RM) $(TARGET)
rebuild: clean all

-include $(CDEPS) $(CPPDEPS) $(CCDEPS)
$(TARGET) : $(COBJS) $(CPPOBJS) $(CCOBJS)
	$(CXX) $(CPPFLAGS) -o $(TARGET) $(COBJS) $(CPPOBJS) $(CCOBJS) $(addprefix -L,$(LIBPATH)) $(LIBS)
	$(STRIP) $(TARGET)
	@$(RM) *.o
	@$(RM) *.d