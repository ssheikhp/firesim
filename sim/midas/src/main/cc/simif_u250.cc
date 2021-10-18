#include "simif_u250.h"
#include <cassert>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define PCI_DEV_FMT "%04x:%02x:%02x.%d"
int bar0_size = 0x2000000; // 32 MB

static int
fpga_pci_check_file_id(char *path, uint16_t id)
{
    assert(path);
    int ret = 0;
    FILE *fp = fopen(path, "r");
    assert(fp);
    uint32_t tmp_id;
    ret = fscanf(fp, "%x", &tmp_id);
    assert(ret >= 0);
    assert(tmp_id == id);
    fclose(fp);
    return 0;
}

simif_u250_t::simif_u250_t(int argc, char** argv) {
#ifdef SIMULATION_XSIM
    mkfifo(driver_to_xsim, 0666);
    fprintf(stderr, "opening driver to xsim\n");
    driver_to_xsim_fd = open(driver_to_xsim, O_WRONLY);
    fprintf(stderr, "opening xsim to driver\n");
    xsim_to_driver_fd = open(xsim_to_driver, O_RDONLY);
#else
    slot_id = -1;
    xdma_id = 0;
    std::vector<std::string> args(argv + 1, argv + argc);
    for (auto &arg: args) {
        if (arg.find("+slotid=") == 0) {
            slot_id = strtol((arg.c_str()) + 8, NULL, 16);
        } else if (arg.find("+xdmaid=") == 0) {
            xdma_id = strtol((arg.c_str()) + 8, NULL, 10);
        }
    }
    if (slot_id == -1) {
        fprintf(stderr, "Slot ID not specified. Assuming Slot 0\n");
        slot_id = 0;
    }
    fpga_setup(slot_id, xdma_id);
#endif
}

void simif_u250_t::check_rc(int rc, char * infostr) {
#ifndef SIMULATION_XSIM
    if (rc) {
        if (infostr) {
            fprintf(stderr, "%s\n", infostr);
        }
        fprintf(stderr, "INVALID RETCODE: %d\n", rc, infostr);
        fpga_shutdown();
        exit(1);
    }
#endif
}

void simif_u250_t::fpga_shutdown() {
#ifndef SIMULATION_XSIM
    int ret = munmap(bar0_base, bar0_size);
    assert(ret == 0);
    close(edma_write_fd);
    close(edma_read_fd);
#endif
}

