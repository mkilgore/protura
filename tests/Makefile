
TESTS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
TESTS_DIR := $(TESTS_DIR:/=)

$(eval $(call dir_rule,$(TEST_RESULTS_DIR)/ktest))
$(eval $(call dir_rule,$(TEST_RESULTS_DIR)/ext2))
$(eval $(call dir_rule,$(TEST_RESULTS_DIR)/pci))
$(eval $(call dir_rule,$(TEST_RESULTS_DIR)/uapi))
$(eval $(call dir_rule,$(TEST_RESULTS_DIR)/symbol-table))

EXTRA_TEST_ARGS :=

ifdef MODULE
EXTRA_TEST_ARGS += ktest.module=$(MODULE)
endif

ifdef SINGLE_TEST
EXTRA_TEST_ARGS += ktest.single_test=$(SINGLE_TEST)
endif

PHONY += check
check: check-uapi check-kernel check-ext2 check-pci check-symbol-table

PHONY += check-kernel
check-kernel: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(KERNEL) | $(TEST_RESULTS_DIR)/ktest
	$(Q)./tests/scripts/run_ktest_tests.sh $(KERNEL) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(TEST_RESULTS_DIR)/ktest "$(EXTRA_TEST_ARGS)"

PHONY += check-ext2
check-ext2: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(KERNEL) | $(TEST_RESULTS_DIR)/ext2
	$(Q)./tests/scripts/run_ext2_tests.sh $(KERNEL) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(TEST_RESULTS_DIR)/ext2

PHONY += check-uapi
check-uapi: | $(TEST_RESULTS_DIR)/uapi
	$(Q)./tests/scripts/verify_uapi.sh "$(CC)" "$(TEST_RESULTS_DIR)/uapi" "./include/uapi" "./arch/$(ARCH)/include/uapi"

PHONY += check-symbol-table
check-symbol-table: $(KERNEL).full $(OBJ_DIR)/vmprotura_symbols2.c | $(TEST_RESULTS_DIR)/symbol-table
	$(Q)./tests/scripts/verify_symbol_table.sh $(KERNEL).full $(OBJ_DIR)/vmprotura_symbols2.c "$(TEST_RESULTS_DIR)/symbol-table"

PHONY += check-pci
check-pci: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(KERNEL) | $(TEST_RESULTS_DIR)/pci
	$(Q)./tests/scripts/run_pci_tests.sh $(KERNEL) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(TEST_RESULTS_DIR)/pci

PHONY += debug
debug: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(KERNEL) | $(LOGS_DIR)
	$(Q)./scripts/start_debug_session.sh $(LOGS_DIR) $(KERNEL) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

PHONY += debug-ktest
debug-ktest: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(KERNEL) | $(LOGS_DIR)
	$(Q)./scripts/start_debug_session.sh $(LOGS_DIR) $(KERNEL) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img "ktest.run=true $(EXTRA_TEST_ARGS)"

PHONY += debug-net
debug-net: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(KERNEL) | $(LOGS_DIR)
	$(Q)./scripts/start_net_debug_session.sh $(LOGS_DIR) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

PHONY += qemu
qemu: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(KERNEL) | $(LOGS_DIR)
	$(Q)./scripts/start_session.sh $(LOGS_DIR) $(KERNEL) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

