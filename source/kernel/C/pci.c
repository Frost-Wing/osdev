/**
 * @file pci.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The PCI Code for the kernel
 * @version 0.1
 * @date 2023-10-23
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <hal.h>
#include <pci.h>

const char* display_adapter_name = "Frost Generic Display Adapter";
const char* GPUName[1] = {"Frost Generic Display Driver for Graphics Processing Unit."}; //Max 2 GPUs allowed

int64* graphics_base_Address = null;

/**
 * @brief Function to read a 32-bit value from the PCI configuration space
 * 
 * @param bus 
 * @param slot 
 * @param func 
 * @param offset 
 * @return int32 
 */
int32 pci_config_read_dword(int8 bus, int8 slot, int8 func, int8 offset) {
    int32 address = (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

/**
 * @brief Function to get the base address register (BAR) of the graphics card
 * 
 * @param bus 
 * @param slot 
 * @param func 
 * @param desiredBAR 
 * @return int64 
 */
int64 getGraphicsCardBAR(int8 bus, int8 slot, int8 func, int8 desiredBAR) {
    for (int8 barIndex = 0; barIndex < 6; ++barIndex) {  // Assume there are 6 BARs, adjust as needed
        int32 lowerBAR = pci_config_read_dword(bus, slot, func, 0x10 + barIndex * 4);

        // Check if the BAR is memory-mapped (bit 0 set to 0) and not an IO space (bit 0 set to 1)
        if ((lowerBAR & 0x1) == 0 && (lowerBAR & 0x6) == 0) {
            // Assuming BAR is memory-mapped, so we concatenate lower and upper halves
            int32 upperBAR = pci_config_read_dword(bus, slot, func, 0x14 + barIndex * 4);
            int64 barAddress = ((int64)upperBAR << 32) | lowerBAR;

            // Check if this is the desired BAR
            if (barIndex == desiredBAR) {
                return barAddress;
            }else {
                warn("Desired BAR not found! (case 2)", __FILE__);
            }
        }
    }

    warn("Desired BAR not found! (case 1)", __FILE__);
    return 0;
}

/**
 * @brief Read a 16-bit value from a PCI configuration register.
 *
 * This function reads a 16-bit value from a specified PCI configuration register
 * using the provided bus, slot, function, and offset parameters.
 *
 * @param bus The PCI bus number.
 * @param slot The PCI slot number.
 * @param func The PCI function number.
 * @param offset The offset within the PCI configuration register to read.
 * @return The 16-bit value read from the specified PCI configuration register.
 */
int16 pci_read_word(int16 bus, int16 slot, int16 func, int16 offset)
{
    int64 address;
    int64 lbus = (int64)bus;
    int64 lslot = (int64)slot;
    int64 lfunc = (int64)func;
    int16 tmp = 0;
    address = (int64)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((int32)0x80000000));
    outl(0xCF8, address);
    tmp = (int16)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
    return (tmp);
}


/**
 * @brief Gets the Vendor ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16 Vendor ID
 */
int16 getVendorID(int16 bus, int16 device, int16 function)
{
        int32 r0 = pci_read_word(bus,device,function,0);
        return r0;
}

/**
 * @brief Gets the Device ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16  Device ID
 */
int16 getDeviceID(int16 bus, int16 device, int16 function)
{
        int32 r0 = pci_read_word(bus,device,function,2);
        return r0;
}

/**
 * @brief Gets the Class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16 Class ID
 */
int16 getClassId(int16 bus, int16 device, int16 function)
{
        int32 r0 = pci_read_word(bus,device,function,0xA);
        return (r0 & ~0x00FF) >> 8;
}

/**
 * @brief Gets the Sub-class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16 Sub-class ID
 */
int16 getSubClassId(int16 bus, int16 device, int16 function)
{
        int32 r0 = pci_read_word(bus,device,function,0xA);
        return (r0 & ~0xFF00);
}

/**
 * @brief Loads a int64* to a Graphics Card's base address
 * 
 * @param bus 
 * @param slot 
 * @param function 
 */
void load_graphics_card(int16 bus, int16 slot, int16 function){

    for(int8 barIndex = 0; barIndex < 6; barIndex++){
        graphics_base_Address = getGraphicsCardBAR(bus, slot, function, barIndex);

        if(graphics_base_Address != null){
            done("Hooray! found graphics card's base address!", __FILE__);
            return;
        }else{
            warn("Cannot use graphics card. (Attempting next BAR Index)", __FILE__);
        }

    }

}

char* vendorNames[512];
char* deviceNames[512];
char* classNames[512];

/**
 * @brief Scans (Probes) PCI Devices
 * 
 */
void probe_pci(){
    info("Probe has been started!", __FILE__);
    int i = 0;
    for(int32 bus = 0; bus < 256; bus++)
    {
        for(int32 slot = 0; slot < 32; slot++)
        {
            for(int32 function = 0; function < 8; function++)
            {
                    int16 vendor = getVendorID(bus, slot, function);
                    if(vendor == 0xffff) continue;
                    int16 device = getDeviceID(bus, slot, function);
                    int16 classid = getClassId(bus, slot, function);
                    int16 subclassid = getSubClassId(bus, slot, function);

                    const char* vendorName;
                    const char* deviceName;
                    const char* className;

                    switch (vendor) {
                        case 0x8086:
                        case 0x8087:
                            vendorName = "Intel Corp.";
                            break;
                        case 0x03e7:
                            vendorName = "Intel";
                            break;
                        case 0x10DE:
                            vendorName = "NVIDIA";
                            break;
                        case 0x1002:
                            vendorName = "AMD";
                            break;
                        case 0x1234:
                            vendorName = "Brain Actuated Technologies";
                            break;
                        case 0x168c:
                            vendorName = "Qualcomm Atheros";
                            break;
                        case 0x10EC:
                            vendorName = "Realtek Semiconductor Co., Ltd.";
                            break;
                        case 0x15ad:
                            vendorName = "VMware";
                            break;
                        case 0x1af4:
                        case 0x1b36:
                            vendorName = "Red Hat, Inc.";
                            break;
                        default:
                            unsigned char str[20];
                            itoa(vendor, str, sizeof(str), 16);
                            vendorName = str;
                    }


                    switch (classid) {
                        case 0x01:
                            className = "Mass Storage Controller";
                            break;
                        case 0x02:
                            className = "Network Controller";
                            break;
                        case 0x03:
                            className = "Display Controller";
                            break;
                        case 0x04:
                            className = "Multimedia Controller";
                            break;
                        case 0x05:
                            className = "Memory Controller";
                            break;
                        case 0x06:
                            className = "Bridge Device";
                            break;
                        case 0x07:
                            className = "Simple Communication Controller";
                            break;
                        case 0x08:
                            className = "Base System Peripheral";
                            break;
                        case 0x09:
                            className = "Input Device";
                            break;
                        case 0x0a:
                            className = "Docking Station";
                            break;
                        case 0x0b:
                            className = "Processor";
                            break;
                        case 0x0c:
                            className = "Serial Bus Controller";
                            break;
                        case 0x0d:
                            className = "Wireless Controller";
                            break;
                        case 0x0e:
                            className = "Intelligent Controller";
                            break;
                        case 0x0f:
                            className = "Satellite Communication Controller";
                            break;
                        case 0x10:
                            className = "Encryption/Decryption Controller";
                            break;
                        case 0x11:
                            className = "Data Acquisition and Signal Processing Controller";
                            break;
                        case 0x12:
                            className = "Processing Accelerator";
                            break;
                        case 0x13:
                            className = "Non-Essential Instrumentation";
                            break;
                        case 0x40:
                            className = "Video Device";
                            break;
                        case 0x0106:
                            className = "SATA controllers";
                            break;
                        case 0x80:
                            className = "Unassigned";
                            break;
                        default:
                            unsigned char str[20];
                            itoa(classid, str, sizeof(str), 16);
                            className = str;
                    }

                    if(subclassid == 0x01 && classid == 0x0106){ // Indeed it is a SATA controller
                        ahci_bar = pci_read_word(bus, slot, 0, 0x10);
                    }


                    if(device == 0x29C0) deviceName = "Express DRAM Controller";
                    else if(device == 0x2918) deviceName = "LPC Interface Controller";
                    else if(device == 0x2922) deviceName = "6 port SATA Controller [AHCI mode]";
                    else if(device == 0x2930) deviceName = "SMBus Controller";
                    else if(vendor == 0x1234 && device == 0x4321) deviceName = "Human Interface Device";
                    else if(vendor == 0x1002 && device == 0x10280810 && classid == 0x03) {display_adapter_name = deviceName = GPUName[1] = "AMD Radeon 530";}
                    else if(vendor == 0x1002 && device == 0x0128079c && classid == 0x03) {display_adapter_name = deviceName = GPUName[1] = "AMD Radeon R7 465X";}
                    else if(vendor == 0x1002 && device == 0x10020124 && classid == 0x03) {display_adapter_name = deviceName = GPUName[1] = "Radeon HD 6470M";}
                    else if(vendor == 0x1002 && device == 0x10020134 && classid == 0x03) {display_adapter_name = deviceName = GPUName[1] = "Radeon HD 6470M";}
                    else if(vendor == 0x1002 && device == 0x6900 && classid == 0x03) {display_adapter_name = deviceName = "Radeon R7 M260/M265 / M340/M360 / M440/M445";}
                    else if(vendor == 0x8086 && device == 0x1606 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics";}
                    else if(vendor == 0x8086 && device == 0x1612 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics 5600";}
                    else if(vendor == 0x8086 && device == 0x1616 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics 5500";}
                    else if(vendor == 0x8086 && device == 0x161e && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics 5300";}
                    else if(vendor == 0x8086 && device == 0x1626 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics 6000";}
                    else if(vendor == 0x8086 && device == 0x1902 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics 510"; }
                    else if(vendor == 0x8086 && device == 0x1906 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics 500"; }
                    else if(vendor == 0x8086 && device == 0x1912 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics 530"; }
                    else if(vendor == 0x8086 && device == 0x1916 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics 520"; }
                    else if(vendor == 0x8086 && device == 0x191b && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics 530"; }
                    else if(vendor == 0x8086 && device == 0x191d && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) HD Graphics P530";}
                    else if(vendor == 0x8086 && device == 0x3e92 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) UHD Graphics 630";}
                    else if(vendor == 0x8086 && device == 0x5917 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) UHD Graphics 620";}
                    else if(vendor == 0x8086 && device == 0x3ea0 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) UHD Graphics 620 ((Whiskey Lake)";}
                    else if(vendor == 0x8086 && device == 0x3e93 && classid == 0x03) {display_adapter_name = deviceName = GPUName[0] = "Intel(R) UHD Graphics 610";}
                    else if(vendor == 0x8086 && device == 70 && classid == 0x03)     {display_adapter_name = deviceName = GPUName[0] = "Intel(R) Graphics";}
                    else if(vendor == 0x1b36) {
                        if(device == 0x100){
                            display_adapter_name = deviceName = GPUName[0] = "QXL Paravirtual Graphics card";
                        }
                    }
                    else if(vendor == 0x1af4){
                        if(device == 0x1050){
                            display_adapter_name = deviceName = GPUName[0] = "Virtio graphics card";
                            load_graphics_card(bus, slot, function);
                        }
                    }
                    else if(device == 0x1237){deviceName = "440FX - 82441FX PMC [Natoma]";}
                    else if(device == 0x7000){deviceName = "82371SB PIIX3 ISA [Natoma/Triton II]";}
                    else if(device == 0x7010){deviceName = "82371SB PIIX3 IDE [Natoma/Triton II]";}
                    else if(device == 0x7113){deviceName = "82371AB/EB/MB PIIX4 ACPI";}
                    else if(device == 0x100e){deviceName = "82540EM Gigabit Ethernet Controller";}
                    else if(device == 0x1111){
                        display_adapter_name = deviceName = GPUName[0] = "Qemu Virtual Graphics";
                        load_graphics_card(bus, slot, function);
                    }
                    else if(vendor == 0x1ff7 && device == 0x001a){deviceName = "Human Interface Device";}
                    else if(vendor == 0x04b4 && device == 0x1006){deviceName = "Human Interface Device";}
                    else if(vendor == 0x04e8 && device == 0x7081){deviceName = "Human Interface Device";}
                    else if(vendor == 5549 && device == 1029){display_adapter_name = deviceName = GPUName[0] = "VMware SVGA Graphics";}
                    else if(vendor == 4115 && device == 184){display_adapter_name = deviceName = GPUName[0] = "Cirrus Graphics";}
                    else if(classid == 0x03 && (vendor != 0x1b36 && device != 0x100)){
                        load_graphics_card(bus, slot, function);
                    }
                    else {
                        unsigned char str[20];
                        itoa(device, str, sizeof(str), 16);
                        deviceName = str;
                    }

                    if(vendor == 0x10EC){
                        if(device == 0x8139){ // Indeed it is an RTL8139 Card
                            RTL8139->io_base = (int16)(pci_read_word(bus, slot, 0, RTL8139_IOADDR1) & 0xFFFC);
                            deviceName = "RTL8139 Networking Card";
                        }
                    }

                    if(vendor == 0x10DE)
                    {
                        if(device == 0x0040)      {display_adapter_name = deviceName = GPUName[1] =  "NV40 [GeForce 6800 Ultra]";}
                        else if(device == 0x0041) {display_adapter_name = deviceName = GPUName[1] =  "NV40 [GeForce 6800]";   }
                        else if(device == 0x0042) {display_adapter_name = deviceName = GPUName[1] =  "NV40 [GeForce 6800 LE]";}
                        else if(device == 0x0043) {display_adapter_name = deviceName = GPUName[1] =  "NV40 [GeForce 6800 XE]";}
                        else if(device == 0x0044) {display_adapter_name = deviceName = GPUName[1] =  "NV40 [GeForce 6800 XT]";}
                        else if(device == 0x0045) {display_adapter_name = deviceName = GPUName[1] =  "NV40 [GeForce 6800 GT]";}
                        else if(device == 0x0046) {display_adapter_name = deviceName = GPUName[1] =  "NV40 [GeForce 6800 GS]";}
                        else if(device == 0x0090) {display_adapter_name = deviceName = GPUName[1] =  "G70 [GeForce 7800 GTX]";}
                        else if(device == 0x0091) {display_adapter_name = deviceName = GPUName[1] =  "G70 [GeForce 7800 GTX]";}
                        else if(device == 0x0092) {display_adapter_name = deviceName = GPUName[1] =  "G70 [GeForce 7800 GT]";}
                        else if(device == 0x0093) {display_adapter_name = deviceName = GPUName[1] =  "G70 [GeForce 7800 GS]";}
                        else if(device == 0x0094) {display_adapter_name = deviceName = GPUName[1] =  "G70 [GeForce 7800 SLI]";}
                        else if(device == 0x1b83) {display_adapter_name = deviceName = GPUName[1] =  "GP104 [GeForce GTX 1060 6GB]";} // My fav GPU for this OS
                        else if(device == 0x1b84) {display_adapter_name = deviceName = GPUName[1] =  "GP104 [GeForce GTX 1060 3GB]";}
                        else {display_adapter_name = "Frost Generic Display Adapter"; return 0;}
                    }

                    print(green_color);
                    printf("%s : %n%t Device : %s%n%t Class  : %s", vendorName, deviceName, className);
                    print(reset_color);

                    vendorNames[i] = vendorName;
                    deviceNames[i] = deviceName;
                    classNames[i] = className;
                    vendorName = null;
                    deviceName = null;
                    className = null;
                    i++;
            }
        }
    }
    done("Successfully completed probe!", __FILE__);
    done("Successfully saved to a array! and verified.", __FILE__);
    print("Graphics Card (GPU0) :");
    print(GPUName[0]);
    print("\nGraphics Card (GPU1) :");
    print(GPUName[1]);
    print("\nDisplay Adapter      :");
    print(display_adapter_name);
    print("\n");
}