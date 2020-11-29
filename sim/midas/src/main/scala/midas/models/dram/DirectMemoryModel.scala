// See LICENSE for license details.
package midas.models.dram

import firrtl.annotations.HasSerializationHints
import midas.OutputDir
import midas.models._

// From RC
import freechips.rocketchip.config.{Parameters, Field}
import freechips.rocketchip.util.{DecoupledHelper}
import freechips.rocketchip.diplomacy.{IdRange, LazyModule, AddressSet, TransferSizes}
import freechips.rocketchip.amba.axi4._
import junctions._

import chisel3._
import chisel3.util._

import midas.core._
import midas.widgets._
import midas.passes.{Fame1ChiselAnnotation}

import scala.math.min
import Console.{UNDERLINED, RESET}

import java.io.{File, FileWriter}

class DirectMemoryModel(completeConfig: CompleteConfig, hostParams: Parameters) extends BridgeModule[HostPortIO[FASEDTargetIO]]()(hostParams)
    with UsesHostDRAM {

  val cfg = completeConfig.userProvided

  // Reconstitute the parameters object
  implicit override val p = hostParams.alterPartial({
    case NastiKey => completeConfig.axi4Widths
    case FasedAXI4Edge => completeConfig.axi4Edge
  })

  // Begin: Implementation of UsesHostDRAM
  val memoryMasterNode = AXI4MasterNode(
    Seq(AXI4MasterPortParameters(
      masters = Seq(AXI4MasterParameters(
        name = "fased-memory-timing-model",
        id   = IdRange(0, 1 << p(NastiKey).idBits),
        maxFlight = Some(math.max(cfg.maxReadsPerID, cfg.maxWritesPerID))
      )))))

  val memorySlaveConstraints = MemorySlaveConstraints(cfg.targetAddressSpace, cfg.targetRTransfer, cfg.targetWTransfer)
  val memoryRegionName = completeConfig.memoryRegionName.getOrElse(getWName)
  // End: Implementation of UsesHostDRAM

  lazy val module = new BridgeModuleImp(this) {
    val io = IO(new WidgetIO)
    val hPort = IO(HostPort(new FASEDTargetIO))
    val toHostDRAM: AXI4Bundle = memoryMasterNode.out.head._1
    val tNasti = hPort.hBits.axi4
    val tReset = hPort.hBits.reset

    // decoupled helper fire currently doesn't support directly passing true/false.B as exclude
    val tFireHelper = DecoupledHelper(hPort.toHost.hValid,
      hPort.fromHost.hReady)

    val targetFire = tFireHelper.fire

    AXI4NastiAssigner.toAXI4(toHostDRAM, tNasti)
    // stuff can only happen on fire
    toHostDRAM.ar.valid := tNasti.ar.valid && targetFire
    toHostDRAM.aw.valid := tNasti.aw.valid && targetFire
    toHostDRAM.w.valid := tNasti.w.valid && targetFire
    toHostDRAM.r.ready := tNasti.r.ready && targetFire
    toHostDRAM.b.ready := tNasti.b.ready && targetFire

    // HACK: Feeding valid back on ready and ready back on valid until we figure out
    // channel tokenization
    hPort.toHost.hReady := tFireHelper.fire
    hPort.fromHost.hValid := tFireHelper.fire

    genCRFile()
    dontTouch(targetFire)

    override def genHeader(base: BigInt, sb: StringBuilder) {
      def genCPPmap(mapName: String, map: Map[String, BigInt]): String = {
        val prefix = s"const std::map<std::string, int> $mapName = {\n"
        map.foldLeft(prefix)((str, kvp) => str + s""" {\"${kvp._1}\", ${kvp._2}},\n""") + "};\n"
      }
      import midas.widgets.CppGenerationUtils._
      super.genHeader(base, sb)
      sb.append(CppGenerationUtils.genMacro(s"${getWName.toUpperCase}_target_addr_bits", UInt32(p(NastiKey).addrBits)))
    }
  }
}

class DirectMemoryBridge(argument: CompleteConfig)(implicit p: Parameters)
    extends BlackBox with Bridge[HostPortIO[FASEDTargetIO], DirectMemoryModel] {
  val io = IO(new FASEDTargetIO)
  val bridgeIO = HostPort(io)
  val constructorArg = Some(argument)
  generateAnnotations()
}

object DirectMemoryBridge {
  def apply(clock: Clock, axi4: AXI4Bundle, reset: Bool, cfg: CompleteConfig)(implicit p: Parameters): DirectMemoryBridge = {
    val ep = Module(new DirectMemoryBridge(cfg)(p.alterPartial({ case NastiKey => cfg.axi4Widths })))
    ep.io.reset := reset
    ep.io.clock := clock
    // HACK: Nasti and Diplomatic have diverged to the point where it's no longer
    // safe to emit a partial connect leaf fields.
    AXI4NastiAssigner.toNasti(ep.io.axi4, axi4)
    //import chisel3.ExplicitCompileOptions.NotStrict
    //ep.io.axi4 <> axi4
    ep
  }
}
