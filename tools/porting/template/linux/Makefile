include default.config

COMPILE_PREX ?=

CC = $(COMPILE_PREX)gcc

LOCAL_SRC_FILES := $(shell find tuyaos_adapter/include/utilities -name "*.c" -o -name "*.cpp" -o -name "*.cc")
LOCAL_SRC_FILES += $(shell find tuyaos_adapter/src -maxdepth 1 -name "*.c" -o -name "*.cpp" -o -name "*.cc" )
LOCAL_SRC_FILES += tuyaos_adapter/include/init/src/tkl_init_common.c
ifeq ($(CONFIG_ENABLE_WIFI), y)
LOCAL_SRC_FILES += $(shell find tuyaos_adapter/src/tkl_wifi "*.c" -o -name "*.cpp" -o -name "*.cc")
LOCAL_SRC_FILES += tuyaos_adapter/include/init/src/tkl_init_wifi.c
endif
ifeq ($(CONFIG_ENABLE_BLUETOOTH), y)
LOCAL_SRC_FILES += $(shell find tuyaos_adapter/src/tkl_bt "*.c" -o -name "*.cpp" -o -name "*.cc")
endif
ifeq ($(CONFIG_ENABLE_WIRED), y)
LOCAL_SRC_FILES += tuyaos_adapter/include/init/src/tkl_init_wired.c
endif

LOCAL_CFLAGS := $(addprefix -I, $(shell find tuyaos_adapter -type d)) 
LOCAL_CFLAGS += $(addprefix -I, $(shell find tuyaos_adapter/include -type d))
LOCAL_CFLAGS += $(foreach base_dir, $(HEADER_DIR), $(addprefix -I , $(base_dir)))

LOCAL_CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g

LOCAL_OUTPUT_DIR = $(OUTPUT_DIR)/$(EXAMPLE_NAME)_$(EXAMPLE_VER)
LOCAL_OUTPUT_DIR_OBJS = $(LOCAL_OUTPUT_DIR)/.objs

LOCAL_OBJS = $(addsuffix .o, $(LOCAL_SRC_FILES))
LOCAL_OBJS_OUT =  $(addprefix $(LOCAL_OUTPUT_DIR_OBJS)/, $(LOCAL_OBJS))
DEP_FILES = $(patsubst %.o,%.d,$(LOCAL_OBJS_OUT))

$(LOCAL_OUTPUT_DIR_OBJS)/%.c.o: %.c
	@echo "CC $<"
	@mkdir -p $(dir $@);
	$(CC) $(LOCAL_CFLAGS) -o $@ -c -MMD $<

-include $(DEP_FILES)

TUYAOS_LIB_DIR = $(LIBS_DIR)

LINKFLAGS = -lasan -L$(TUYAOS_LIB_DIR) $(addprefix -l, $(LIBS)) -pthread -lm  -lsystemd
all: app_excute
app_excute: $(LOCAL_OBJS_OUT)
	@mkdir -p $(LOCAL_OUTPUT_DIR_OBJS)
	$(CC) $(LOCAL_OBJS_OUT) $(LINKFLAGS) -o $(LOCAL_OUTPUT_DIR)/$(EXAMPLE_NAME)_$(EXAMPLE_VER)
	@echo "Build APP Finish"

.PHONY: all clean SHOWARGS app_excute pack
clean:
	rm -rf $(LOCAL_OUTPUT_DIR)