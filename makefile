MAKE_DIR=$(PWD)

MSEMU_DIR=$(MAKE_DIR)/src
Z80EM_DIR=$(MSEMU_DIR)/z80em

all: z80em msemu

z80em:
	@$(MAKE) -C $(Z80EM_DIR)

msemu: z80em
	@$(MAKE) -C $(MSEMU_DIR)

.PHONY: clean
clean:
	@$(MAKE) -C $(Z80EM_DIR) clean
	@$(MAKE) -C $(MSEMU_DIR) clean
