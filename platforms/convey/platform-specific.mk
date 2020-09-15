# See LICENSE for license details.

# FireSim Host-Specific Makefrag interface. Provides the following recipes:
# 1) replace-rtl: for creating a fresh FPGA project directory
# 2) fpga: generates a bitstream for this host platform
# Additionally, provides recipes for doing complete host RTL simulation
# (FPGA-level) with a full model of the host FPGA.

# Compulsory variables follow
# The directory into which generated verilog and headers will be dumped
GENERATED_DIR ?=
# Root name for generated binaries
DESIGN ?=
# A string generated from the config names to uniquely indentify the simulator
name_tuple ?=


fpga_dir = $(firesim_base_dir)/../platforms/convey
#############################
# FPGA Build Initialization #
#############################
fpga_work_dir  := $(GENERATED_DIR)/convey
fpga_v         := $(fpga_work_dir)/verilog/FPGATop.v
repo_state     := $(fpga_work_dir)/repo_state

$(fpga_work_dir)/stamp: $(shell find $(fpga_dir)/cl_firesim -name '*')
	mkdir -p $(@D)
	cp -rf $(fpga_dir)/base -T $(fpga_work_dir)
	#sed -i "s/wolverine_firesim_name/$(name_tuple)/" $(fpga_work_dir)/Makefile.include
	touch $@


$(fpga_v): $(VERILOG) $(fpga_work_dir)/stamp
	$(firesim_base_dir)/../scripts/repo_state_summary.sh > $(repo_state)
	cp -f $< $@
	sed -i "s/\$$random/64'b0/g" $@
	sed -i "s/\(^ *\)fatal;\( *$$\)/\1fatal(0, \"\");\2/g" $@

# Goes as far as setting up the build directory without running the cad job
# Used by the manager before passing a build to a remote machine
replace-rtl: $(fpga_v)

.PHONY: replace-rtl

# Runs a local fpga-bitstream build.
fpga: $(fpga_v)
	make -C $(fpga_work_dir)/phys pers

.PHONY: fpga
