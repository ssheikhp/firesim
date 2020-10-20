// See LICENSE for license details.

package midas.widgets

import junctions._

import chisel3._
import chisel3.util.{Decoupled, Counter, log2Up}
import freechips.rocketchip.config.Parameters
import freechips.rocketchip.diplomacy._

class BridgeMonitorIO(implicit val p: Parameters) extends WidgetIO()(p){
  val fromHostValid = Input(UInt(32.W))
  val fromHostReady = Input(UInt(32.W))
  val toHostReady = Input(UInt(32.W))
  val toHostValid = Input(UInt(32.W))
}

class BridgeMonitor(implicit p: Parameters) extends Widget()(p) {
  lazy val module = new WidgetImp(this) {
    val io = IO(new BridgeMonitorIO)
    genROReg(io.fromHostValid, "FROM_HOST_VALID")
    genROReg(io.fromHostReady, "FROM_HOST_READY")
    genROReg(io.toHostReady, "TO_HOST_READY")
    genROReg(io.toHostValid, "TO_HOST_VALID")
    val target_ready = io.toHostValid & io.fromHostReady
    val host_ready = io.fromHostValid & io.toHostReady
    val blame_host = target_ready & (~host_ready)
    genROReg(blame_host, "BLAME_HOST")


    genCRFile()
  }
}
