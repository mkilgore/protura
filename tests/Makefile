
TESTS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
TESTS_DIR := $(TESTS_DIR:/=)

$(eval $(call dir_rule,$(TEST_RESULTS_DIR)/ktest))
$(eval $(call dir_rule,$(TEST_RESULTS_DIR)/ext2))
$(eval $(call dir_rule,$(TEST_RESULTS_DIR)/pci))

PHONY += check
check: check-uapi check-kernel check-ext2 check-pci

PHONY += check-kernel
check-kernel: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(REAL_BOOT_TARGETS) | $(TEST_RESULTS_DIR)/ktest
	$(Q)./tests/scripts/run_ktest_tests.sh $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(TEST_RESULTS_DIR)/ktest

PHONY += check-ext2
check-ext2: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(REAL_BOOT_TARGETS) | $(TEST_RESULTS_DIR)/ext2
	$(Q)./tests/scripts/run_ext2_tests.sh $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(TEST_RESULTS_DIR)/ext2

PHONY += check-uapi
check-uapi:
	$(Q)./tests/scripts/verify_uapi.sh "$(CC)" "./include/uapi" "./arch/$(ARCH)/include/uapi"

PHONY += check-pci
check-pci: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(REAL_BOOT_TARGETS) | $(TEST_RESULTS_DIR)/pci
	$(Q)./tests/scripts/run_pci_tests.sh $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(TEST_RESULTS_DIR)/pci

PHONY += debug
debug: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img | $(LOGS_DIR)
	$(Q)./scripts/start_debug_session.sh $(LOGS_DIR) $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

PHONY += debug-ktest
debug-ktest: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img | $(LOGS_DIR)
	$(Q)./scripts/start_debug_session.sh $(LOGS_DIR) $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img "ktest.run=true"

PHONY += debug-net
debug-net: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img | $(LOGS_DIR)
	$(Q)./scripts/start_net_debug_session.sh $(LOGS_DIR) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

PHONY += qemu
qemu: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img | $(LOGS_DIR)
	$(Q)./scripts/start_session.sh $(LOGS_DIR) $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

