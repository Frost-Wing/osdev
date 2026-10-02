#include <commands/commands.h>
#include <xhci.h>

int cmd_lsusb(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("Bus Port Slot ID              Class");
    for (size_t i = 0; i < usb_device_count(); ++i) {
        usb_device_t *device = usb_get_device(i);
        if (!device)
            continue;
        printfnoln("001 %4u %4u %04x:%04x",
            device->root_port, device->slot_id,
            device->descriptor.vendor_id, device->descriptor.product_id);
        for (uint8_t j = 0; j < device->interface_count; ++j) {
            printfnoln(" 0x%02x", device->interfaces[j].interface_class);
            if (j + 1U < device->interface_count)
                printfnoln(",");
        }
        printfnoln("\n");
    }
    return 0;
}