void simif_u250_t::fpga_setup(int slot_id, int xdma_id) {
#ifndef SIMULATION_XSIM
    int domain = 0;
    int device_id = 0;
    int pf_id = 0;
    int bar_id = 0;


    uint16_t pci_vendor_id = 0x10ee; /* Xilinx PCI Vendor ID */
    uint16_t pci_device_id = 0x903f; /* PCI Device ID preassigned by Xilinx for XDMA */

    int fd = -1;
    char sysfs_name[256];
    int ret;

    // check vendor id
    ret = snprintf(sysfs_name, sizeof(sysfs_name),
			"/sys/bus/pci/devices/" PCI_DEV_FMT "/vendor",
			domain, slot_id, device_id, pf_id);
    assert(ret>=0);
    fpga_pci_check_file_id(sysfs_name, pci_vendor_id);

    // check device id
    ret = snprintf(sysfs_name, sizeof(sysfs_name),
			"/sys/bus/pci/devices/" PCI_DEV_FMT "/device",
			domain, slot_id, device_id, pf_id);
    assert(ret>=0);
    fpga_pci_check_file_id(sysfs_name, pci_device_id);

    // open and memory map
    snprintf(sysfs_name, sizeof(sysfs_name),
             "/sys/bus/pci/devices/" PCI_DEV_FMT "/resource%u",
             domain, slot_id, device_id, pf_id, bar_id);

    fd = open(sysfs_name, O_RDWR | O_SYNC);
    assert(fd!=-1);

    bar0_base = mmap(0, bar0_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(bar0_base != MAP_FAILED);
    close(fd);
    fd = -1;

    // EDMA setup
    char device_file_name[256];
    char device_file_name2[256];

    sprintf(device_file_name, "/dev/xdma%d_h2c_0", xdma_id);
    printf("Using xdma write queue: %s\n", device_file_name);
    sprintf(device_file_name2, "/dev/xdma%d_c2h_0", xdma_id);
    printf("Using xdma read queue: %s\n", device_file_name2);


    edma_write_fd = open(device_file_name, O_WRONLY);
    edma_read_fd = open(device_file_name2, O_RDONLY);
    assert(edma_write_fd >= 0);
    assert(edma_read_fd >= 0);
#endif
}



simif_u250_t::~simif_u250_t() {
    fpga_shutdown();
}

void * simif_u250_t::fpga_pci_bar_get_mem_at_offset(uint64_t offset){
    assert(!(((uint64_t)(offset + 4)) > bar0_size));
    return bar0_base + offset;
}

int simif_u250_t::fpga_pci_poke(uint64_t offset, uint32_t value) {
    uint32_t *reg_ptr = (uint32_t *)fpga_pci_bar_get_mem_at_offset(offset);
    *reg_ptr = value;
	return 0;
}

int simif_u250_t::fpga_pci_peek(uint64_t offset, uint32_t *value) {
    uint32_t *reg_ptr = (uint32_t *)fpga_pci_bar_get_mem_at_offset(offset);
    *value = *reg_ptr;
	return 0;
}

void simif_u250_t::write(size_t addr, uint32_t data) {
    // addr is really a (32-byte) word address because of zynq implementation
    addr <<= CTRL_AXI4_SIZE;
#ifdef SIMULATION_XSIM
    uint64_t cmd = (((uint64_t)(0x80000000 | addr)) << 32) | (uint64_t)data;
    char * buf = (char*)&cmd;
    ::write(driver_to_xsim_fd, buf, 8);
#else
    int rc = fpga_pci_poke(addr, data);
    check_rc(rc, NULL);
#endif
}

uint32_t simif_u250_t::read(size_t addr) {
    // Convert the word address into a byte-address
    addr <<= CTRL_AXI4_SIZE;
#ifdef SIMULATION_XSIM
    uint64_t cmd = addr;
    char * buf = (char*)&cmd;
    ::write(driver_to_xsim_fd, buf, 8);

    int gotdata = 0;
    while (gotdata == 0) {
        gotdata = ::read(xsim_to_driver_fd, buf, 8);
        if (gotdata != 0 && gotdata != 8) {
            printf("ERR GOTDATA %d\n", gotdata);
        }
    }
    return *((uint64_t*)buf);
#else
    uint32_t value;
    int rc = fpga_pci_peek(addr, &value);
    return value & 0xFFFFFFFF;
#endif
}

ssize_t simif_u250_t::pull(size_t addr, char* data, size_t size) {
#ifdef SIMULATION_XSIM
  return -1; // TODO
#else
  return ::pread(edma_read_fd, data, size, addr);
#endif
}

ssize_t simif_u250_t::push(size_t addr, char* data, size_t size) {
#ifdef SIMULATION_XSIM
  return -1; // TODO
#else
  return ::pwrite(edma_write_fd, data, size, addr);
#endif
}

uint32_t simif_u250_t::is_write_ready() {
    uint64_t addr = 0x4;
#ifdef SIMULATION_XSIM
    uint64_t cmd = addr;
    char * buf = (char*)&cmd;
    ::write(driver_to_xsim_fd, buf, 8);

    int gotdata = 0;
    while (gotdata == 0) {
        gotdata = ::read(xsim_to_driver_fd, buf, 8);
        if (gotdata != 0 && gotdata != 8) {
            printf("ERR GOTDATA %d\n", gotdata);
        }
    }
    return *((uint64_t*)buf);
#else
    uint32_t value;
    int rc = fpga_pci_peek(addr, &value);
    check_rc(rc, NULL);
    return value & 0xFFFFFFFF;
#endif
}
