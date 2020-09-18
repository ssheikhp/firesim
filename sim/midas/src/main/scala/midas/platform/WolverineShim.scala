// See LICENSE for license details.

package midas
package platform

import chisel3._
import chisel3.util._
import junctions._
import freechips.rocketchip.config.{Parameters}
import freechips.rocketchip.diplomacy.{LazyModule, LazyModuleImp}

import midas.widgets.CtrlNastiKey

// response bundle for return read data or write completes (?)
class ConveyMemResponse(val rtnCtlBits: Int, val dataBits: Int) extends Bundle {
  val rtnCtl = UInt(rtnCtlBits.W)
  val readData = UInt(dataBits.W)
  val cmd = UInt(3.W)
  val scmd = UInt(4.W)

  //  override def clone = {
  //    new ConveyMemResponse(rtnCtlBits, dataBits).asInstanceOf[this.type] }
}

// wrapper for Convey Verilog personality
class TopWrapperInterface(val numMemPorts: Int, val rtnctl: Int) extends Bundle {
  // dispatch interface
  val dispInstValid = Input(Bool())
  val dispInstData = Input(UInt(5.W))
  val dispRegID = Input(UInt(18.W))
  val dispRegRead = Input(Bool())
  val dispRegWrite = Input(Bool())
  val dispRegWrData = Input(UInt(64.W))
  val dispAegCnt = Output(UInt(18.W))
  val dispException = Output(UInt(16.W))
  val dispIdle = Output(Bool())
  val dispRtnValid = Output(Bool())
  val dispRtnData = Output(UInt(64.W))
  val dispStall = Output(Bool())
  // memory controller interface
  // request
  val mcReqValid = Output(UInt((numMemPorts).W))
  val mcReqRtnCtl = Output(UInt((rtnctl * numMemPorts).W))
  val mcReqData = Output(UInt((64 * numMemPorts).W))
  val mcReqAddr = Output(UInt((48 * numMemPorts).W))
  val mcReqSize = Output(UInt((2 * numMemPorts).W))
  val mcReqCmd = Output(UInt((3 * numMemPorts).W))
  val mcReqSCmd = Output(UInt((4 * numMemPorts).W))
  val mcReqStall = Input(UInt((numMemPorts).W))
  // response
  val mcResValid = Input(UInt((numMemPorts).W))
  val mcResCmd = Input(UInt((3 * numMemPorts).W))
  val mcResSCmd = Input(UInt((4 * numMemPorts).W))
  val mcResData = Input(UInt((64 * numMemPorts).W))
  val mcResRtnCtl = Input(UInt((rtnctl * numMemPorts).W))
  val mcResStall = Output(UInt((numMemPorts).W))
  // flush
  val mcReqFlush = Output(UInt((numMemPorts).W))
  val mcResFlushOK = Input(UInt((numMemPorts).W))
  // control-status register interface
  val csrWrValid = Input(Bool())
  val csrRdValid = Input(Bool())
  val csrAddr = Input(UInt(16.W))
  val csrWrData = Input(UInt(64.W))
  val csrReadAck = Output(Bool())
  val csrReadData = Output(UInt(64.W))
  // misc
  val aeid = Input(UInt(4.W))


  // rename signals
  def renameSignals() {
    dispInstValid.suggestName("disp_inst_vld")
    dispInstData.suggestName("disp_inst")
    dispRegID.suggestName("disp_aeg_idx")
    dispRegRead.suggestName("disp_aeg_rd")
    dispRegWrite.suggestName("disp_aeg_wr")
    dispRegWrData.suggestName("disp_aeg_wr_data")
    dispAegCnt.suggestName("disp_aeg_cnt")
    dispException.suggestName("disp_exception")
    dispIdle.suggestName("disp_idle")
    dispRtnValid.suggestName("disp_rtn_data_vld")
    dispRtnData.suggestName("disp_rtn_data")
    dispStall.suggestName("disp_stall")
    mcReqValid.suggestName("mc_rq_vld")
    mcReqRtnCtl.suggestName("mc_rq_rtnctl")
    mcReqData.suggestName("mc_rq_data")
    mcReqAddr.suggestName("mc_rq_vadr")
    mcReqSize.suggestName("mc_rq_size")
    mcReqCmd.suggestName("mc_rq_cmd")
    mcReqSCmd.suggestName("mc_rq_scmd")
    mcReqStall.suggestName("mc_rq_stall")
    mcResValid.suggestName("mc_rs_vld")
    mcResCmd.suggestName("mc_rs_cmd")
    mcResSCmd.suggestName("mc_rs_scmd")
    mcResData.suggestName("mc_rs_data")
    mcResRtnCtl.suggestName("mc_rs_rtnctl")
    mcResStall.suggestName("mc_rs_stall")
    mcReqFlush.suggestName("mc_rq_flush")
    mcResFlushOK.suggestName("mc_rs_flush_cmplt")
    csrWrValid.suggestName("csr_wr_vld")
    csrRdValid.suggestName("csr_rd_vld")
    csrAddr.suggestName("csr_address")
    csrWrData.suggestName("csr_wr_data")
    csrReadAck.suggestName("csr_rd_ack")
    csrReadData.suggestName("csr_rd_data")
    aeid.suggestName("i_aeid")
  }
}

