create_project -force firesim ./vivado_proj -part xcu250-figd2104-2L-e
set_property board_part xilinx.com:au250:part0:1.3 [current_project]
add_files {./verilog/axi_tieoff_master.v ./verilog/firesim_wrapper.v ./verilog/FPGATop.v}
source ./tcl/cl_firesim_generated_env.tcl
source ./tcl/create_bd.tcl
make_wrapper -files [get_files ./vivado_proj/firesim.srcs/sources_1/bd/design_1/design_1.bd] -top
add_files -norecurse ./vivado_proj/firesim.gen/sources_1/bd/design_1/hdl/design_1_wrapper.v
update_compile_order -fileset sources_1
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1
launch_runs synth_1 -jobs 6
wait_on_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 6
wait_on_run impl_1