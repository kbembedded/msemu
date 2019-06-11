MAKE_DIR=$(PWD)

MSEMU_DIR=$(MAKE_DIR)/src

all: msemu

msemu:
	@$(MAKE) -C $(MSEMU_DIR)

.PHONY: clean
clean:
	@$(MAKE) -C $(MSEMU_DIR) clean
