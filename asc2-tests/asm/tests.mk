# The assembly tests of this directory. The emulator runs them (`../Makefile`),
# and so does the hardware description of the chip (`../chip/Makefile`).

# Programs that check themselves and park the CPU with the pass code in `r0`
EMU_TESTS=cpu_data_move cpu_add_sub cpu_bitwise cpu_shift cpu_mul_dot4 cpu_dot4 cpu_array cpu_learn cpu_sat \
          cpu_branch cpu_call_ret cpu_reg_special cpu_array_user cpu_dbg_lock \
          cpu_excpt_invalid cpu_privilege cpu_misaligned cpu_bus_error cpu_nested_trap cpu_fetch_next cpu_icache \
          mmu_lookup mmu_fault mmu_tlb mmu_user_fault mmu_soft_fill mmu_fetch_next \
          cpu_irq_pit cpu_irq_kbd rp_console rp_disk rp_lost_answer rp_screen rp_button

# Programs that pass if the CPU stops with a double fault
EMU_DOUBLE_FAULT_TESTS=cpu_double_fault rp_lost_vector
