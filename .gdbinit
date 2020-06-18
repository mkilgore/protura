
add-symbol-file ./bin/kernel/protura_x86_multiboot.sym 0xC0100000
directory ./src
directory ./src/fs
directory ./src/kernel
directory ./src/mm
directory ./src/net
directory ./src/pci
directory ./arch/x86
directory ./arch/x86/drivers
directory ./arch/x86/kernel
directory ./arch/x86/mm
target remote localhost:1234
#layout split
#layout regs
set pagination off

dashboard -layout expressions stack assembly source !threads !memory !history
dashboard stack -style limit 5


macro define offsetof(_type, _memb) ((long)(&((_type *)0)->_memb))
macro define container_of(_ptr, _type, _memb) ((_type *)((void *)(_ptr) - offsetof(_type, _memb)))
macro define task1 container_of(ktasks.list.next, struct task, task_list_node)
macro define task2 container_of(ktasks.list.next->next, struct task, task_list_node)
macro define task3 container_of(ktasks.list.next->next->next, struct task, task_list_node)
macro define task4 container_of(ktasks.list.next->next->next->next, struct task, task_list_node)
macro define task5 container_of(ktasks.list.next->next->next->next->next, struct task, task_list_node)
macro define task6 container_of(ktasks.list.next->next->next->next->next->next, struct task, task_list_node)
macro define task7 container_of(ktasks.list.next->next->next->next->next->next->next, struct task, task_list_node)
macro define task8 container_of(ktasks.list.next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task9 container_of(ktasks.list.next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task10 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task11 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task12 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task13 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task14 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task15 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task16 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task17 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task18 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task19 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)
macro define task20 container_of(ktasks.list.next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next->next, struct task, task_list_node)

define list_tasks
    set $list = ktasks.list.next
    set $cur = 1
    while $list != ktasks.list.prev
        set $list = container_of($list, struct task, task_list_node)
        printf "task %d: %d, %s\n", $cur, $list->pid, $list->name
        set $list = $list->task_list_node.next
        set $cur = $cur + 1
    end
end

define switch_task
    set $prev_eip = $eip
    set $prev_ebp = $ebp
    set $prev_esp = $esp

    set $esp = ($arg0)->context.esp
    set $ebp = ((struct x86_regs *)$esp)->ebp
    set $eip = ((struct x86_regs *)$esp)->eip
end

define end_switch_task
    set $esp = $prev_esp
    set $ebp = $prev_ebp
    set $eip = $prev_dip
end
