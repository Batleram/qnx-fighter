#Build artifact type, possible values shared, static and exe
ARTIFACT_TYPE = static
PROJECT_NAME = rpi_spi

LDFLAGS_shared = -shared -o
ARTIFACT_NAME_shared = lib$(PROJECT_NAME).so

LDFLAGS_static = -static -a
ARTIFACT_NAME_static = lib$(PROJECT_NAME).a

LDFLAGS_exe = -o
ARTIFACT_NAME_exe = $(PROJECT_NAME)

ARTIFACT = $(ARTIFACT_NAME_$(ARTIFACT_TYPE))

#Build architecture/variant string, possible values: x86, armv7le, etc...
PLATFORM ?= aarch64le

#Build profile, possible values: release, debug, profile, coverage
BUILD_PROFILE ?= debug

CONFIG_NAME ?= $(PLATFORM)-$(BUILD_PROFILE)
OUTPUT_DIR = build/$(CONFIG_NAME)
TARGET = $(OUTPUT_DIR)/$(ARTIFACT)

#Compiler definitions

CC = qcc -Vgcc_nto$(PLATFORM)
CXX = q++ -Vgcc_nto$(PLATFORM)_cxx

LD = $(CC)

#Compiler flags for build profiles
CCFLAGS_release += -O2
CCFLAGS_debug += -g -O0 -fno-builtin
CCFLAGS_coverage += -g -O0 -ftest-coverage -fprofile-arcs
LDFLAGS_coverage += -ftest-coverage -fprofile-arcs
CCFLAGS_profile += -g -O0 -finstrument-functions
LIBS_profile += -lprofilingS

#Generic compiler flags (which include build type flags)
CCFLAGS_all += -Wall -fmessage-length=0 -fPIC
CCFLAGS_all += $(CCFLAGS_$(BUILD_PROFILE))

LDFLAGS_all += $(LDFLAGS_$(BUILD_PROFILE))
LIBS_all += $(LIBS_$(BUILD_PROFILE))
DEPS = -Wp,-MMD,$(@:%.o=%.d),-MT,$@

#Macro to expand files recursively: parameters $1 -  directory, $2 - extension, i.e. cpp
rwildcard = $(wildcard $(addprefix $1/*.,$2)) $(foreach d,$(wildcard $1/*),$(call rwildcard,$d,$2))

# includes
INCLUDES += -I../system/gpio

#Source list
SRCS = $(call rwildcard, ., c cpp)

#Object files list
OBJS = $(addprefix $(OUTPUT_DIR)/,$(addsuffix .o, $(basename $(SRCS))))

#Compiling rule for c
$(OUTPUT_DIR)/%.o: %.c
	-@mkdir -p $(OUTPUT_DIR)
	$(CC) -c $(DEPS) -o $@ $(INCLUDES) $(CCFLAGS_all) $(CCFLAGS) $<

#Compiling rule for c++
$(OUTPUT_DIR)/%.o: %.cpp
	-@mkdir -p $(OUTPUT_DIR)
	$(CXX) -c $(DEPS) -o $@ $(INCLUDES) $(CCFLAGS_all) $(CCFLAGS) $<

#Linking rule
$(TARGET):$(OBJS)
	$(LD) $(LDFLAGS_$(ARTIFACT_TYPE)) $(TARGET) $(LDFLAGS_all) $(LDFLAGS) $(OBJS) $(LIBS_all) $(LIBS)

#Rules section for default compilation and linking
all: $(TARGET)

CLEAN_DIRS := $(shell find build -type d)
CLEAN_PATTERNS := *.o *.d $(ARTIFACT_NAME_exe) $(ARTIFACT_NAME_shared) $(ARTIFACT_NAME_static)
CLEAN_FILES := $(foreach DIR,$(CLEAN_DIRS),$(addprefix $(DIR)/,$(CLEAN_PATTERNS)))

clean:
	rm -f $(CLEAN_FILES)

rebuild: clean all

#Inclusion of dependencies (object files to source and includes)
-include $(OBJS:%.o=%.d)
