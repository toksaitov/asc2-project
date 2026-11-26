# The clock of the chip is one of two, by the slide switch SW8: the 50 MHz
# of the board divided by 4, or the clock from the Pico, 12.5 MHz on the
# bench. The period asked for the Pico's clock is half of its 80 ns.
create_clock -name clk50 -period 20.000 [get_ports clk50]
create_generated_clock -name board_clk -source [get_ports clk50] -divide_by 4 [get_registers {div_q[1]}]
create_clock -name pi_clk -period 40.000 [get_ports pi_clk]
derive_pll_clocks
set_clock_groups -exclusive -group {pi_clk} -group {clk50 board_clk}

# The 200 MHz of the pin delays and the clock of the chip know nothing of
# each other, as the pins of a Tiny Tapeout chip and its clock do not
set_clock_groups -asynchronous -group {pll|auto_generated|pll1|clk[0]} -group {pi_clk clk50 board_clk}
derive_clock_uncertainty

# Every other pin changes at any time on the Pico's side or is read there
# long after it has changed: the handshake of the bus does the timing, and
# the chip synchronizes each input itself.
set_false_path -from [get_ports {pi_rst_n pi_ack pi_irq pi_stop pi_drives kbd_clk kbd_data pi_dbg_ck pi_dbg_di uio[*] SW[*]}]
set_false_path -to [get_ports {req chip_drives dbg_do uio[*] LEDR[*]}]
