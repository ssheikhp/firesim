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

    require(key.vecSize == 1)
    val lastInstruction = Reg(new DeclockedTracedInstruction(traces.head.widths))

    val insnRead = Wire(Decoupled(UInt(32.W)))
    attachDecoupledSource(insnRead, "lastInstruction")
    insnRead.valid := true.B // don't use lastInstruction.valid to avoid deadlocking
    insnRead.bits := lastInstruction.iaddr | lastInstruction.valid
    when(insnRead.fire){
      lastInstruction.valid := false.B
    }
    attach(lastInstruction.valid, "li_valid", ReadOnly)
    attach(lastInstruction.iaddr, "li_iaddr", ReadOnly)
    attach(lastInstruction.insn, "li_insn", ReadOnly)
    lastInstruction.wdata.map(w => attach(w, "li_wdata", ReadOnly))
    attach(lastInstruction.priv, "li_priv", ReadOnly)
    attach(lastInstruction.exception, "li_exception", ReadOnly)
    attach(lastInstruction.interrupt, "li_interrupt", ReadOnly)
    attach(lastInstruction.cause, "li_cause", ReadOnly)
    attach(lastInstruction.tval, "li_tval", ReadOnly)

    val tFireHelper = DecoupledHelper((!lastInstruction.valid || !traceActive), hPort.toHost.hValid, hPort.fromHost.hReady, initDone)

    hPort.toHost.hReady := tFireHelper.fire(hPort.toHost.hValid)
    hPort.fromHost.hValid := tFireHelper.fire(hPort.fromHost.hReady)

    val trace_cycle_counter = RegInit(0.U(cycleCountWidth.W))
    when (tFireHelper.fire) {
      trace_cycle_counter := trace_cycle_counter + 1.U
      when(traces.head.valid){
        lastInstruction := traces.head
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
