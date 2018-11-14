# Makefile for building NTP client

TARGET=libotntp.a

SOURCE_DIR ?= $(PWD)
BUILD_DIR ?= $(PWD)/build

SOURCES_C = $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS_C = $(patsubst $(SOURCE_DIR)/%,$(BUILD_DIR)/%,$(SOURCES_C:.c=.o))

all: $(BUILD_DIR)/$(TARGET)
install: all
	install -d $(INSTALL_DIR)/include/ntp
	install -t $(INSTALL_DIR)/include/ntp -m 0644 $(wildcard $(SOURCE_DIR)/*.h)
	install -d $(INSTALL_DIR)/lib
	install -t $(INSTALL_DIR)/lib -m 0644 $(BUILD_DIR)/$(TARGET)

clean:
	-rm -f $(BUILD_DIR)/$(TARGET) $(OBJECTS_C) $(BUILD_DIR)

$(BUILD_DIR)/$(TARGET): $(OBJECTS_C)
	$(AR) cr $@ $^
	$(RANLIB) $@

$(BUILD_DIR)/.dir:
	-mkdir $(BUILD_DIR)
	touch $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)/.dir
	$(CC) -MM -MT $@ -MF $@.d $(CPPFLAGS) $(CFLAGS) -c $<
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

-include $(wildcard $(BUILD_DIR)/*.d)
