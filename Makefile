.PHONY: all
all:
	@$(MAKE) -C lib
	@$(MAKE) -C src

.PHONY: debug
debug:
	@$(MAKE) debug -C lib
	@$(MAKE) debug -C src

.PHONY: clean
clean:
	@$(MAKE) clean -C lib
	@$(MAKE) clean -C src
