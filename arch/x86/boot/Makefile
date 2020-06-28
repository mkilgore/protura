
# This target is for creating a multiboot compliant kernel
objs-y += boot_multiboot.o
objs-y += boot_multiboot_pagedir.o

$(tree)/boot_multiboot_pagedir.S: $(tree)/gen_initial_pagedir.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $< > $@

clean-list-y += boot_multiboot_pagedir.S
clean-list-y += vmprotura.ld