class WolverineShim(implicit p: Parameters) extends PlatformShim {
  lazy val module = new LazyModuleImp(this) {
    val numMemPorts: Int = 1
    val idBits: Int = 32

    val io = IO(new TopWrapperInterface(numMemPorts, idBits))
    // registers
    val regCnt = 8
    val regs = RegInit(VecInit(Seq.fill(regCnt)(0.U(64.W))))

    val stall_reg = regs(0)

    val dispReadAddr = RegEnable(io.dispRegID, io.dispRegRead)
    io.dispRtnValid := RegNext(io.dispRegRead)
    io.dispRtnData := regs(dispReadAddr)
    when(io.dispRegWrite) {
      regs(io.dispRegID) := io.dispRegWrData
    }
    // csr
    val csrReadAddr = RegEnable(io.csrAddr, io.csrRdValid)
    io.csrReadData := regs(csrReadAddr)
    io.csrReadAck := RegNext(io.csrRdValid)
    when(io.csrWrValid && io.csrAddr < regCnt.U) {
      regs(io.csrAddr) := io.csrWrData
    }
    // TODO: react to dispatch
    io.dispIdle := !stall_reg.orR()
    io.dispStall := io.dispInstValid || stall_reg.orR()
    when(io.dispInstValid) {
      stall_reg := 1.U
    }
    io.dispException := Cat(
      (io.dispRegWrite || io.dispRegRead) && (io.dispRegID >= regCnt.U),
      io.dispInstValid && io.dispInstData =/= 0.U
    )
    io.dispAegCnt := regCnt.U

    //    val p: Parameters = new With1GbRam ++ new RocketZynqConfig ++ Parameters.empty
    val memAxiIdBits = p(midas.core.HostMemChannelKey).idBits

    val adapt_axi = top.module.ctrl

    val sIDLE :: sREAD :: sWRITE :: sWRITE_DATA :: sREAD_DATA :: sWRITE_RESPONSE :: Nil = Enum(6)

    val eOK :: eBURST :: eLEN :: eSIZE :: eQUEUE_READY :: eQUEUE_CMD :: eMASK :: Nil = Enum(7)

    val mem_axi_error_reg = RegInit(eOK)
    val mem_axi_error_next = WireInit(mem_axi_error_reg)
    mem_axi_error_reg := mem_axi_error_next
    //TODO: allocate memory
    val mem_alloc_addr = regs(1)

    val mem_ram_base: BigInt = 0
    val mem_ram_size: BigInt = p(midas.core.HostMemChannelKey).size
    val mem_write_addr = RegInit(0.U)
    val mem_write_id = RegInit(0.U)
    val mem_write_echo = RegInit(0.U)
    val mem_write_len = RegInit(0.U)
    val mem_request_state = RegInit(sIDLE)

    val adapt_addr = RegInit(0.U)
    val adapt_wData = RegInit(0.U)
    val adapt_rData = RegInit(0.U)
    val adapt_state = RegInit(sIDLE)

    when(io.csrReadAck && csrReadAddr === regCnt.U) {
      io.csrReadData := adapt_rData
    }
    when(io.csrReadAck && csrReadAddr === (regCnt + 1).U) {
      io.csrReadData := adapt_addr
    }
    when(io.csrReadAck && csrReadAddr === (regCnt + 2).U) {
      io.csrReadData := adapt_wData
    }
    when(io.csrReadAck && csrReadAddr === (regCnt + 3).U) {
      io.csrReadData := adapt_rData
    }
    when(io.csrReadAck && csrReadAddr === (regCnt + 4).U) {
      io.csrReadData := adapt_state
    }
    when(io.csrReadAck && csrReadAddr === (regCnt + 5).U) {
      io.csrReadData := mem_axi_error_reg
    }

    // this is quite hacky and only works because csr operations only happen every ~100 cycles at 50MHz
    when(io.csrAddr === regCnt.U && io.csrWrValid) {
      val write = io.csrWrData(63)
      adapt_addr := io.csrWrData(62, 32)
      assert(adapt_state === sIDLE, "csr-axi operation still ongoing")
      when(write) {
        adapt_state := sWRITE
        adapt_wData := io.csrWrData(31, 0)
      }.otherwise {
        adapt_state := sREAD
      }
    }

    //read add
    adapt_axi.ar.bits.id := 0.U
    adapt_axi.ar.bits.addr := adapt_addr
    adapt_axi.ar.bits.len := 0.U // 1 transfer
    adapt_axi.ar.bits.size := "b010".U // 4 bytes per transfer
    adapt_axi.ar.bits.burst := "b00".U // FIXED
    adapt_axi.ar.bits.lock := 0.U // no lock?
    adapt_axi.ar.bits.cache := 0.U // no buffer?
    adapt_axi.ar.bits.prot := 0.U // unpriviledged, secure, data?
    adapt_axi.ar.bits.qos := 0.U // no qos
    //write addr
    adapt_axi.aw.bits.id := 0.U
    adapt_axi.aw.bits.addr := adapt_addr
    adapt_axi.aw.bits.len := 0.U // 1 transfer
    adapt_axi.aw.bits.size := "b010".U // 4 bytes per transfer
    adapt_axi.aw.bits.burst := "b00".U // FIXED
    adapt_axi.aw.bits.lock := 0.U // no lock?
    adapt_axi.aw.bits.cache := 0.U // no buffer?
    adapt_axi.aw.bits.prot := 0.U // unpriviledged, secure, data?
    adapt_axi.aw.bits.qos := 0.U // no qos
    //write data
    adapt_axi.w.bits.data := adapt_wData
    adapt_axi.w.bits.strb := ((1 << 4) - 1).U
    adapt_axi.w.bits.last := true.B

    //send/receive nothing
    adapt_axi.ar.valid := false.B
    adapt_axi.aw.valid := false.B
    adapt_axi.w.valid := false.B
    adapt_axi.r.ready := false.B
    adapt_axi.b.ready := false.B

    when(adapt_state === sREAD) {
      adapt_axi.ar.valid := true.B
      when(adapt_axi.ar.fire()) {
        adapt_state := sREAD_DATA
      }
    }.elsewhen(adapt_state === sWRITE) {
      adapt_axi.aw.valid := true.B
      adapt_axi.w.valid := true.B

      when(adapt_axi.aw.fire() && adapt_axi.w.fire()) {
        adapt_state := sWRITE_RESPONSE
      }.elsewhen(adapt_axi.aw.fire()) {
        adapt_state := sWRITE_DATA
      }
    }.elsewhen(adapt_state === sWRITE_DATA) {
      adapt_axi.w.valid := true.B
      when(adapt_axi.w.fire()) {
        adapt_state := sWRITE_RESPONSE
      }
    }.elsewhen(adapt_state === sREAD_DATA) {
      adapt_axi.r.ready := true.B
      when(adapt_axi.r.fire()) {
        adapt_rData := adapt_axi.r.bits.data
        adapt_state := sIDLE
      }
    }.elsewhen(adapt_state === sWRITE_RESPONSE) {
      adapt_axi.b.ready := true.B
      when(adapt_axi.b.fire()) {
        adapt_state := sIDLE
      }
    }

    // mc
    io.mcReqValid := false.B
    io.mcReqCmd := 0.U
    io.mcReqSCmd := 0.U
    io.mcReqSize := 0.U
    io.mcReqAddr := 0.U
    io.mcReqRtnCtl := 0.U
    io.mcReqData := 0.U
    io.mcReqFlush := false.B

    io.mcResStall := false.B

    Some(top.module.mem(0)).map(master_axi => {
      dontTouch(master_axi)

      //    val edge = top.test.memAXI4Node.in.head._2
      //    val simplm = LazyModule(new AXISimplifier(edge))
      //    val simp = Module(simplm.module)
      //    val mem_axi = simplm.io_mem_axi.head
      //    simplm.io_master_axi.head <> master_axi
      //    dontTouch(mem_axi)

      val mem_axi = master_axi


      // supports only single beat 1-8byte transactions for now
      // ensure correct values
      when(mem_axi.ar.valid) {
        when(mem_axi.ar.bits.burst =/= "b01".U /*INC*/) {
          mem_axi_error_next := eBURST
        }
        // reads of different sizes are allowed
      }
      assert(mem_axi_error_next === eOK)

      when(mem_axi.aw.valid) {
        when(mem_axi.aw.bits.burst =/= "b01".U /*INC*/) {
          mem_axi_error_next := eBURST
        }.elsewhen(mem_axi.aw.bits.size =/= "b011".U /*8 bytes*/) {
          mem_axi_error_next := eSIZE
        }.elsewhen(mem_axi.aw.bits.len > 7.U /*8 beats*/) {
          mem_axi_error_next := eLEN
        }
      }

      // closed by default
      mem_axi.tieoff()

      val can_request = !RegNext(io.mcReqStall(0))

      // handles ar, aw and w
      when(mem_request_state === sIDLE) {
        when(mem_axi.ar.valid) {
          mem_request_state := sREAD
        }.elsewhen(mem_axi.aw.valid) {
          mem_request_state := sWRITE
        }
      }.elsewhen(mem_request_state === sREAD) {
        mem_axi.ar.ready := can_request
        when(mem_axi.ar.fire()) {
          // just always perform a quadword read
          mem_request_state := sIDLE
          io.mcReqValid := true.B
          when(mem_axi.ar.bits.len === 0.U){
            io.mcReqCmd := 1.U // quadword read
            io.mcReqSCmd := 0.U
          }.elsewhen(mem_axi.ar.bits.len === 7.U){
            io.mcReqCmd := 7.U // multi-quadword read
            io.mcReqSCmd := 0.U
            // always 8 beats
          }.otherwise{
            mem_axi_error_next := eLEN
          }
          io.mcReqSize := 3.U
          io.mcReqAddr := mem_alloc_addr + Cat(mem_axi.ar.bits.addr(31, 3), 0.U(3.W)) - mem_ram_base.U
          //        io.mcReqRtnCtl := Cat(mem_axi.ar.bits.echo(AXI4FragLast), mem_axi.ar.bits.id)
          io.mcReqRtnCtl := mem_axi.ar.bits.id
        }
      }.elsewhen(mem_request_state === sWRITE) {
        mem_axi.aw.ready := can_request
        when(mem_axi.aw.fire()) {
          mem_request_state := sWRITE_DATA
          mem_write_addr := mem_axi.aw.bits.addr
          mem_write_id := mem_axi.aw.bits.id
          //        mem_write_echo := mem_axi.aw.bits.echo(AXI4FragLast)
          mem_write_len := mem_axi.aw.bits.len
        }
      }.elsewhen(mem_request_state === sWRITE_DATA) {
        mem_axi.w.ready := can_request
        when(mem_axi.w.fire()) {
          io.mcReqValid := true.B
          when(mem_write_len === 0.U){
            io.mcReqCmd := 2.U // write
            io.mcReqSCmd := 0.U // 1 beat
            when(BitPat("b11111111") === mem_axi.w.bits.strb){
              io.mcReqSize := 3.U // 8 bytes
              io.mcReqData := mem_axi.w.bits.data
              io.mcReqAddr := mem_alloc_addr + mem_write_addr - mem_ram_base.U
            }.elsewhen(BitPat("b00001111") === mem_axi.w.bits.strb){
              io.mcReqSize := 2.U // 4 bytes
              io.mcReqData := mem_axi.w.bits.data(31,0)
              io.mcReqAddr := mem_alloc_addr + mem_write_addr - mem_ram_base.U
            }.elsewhen(BitPat("b11110000") === mem_axi.w.bits.strb){
              io.mcReqSize := 2.U // 4 bytes
              io.mcReqData := mem_axi.w.bits.data(63,32)
              io.mcReqAddr := mem_alloc_addr + mem_write_addr - mem_ram_base.U + 4.U
            }.otherwise{
              mem_axi_error_next := eMASK
              io.mcReqValid := false.B
            }
          }.otherwise{
            io.mcReqCmd := 6.U // write multi
            // from https://github.com/maltanar/fpga-tidbits/blob/master/src/main/scala/fpgatidbits/platform-wrapper/convey/WolverinePlatformWrapper.scala
            // AEMC WR64 - sub cmd
            // Sub-command[2:0] indicates WR64 length, i.e.
            // sub-command  | write size
            //     0	    64 bytes
            //     7	    56 bytes
            //     6	    48 bytes
            //     5	    40 bytes
            //     4	    32 bytes
            //     3	    24 bytes
            //     2	    16 bytes
            //     1	     8 bytes
            io.mcReqSCmd := mem_write_len + 1.U
            when(mem_write_len === 7.U){
              io.mcReqSCmd := 0.U
            }
            io.mcReqSize := 3.U // 8 bytes
            io.mcReqData := mem_axi.w.bits.data
            io.mcReqAddr := mem_alloc_addr + mem_write_addr - mem_ram_base.U
            //increment write addr for each beat
            mem_write_addr := mem_write_addr + 8.U
            when(BitPat("b11111111") =/= mem_axi.w.bits.strb){
              mem_axi_error_next := eMASK
              io.mcReqValid := false.B
            }

          }
          //        io.mcReqRtnCtl := Cat(mem_write_echo, mem_write_id)
          io.mcReqRtnCtl := mem_write_id
          when(mem_axi.w.bits.last) {
            mem_request_state := sIDLE
          }
        }
      }
      // handles r and b

      //response fifo
      val response_queue = Module(new Queue(new ConveyMemResponse(rtnCtlBits = memAxiIdBits + 1, 64), 16, false, true))
      response_queue.io.enq.valid := io.mcResValid(0)
      io.mcResStall := response_queue.io.count >= 8.U
      // ensure that queue logic works
      when(response_queue.io.enq.valid && !response_queue.io.enq.ready) {
        mem_axi_error_next := eQUEUE_READY
      }
      response_queue.io.enq.bits.cmd := io.mcResCmd
      response_queue.io.enq.bits.scmd := io.mcResSCmd
      response_queue.io.enq.bits.rtnCtl := io.mcResRtnCtl
      response_queue.io.enq.bits.readData := io.mcResData

      response_queue.io.deq.ready := false.B

      mem_axi.b.bits.id := 0.U
      mem_axi.b.bits.resp := 0.U
      mem_axi.r.bits.id := 0.U
      mem_axi.r.bits.last := 0.U
      mem_axi.r.bits.resp := 0.U
      mem_axi.r.bits.data := 0.U
      //    mem_axi.r.bits.echo(AXI4FragLast) := false.B
      //    mem_axi.b.bits.echo(AXI4FragLast) := false.B

      when(response_queue.io.deq.valid) {
        when(response_queue.io.deq.bits.cmd === 3.U /*WR_CMP*/) {
          mem_axi.b.valid := true.B
          mem_axi.b.bits.id := response_queue.io.deq.bits.rtnCtl(memAxiIdBits - 1, 0)
          //        mem_axi.b.bits.echo(AXI4FragLast) := response_queue.io.deq.bits.rtnCtl(memAxiIdBits, memAxiIdBits)
          mem_axi.b.bits.resp := "b00".U //OK
          when(mem_axi.b.fire()) {
            response_queue.io.deq.ready := true.B
          }
        }.elsewhen(response_queue.io.deq.bits.cmd === 2.U /*RD_DATA*/) {
          mem_axi.r.valid := true.B
          mem_axi.r.bits.id := response_queue.io.deq.bits.rtnCtl(memAxiIdBits - 1, 0)
          //        mem_axi.r.bits.echo(AXI4FragLast) := response_queue.io.deq.bits.rtnCtl(memAxiIdBits, memAxiIdBits)
          mem_axi.r.bits.last := true.B // only single beat reads
          mem_axi.r.bits.resp := "b00".U //OK
          mem_axi.r.bits.data := response_queue.io.deq.bits.readData
          when(mem_axi.r.fire()) {
            response_queue.io.deq.ready := true.B
          }
        }.elsewhen(response_queue.io.deq.bits.cmd === 7.U /*RD64_DATA*/) {
          mem_axi.r.valid := true.B
          mem_axi.r.bits.id := response_queue.io.deq.bits.rtnCtl(memAxiIdBits - 1, 0)
          //        mem_axi.r.bits.echo(AXI4FragLast) := response_queue.io.deq.bits.rtnCtl(memAxiIdBits, memAxiIdBits)
          mem_axi.r.bits.last := response_queue.io.deq.bits.scmd === 7.U // final beat
          mem_axi.r.bits.resp := "b00".U //OK
          mem_axi.r.bits.data := response_queue.io.deq.bits.readData
          when(mem_axi.r.fire()) {
            response_queue.io.deq.ready := true.B
          }
        }.otherwise {
          mem_axi_error_next := eQUEUE_CMD
        }
      }
    })

    io.renameSignals()
  }
}
