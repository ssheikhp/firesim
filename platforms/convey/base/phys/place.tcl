if {[file exists synth_cs.dcp]} {
  open_checkpoint synth_cs.dcp
} else {
  open_checkpoint synth.dcp
}
if {[file exists debug.xdc]} {
  source debug.xdc
  write_debug_probes debug_nets.ltx
}
read_xdc /opt/convey/pdk2/2015.02.11-3047/wx-2000/phys/pins_normc.xdc
read_xdc place.xdc
read_xdc place_fix.xdc
read_xdc /opt/convey/pdk2/2015.02.11-3047/wx-2000/phys/place_timing.xdc
read_xdc /opt/convey/pdk2/2015.02.11-3047/wx-2000/phys/async_throttling.xdc
opt_design -verbose -directive NoBramPowerOpt
#power_opt_design
place_design 
phys_opt_design 
write_checkpoint -force place
report_clock_utilization -force -file place_clock_util.rpt
report_utilization -force -file place_util.rpt
#report_timing -sort_by group -max_paths 5 \
#  -path_type summary -file place_timing.rpt
