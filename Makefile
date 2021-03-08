ifeq ($(strip $(DEVKITPRO)),)
    $(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR           ?=   $(CURDIR)

VERSION           =   0.0.0
COMMIT            =   $(shell git rev-parse --short HEAD)

# -----------------------------------------------

TARGET            =    nvjpg
EXTENSION         =    a
OUT               =    out
BUILD             =    build
SOURCES           =    lib
INCLUDES          =    include
CUSTOM_LIBS       =
ROMFS             =
EXAMPLES          =    examples/render-nx.cpp examples/render-deko3d.cpp

DEFINES           =    __SWITCH__ VERSION=\"$(VERSION)\" COMMIT=\"$(COMMIT)\"
ARCH              =    -march=armv8-a+crc+crypto+simd -mtune=cortex-a57 -mtp=soft -fpie
FLAGS             =    -Wall -pipe -g -O2 -ffunction-sections -fdata-sections
CFLAGS            =    -std=gnu11
CXXFLAGS          =    -std=gnu++20 -fno-rtti -fno-exceptions
ASFLAGS           =
LDFLAGS           =    -Wl,-pie -specs=$(DEVKITPRO)/libnx/switch.specs -g
LINKS             =    -lnx

PREFIX            =    aarch64-none-elf-
CC                =    $(PREFIX)gcc
CXX               =    $(PREFIX)g++
AR                =    $(PREFIX)ar
LD                =    $(PREFIX)g++

# -----------------------------------------------

export PATH      :=    $(DEVKITPRO)/tools/bin:$(DEVKITPRO)/devkitA64/bin:$(PORTLIBS)/bin:$(PATH)

PORTLIBS          =    $(DEVKITPRO)/portlibs/switch
LIBNX             =    $(DEVKITPRO)/libnx
LIBS              =    $(LIBNX) $(PORTLIBS)

# -----------------------------------------------

CFILES            =    $(shell find $(SOURCES) -name *.c)
CPPFILES          =    $(shell find $(SOURCES) -name *.cpp)
SFILES            =    $(shell find $(SOURCES) -name *.s -or -name *.S)
OFILES            =    $(CFILES:%=$(BUILD)/%.o) $(CPPFILES:%=$(BUILD)/%.o) $(SFILES:%=$(BUILD)/%.o)
DFILES            =    $(OFILES:.o=.d)

LIB_TARGET        =    $(if $(OUT:=), $(OUT)/lib$(TARGET).$(EXTENSION), .$(OUT)/lib$(TARGET).$(EXTENSION))
DEFINE_FLAGS      =    $(addprefix -D,$(DEFINES))
INCLUDE_FLAGS     =    $(addprefix -I$(CURDIR)/,$(INCLUDES)) $(foreach dir,$(CUSTOM_LIBS),-I$(CURDIR)/$(dir)/include) \
					   $(foreach dir,$(filter-out $(CUSTOM_LIBS),$(LIBS)),-I$(dir)/include)
LIB_FLAGS         =    $(foreach dir,$(LIBS),-L$(dir)/lib)


EXAMPLES_TARGET   =    $(if $(OUT:=), $(patsubst %, $(OUT)/%.nro, $(basename $(EXAMPLES))), \
                           $(patsubst %, .$(OUT)/%.nro, $(basename $(EXAMPLES))))
EXAMPLES_OFILES   =    $(EXAMPLES:%=$(BUILD)/%.o)
EXAMPLES_DFILES   =    $(EXAMPLES_OFILES:.o=.d)

# -----------------------------------------------

.SUFFIXES:

.PHONY: all clean examples

all: $(LIB_TARGET)
	@:

examples: $(EXAMPLES_TARGET) $(EXAMPLES_OFILES)
	@:

$(OUT)/%.nro: $(BUILD)/%.cpp.o | $(LIB_TARGET)
	@mkdir -p $(dir $@)
	@echo " NRO " $@
	@$(LD) $(ARCH) $^ -L $(OUT) -L $(DEVKITPRO)/libnx/lib -ldeko3d -l nvjpg -l nx -Wl,-pie -specs=$(DEVKITPRO)/libnx/switch.specs -o $(@:.nro=.elf)
	@nacptool --create $(notdir $(@:.nro=)) averne 0.0.0 $(@:.nro=.nacp)
	@elf2nro $(@:.nro=.elf) $@ --nacp=$(@:.nro=.nacp) > /dev/null

$(LIB_TARGET): $(OFILES)
	@echo " AR  " $@
	@mkdir -p $(dir $@)
	@$(AR) rc $@ $^

$(BUILD)/%.c.o: %.c
	@echo " CC  " $@
	@mkdir -p $(dir $@)
	@$(CC) -MMD -MP $(ARCH) $(FLAGS) $(CFLAGS) $(DEFINE_FLAGS) $(INCLUDE_FLAGS) -c $(CURDIR)/$< -o $@

$(BUILD)/%.cpp.o: %.cpp
	@echo " CXX " $@
	@mkdir -p $(dir $@)
	@$(CXX) -MMD -MP $(ARCH) $(FLAGS) $(CXXFLAGS) $(DEFINE_FLAGS) $(INCLUDE_FLAGS) -c $(CURDIR)/$< -o $@

$(BUILD)/%.s.o: %.s %.S
	@echo " AS  " $@
	@mkdir -p $(dir $@)
	@$(AS) -MMD -MP -x assembler-with-cpp $(ARCH) $(FLAGS) $(ASFLAGS) $(INCLUDE_FLAGS) -c $(CURDIR)/$< -o $@

clean:
	@echo Cleaning...
	@rm -rf $(BUILD) $(OUT)

-include $(DFILES) $(EXAMPLES_DFILES)
