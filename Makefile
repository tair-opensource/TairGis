uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

COMPAT_MODULE = tairgis.so
all: $(COMPAT_MODULE)

COMPAT_DIR := build
TOPDIR := $(shell pwd)

ifeq ($(GCOV_MODE),TRUE)
	MODULE_FLAG = -DGCOV_MODE=TRUE
endif

$(COMPAT_MODULE): $(COMPAT_DIR)/tairgis.so
	cp $^ $@

$(COMPAT_DIR)/tairgis.so:
	rm -fr $(COMPAT_DIR)
	mkdir $(COMPAT_DIR) && cd $(COMPAT_DIR) && cmake .. ${MODULE_FLAG}
	$(MAKE) -C $(COMPAT_DIR)

ifeq ($(uname_S),Linux)
	    cp $(COMPAT_DIR)/src/libtairgis.so $(COMPAT_DIR)/tairgis.so
else
	    cp $(COMPAT_DIR)/src/libtairgis.dylib $(COMPAT_DIR)/tairgis.so
endif

clean:
	-rm -fr $(COMPAT_MODULE) $(COMPAT_DIR)
