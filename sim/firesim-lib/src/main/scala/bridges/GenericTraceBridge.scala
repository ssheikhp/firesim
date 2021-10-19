//See LICENSE for license details
package firesim.bridges

import chisel3._
import chisel3.util._
import freechips.rocketchip.config.Parameters
import midas.widgets._
import testchipip.TileGenericTraceIO

object GenericTraceConsts {
  val TOKEN_QUEUE_DEPTH = 6144
}

case class GenericTraceKey(bitWidth: Int)

class GenericTraceTargetIO(val bitWidth : Int) extends Bundle {
  val tracevTrigger = Input(Bool())
  val trace = Input(new TileGenericTraceIO(bitWidth))
}

class GenericTraceBridge(val bitWidth: Int) extends BlackBox
    with Bridge[HostPortIO[GenericTraceTargetIO], GenericTraceBridgeModule] {
  val io = IO(new GenericTraceTargetIO(bitWidth))
  val bridgeIO = HostPort(io)
  val constructorArg = Some(GenericTraceKey(bitWidth))
  generateAnnotations()
}

object GenericTraceBridge {
  def apply(generic_trace: TileGenericTraceIO)(implicit p:Parameters): GenericTraceBridge = {
    val ep = Module(new GenericTraceBridge(generic_trace.bitWidth))
    // withClockAndReset(generic_trace.clock, generic_trace.reset) { TriggerSink(ep.io.tracevTrigger, noSourceDefault = false.B) }
    ep.io.trace := generic_trace
    ep
  }
}

class GenericTraceBridgeModule(key: GenericTraceKey)(implicit p: Parameters) extends BridgeModule[HostPortIO[GenericTraceTargetIO]]()(p) {
  lazy val module = new BridgeModuleImp(this) with UnidirectionalDMAToHostCPU {
    val io = IO(new WidgetIO)
    val hPort = IO(HostPort(new GenericTraceTargetIO(key.bitWidth)))

    // Set after trigger-dependent memory-mapped registers have been set, to
    // prevent spurious credits
    val initDone    = genWORegInit(Wire(Bool()), "initDone", false.B)
    val traceEnable    = genWORegInit(Wire(Bool()), "traceEnable", false.B)

    // Trigger Selector
    val triggerSelector = RegInit(0.U((p(CtrlNastiKey).dataBits).W))
    attach(triggerSelector, "triggerSelector", WriteOnly)

    // Mask off ready samples when under reset
    val trace = hPort.hBits.trace.data
    val traceValid = trace.valid && !hPort.hBits.trace.reset.asBool()
    val triggerTraceV = hPort.hBits.tracevTrigger

    // Connect trigger
    val trigger = MuxLookup(triggerSelector, false.B, Seq(
      0.U -> true.B,
      1.U -> triggerTraceV
    ))

    val traceOut = initDone && traceEnable && traceValid && trigger


    // Width of the trace vector
    val traceWidth = trace.bits.getWidth
    // Width of one token as defined by the DMA
    val discreteDmaWidth = dmaBytes * 8
    // How many tokens we need to trace out the bit vector, at least one for DMA sanity
    val tokensPerTrace = math.max((traceWidth + discreteDmaWidth - 1) / discreteDmaWidth, 1)

    // Bridge DMA Parameters
    lazy val toHostCPUQueueDepth = GenericTraceConsts.TOKEN_QUEUE_DEPTH
    lazy val dmaSize = BigInt((discreteDmaWidth / 8) * GenericTraceConsts.TOKEN_QUEUE_DEPTH)

    // TODO: the commented out changes below show how multi-token transfers would work
    // However they show a bad performance for yet unknown reasons in terms of FPGA synth
    // timings -- verilator shows expected results
    // for now we limit us to 512 bits with an assert.

    assert(tokensPerTrace == 1)

    // State machine that controls which token we are sending and whether we are finished
    // val tokenCounter = new Counter(tokensPerTrace)
    // val readyNextTrace = WireInit(true.B)
    // when (outgoingPCISdat.io.enq.fire()) {
    //  readyNextTrace := tokenCounter.inc()
    // }

    println( "GenericTraceBridgeModule")
    println(s"    traceWidth      ${traceWidth}")
    println(s"    dmaTokenWidth   ${discreteDmaWidth}")
    println(s"    requiredTokens  {")
    for (i <- 0 until tokensPerTrace)  {
      val from = ((i + 1) * discreteDmaWidth) - 1
      val to   = i * discreteDmaWidth
      println(s"        ${i} -> traceBits(${from}, ${to})")
    }
    println( "    }")
    println( "")

    // val paddedTrace = trace.bits.asUInt().pad(tokensPerTrace * discreteDmaWidth)
    // val paddedTraceSeq = for (i <- 0 until tokensPerTrace) yield {
    //   i.U -> paddedTrace(((i + 1) * discreteDmaWidth) - 1, i * discreteDmaWidth)
    // }

    // outgoingPCISdat.io.enq.valid := hPort.toHost.hValid && traceOut
    // outgoingPCISdat.io.enq.bits := MuxLookup(tokenCounter.value , 0.U, paddedTraceSeq)

    // hPort.toHost.hReady := initDone && outgoingPCISdat.io.enq.ready && readyNextTrace

    outgoingPCISdat.io.enq.valid := hPort.toHost.hValid && traceOut
    outgoingPCISdat.io.enq.bits := trace.bits.asUInt().pad(discreteDmaWidth)

    hPort.toHost.hReady := initDone && outgoingPCISdat.io.enq.ready
    hPort.fromHost.hValid := true.B

    attach(outgoingPCISdat.io.deq.valid && !outgoingPCISdat.io.enq.ready, "generictracequeuefull", ReadOnly)

    genCRFile()
    override def genHeader(base: BigInt, sb: StringBuilder) {
      import CppGenerationUtils._
      val headerWidgetName = getWName.toUpperCase
      super.genHeader(base, sb)
      sb.append(genConstStatic(s"${headerWidgetName}_queue_depth", UInt32(GenericTraceConsts.TOKEN_QUEUE_DEPTH)))
      sb.append(genConstStatic(s"${headerWidgetName}_token_width", UInt32(discreteDmaWidth)))
      sb.append(genConstStatic(s"${headerWidgetName}_trace_width", UInt32(traceWidth)))
      emitClockDomainInfo(headerWidgetName, sb)
    }
  }
}
