# CLK from the Pico: now only clocks the CLK wire monitor's toggle flop.
# Test 4 runs it up to 50 MHz; constrained at 62.5 MHz for margin.
create_clock -name pi_clk -period 16.000 [get_ports pi_clk]
# The board oscillator: 200 MHz pin delays, 150 MHz chip clock divider.
create_clock -name clk50 -period 20.000 [get_ports clk50]
derive_pll_clocks
# The chip clock, 150 MHz / div from a register; timed at the fastest
# division used, 3 (50 MHz). Only rising edges are used.
create_generated_clock -name chip_clk -source [get_pins {u_pll|auto_generated|pll1|clk[1]}] -divide_by 3 [get_registers {ck_q}]
derive_clock_uncertainty

# The chip logic (chip_clk), the pin delays (PLL clocks) and the CLK wire
# are treated as asynchronous to each other, as the chip is to the RP:
# every signal between them is a handshake strobe, data held stable
# around its strobe, or a setting changed only while the chip is in
# reset, and each crossing has a two-flop synchronizer.
set_clock_groups -asynchronous -group [get_clocks pi_clk] -group [get_clocks chip_clk] -group [get_clocks {clk50 u_pll|*}]

# ACK, IRQ, RST_N and the Pico's driving flag pass through synchronizers.
# uio is sampled only after ACK has been synchronized, and the protocol
# keeps it stable from before the ACK toggle until after the answer, so
# none of these inputs is timed against a clock.
set_false_path -from [get_ports {pi_ack pi_rst_n pi_dbg_di pi_irq pi_dbg_ck uio[*]}]

# REQ, uio and dbg_do are read by the Pico through its own synchronizers,
# the same handshake rule in the other direction. LEDs and displays are
# static indicators.
set_false_path -to [get_ports {req dbg_do uio[*] LEDR[*] HEX0[*] HEX1[*] HEX2[*] HEX3[*] HEX4[*] HEX5[*]}]
