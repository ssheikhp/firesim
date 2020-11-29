//See LICENSE for license details
package firesim.bridges

import chisel3._
import chisel3.util._
import chisel3.util.experimental.BoringUtils
import freechips.rocketchip.config.{Parameters, Field}
import freechips.rocketchip.diplomacy.AddressSet
import freechips.rocketchip.util._
import freechips.rocketchip.rocket.TracedInstruction
import freechips.rocketchip.subsystem.RocketTilesKey
import freechips.rocketchip.tile.TileKey

import testchipip.{TileTraceIO, DeclockedTracedInstruction, TracedInstructionWidths}

import midas.targetutils.TriggerSource
import midas.widgets._
import testchipip.{StreamIO, StreamChannel}
import junctions.{NastiIO, NastiKey}
import TokenQueueConsts._



class TracerSTargetIO(insnWidths: TracedInstructionWidths, numInsns: Int) extends Bundle {
  val trace = Input(new TileTraceIO(insnWidths, numInsns))
}

// Warning: If you're not going to use the companion object to instantiate this
// bridge you must call generate trigger annotations _in the parent module_.
//
// TODO: Generalize a mechanism to promote annotations from extracted bridges...
class TracerSBridge(insnWidths: TracedInstructionWidths, numInsns: Int) extends BlackBox
    with Bridge[HostPortIO[TracerSTargetIO], TracerSBridgeModule] {
  val io = IO(new TracerSTargetIO(insnWidths, numInsns))
  val bridgeIO = HostPort(io)
  val constructorArg = Some(TracerVKey(insnWidths, numInsns))
  generateAnnotations()
}

object TracerSBridge {
  def apply(tracedInsns: TileTraceIO)(implicit p:Parameters): TracerSBridge = {
    val ep = Module(new TracerSBridge(tracedInsns.insnWidths, tracedInsns.numInsns))
    ep.io.trace := tracedInsns
    ep
  }
}

class TracerSBridgeModule(key: TracerVKey)(implicit p: Parameters) extends BridgeModule[HostPortIO[TracerSTargetIO]]()(p) {
  lazy val module = new BridgeModuleImp(this){
    val io = IO(new WidgetIO)
    val hPort = IO(HostPort(new TracerSTargetIO(key.insnWidths, key.vecSize)))

    // Mask off valid committed instructions when under reset
    val traces = hPort.hBits.trace.insns.map({ unmasked =>
      val masked = WireDefault(unmasked)
      masked.valid := unmasked.valid && !hPort.hBits.trace.reset.asBool
      masked
    })
    private val pcWidth = traces.map(_.iaddr.getWidth).max
    private val insnWidth = traces.map(_.insn.getWidth).max
    val cycleCountWidth = 64

    // Set after trigger-dependent memory-mapped registers have been set, to
    // prevent spurious credits
    val initDone    = genWORegInit(Wire(Bool()), "initDone", false.B)

    val traceActive    = genWORegInit(Wire(Bool()), "traceActive", false.B)

    val sampleStep    = genWORegInit(Wire(UInt(32.W)), "sampleStep", 10000.U)
    val nextSample = RegInit(100.U(cycleCountWidth.W))
    attach(nextSample(cycleCountWidth-1, 32), "nextSample_upper", ReadOnly)
    attach(nextSample(31, 0), "nextSample_lower", ReadOnly)

    val insnWidths = traces.head.widths
    val numInsns = traces.length
    val lastInstruction = Reg(new DeclockedTracedInstruction(insnWidths))
    val lastInstructionCycle = RegInit(0.U(cycleCountWidth.W))

    // block on next valid instruction
    val blockOnNextValid = RegInit(true.B)
    val block = RegInit(false.B)
    attach(block, "block", ReadOnly)

    val insnRead = Wire(Decoupled(UInt(32.W)))
    attachDecoupledSource(insnRead, "lastInstruction")
    insnRead.valid := true.B // don't use lastInstruction.valid to avoid deadlocking
    insnRead.bits := lastInstruction.iaddr | lastInstruction.valid
    when(insnRead.fire){
      lastInstruction.valid := false.B
      when(!lastInstruction.valid){
        blockOnNextValid := true.B
      }
      // unblock
      block := false.B
    }
    attach(lastInstruction.valid, "li_valid", ReadOnly)
    attach(lastInstruction.iaddr(pcWidth-1,32), "li_iaddr_upper", ReadOnly)
    attach(lastInstruction.iaddr(31,0), "li_iaddr_lower", ReadOnly)
    attach(lastInstruction.insn, "li_insn", ReadOnly)
    lastInstruction.wdata.map(w => attach(w, "li_wdata", ReadOnly))
    attach(lastInstruction.priv, "li_priv", ReadOnly)
    attach(lastInstruction.exception, "li_exception", ReadOnly)
    attach(lastInstruction.interrupt, "li_interrupt", ReadOnly)
    attach(lastInstruction.cause, "li_cause", ReadOnly)
    attach(lastInstruction.tval, "li_tval", ReadOnly)
    attach(lastInstructionCycle(cycleCountWidth-1, 32), "li_cycle_upper", ReadOnly)
    attach(lastInstructionCycle(31, 0), "li_cycle_lower", ReadOnly)

    val tFireHelper = DecoupledHelper(!(block && traceActive), hPort.toHost.hValid, hPort.fromHost.hReady, initDone)

    hPort.toHost.hReady := tFireHelper.fire(hPort.toHost.hValid)
    hPort.fromHost.hValid := tFireHelper.fire(hPort.fromHost.hReady)

    val trace_cycle_counter = RegInit(0.U(cycleCountWidth.W))
    attach(trace_cycle_counter(cycleCountWidth-1, 32), "trace_cycle_upper", ReadOnly)
    attach(trace_cycle_counter(31, 0), "trace_cycle_lower", ReadOnly)

    when (tFireHelper.fire) {
      val trace_cycle_next = trace_cycle_counter + 1.U
      trace_cycle_counter := trace_cycle_next
      when(trace_cycle_next >= nextSample && traceActive){
        block := true.B
        nextSample := nextSample + sampleStep
      }
      // only write to register if valid
      when(traces.map(_.valid).reduce(_ || _) && traceActive){
        // select last (newest) valid instruction
        lastInstruction := PriorityMux(traces.reverse.map(a => a.valid -> a))
        lastInstructionCycle := trace_cycle_counter
        when(blockOnNextValid){
          blockOnNextValid := false.B
          block := true.B
        }
      }
    }

    genCRFile()
    override def genHeader(base: BigInt, sb: StringBuilder) {
      import CppGenerationUtils._
      val headerWidgetName = getWName.toUpperCase
      super.genHeader(base, sb)
      sb.append(genConstStatic(s"${headerWidgetName}_max_core_ipc", UInt32(traces.size)))
      emitClockDomainInfo(headerWidgetName, sb)
    }
  }
}
