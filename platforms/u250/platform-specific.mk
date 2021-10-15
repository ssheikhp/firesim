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

fpga_dir = $(firesim_base_dir)/../platforms/u250
#############################
# FPGA Build Initialization #
#############################
fpga_work_dir  := $(GENERATED_DIR)/u250
fpga_v         := $(fpga_work_dir)/verilog/FPGATop.v
repo_state     := $(fpga_work_dir)/repo_state
fpga_tcl_env   := $(fpga_work_dir)/tcl/cl_firesim_generated_env.tcl

$(fpga_work_dir)/stamp: $(shell find $(fpga_dir)/cl_firesim -name '*')
	mkdir -p $(@D)
	cp -rf $(fpga_dir)/base -T $(fpga_work_dir)
	touch $@


$(fpga_v): $(VERILOG) $(fpga_work_dir)/stamp
	$(firesim_base_dir)/../scripts/repo_state_summary.sh > $(repo_state)
	cp -f $< $@
	sed -i "s/\$$random/64'b0/g" $@
	sed -i "s/\(^ *\)fatal;\( *$$\)/\1fatal(0, \"\");\2/g" $@

$(fpga_tcl_env): $(VERILOG) $(fpga_work_dir)/stamp
	cp -f $(GENERATED_DIR)/$(@F) $@

# Goes as far as setting up the build directory without running the cad job
# Used by the manager before passing a build to a remote machine
replace-rtl: $(fpga_v) $(fpga_tcl_env)

.PHONY: replace-rtl

# Runs a local fpga-bitstream build.
fpga: replace-rtl
	cd $(fpga_work_dir) && vivado -mode batch -source ./tcl/generate_bitstream.tcl

.PHONY: fpga

############################
# Master Simulation Driver #
############################

$(u250-driver): export CXXFLAGS := $(CXXFLAGS) $(common_cxx_flags) $(DRIVER_CXXOPTS)
$(u250-driver): export LDFLAGS := $(LDFLAGS) $(common_ld_flags)

# Compile Driver
$(u250-driver): $(HEADER) $(DRIVER_CC) $(DRIVER_H) $(midas_cc) $(midas_h) $(runtime_conf)
	mkdir -p $(OUTPUT_DIR)/build
	cp $(HEADER) $(OUTPUT_DIR)/build/
	cp -f $(GENERATED_DIR)/$(CONF_NAME) $(OUTPUT_DIR)/runtime.conf
	$(MAKE) -C $(simif_dir) $(PLATFORM) PLATFORM=$(PLATFORM) DESIGN=$(DESIGN) \
	GEN_DIR=$(OUTPUT_DIR)/build OUT_DIR=$(OUTPUT_DIR) DRIVER="$(DRIVER_CC)" \
	TOP_DIR=$(chipyard_dir)