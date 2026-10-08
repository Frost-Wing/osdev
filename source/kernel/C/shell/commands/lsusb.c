/**
 * @file lsusb.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Linux similar implementation of lsusb for kernel
 * @version 0.1
 * @date 2026-10-08
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#include <commands/commands.h>
#include <usb.h>

/* column widths */
#define COL_BUS  5
#define COL_PORT 6
#define COL_SLOT 6
#define COL_ID   11

static void cell(const char *s, int width) {
    printfnoln("%s", s);
    for (int i = (int)strlen(s); i < width; i++)
        printfnoln(" ");
}

int cmd_lsusb(int argc, char **argv) {
    (void)argc;
    (void)argv;

    cell("Bus", COL_BUS);
    cell("Port", COL_PORT);
    cell("Slot", COL_SLOT);
    cell("ID", COL_ID);
    printf("Class");

    for (size_t i = 0; i < usb_device_count(); ++i) {
        usb_device_t *device = usb_get_device(i);
        if (!device)
            continue;

        char port[8], slot[8], id[16];
        snprintf(port, sizeof(port), "%hu", (uint32_t)device->root_port);
        snprintf(slot, sizeof(slot), "%hu", (uint32_t)device->slot_id);
        snprintf(id, sizeof(id), "%04hx:%04hx",
                 (uint32_t)device->descriptor.vendor_id,
                 (uint32_t)device->descriptor.product_id);

        cell("001", COL_BUS);
        cell(port, COL_PORT);
        cell(slot, COL_SLOT);
        cell(id, COL_ID);

        if (device->interface_count == 0)
            printfnoln("-");

        for (uint8_t j = 0; j < device->interface_count; ++j) {
            printfnoln("0x%02hx", (uint32_t)device->interfaces[j].interface_class);
            if (j + 1U < device->interface_count)
                printfnoln(",");
        }
        printfnoln("\n");
    }

    return 0;
}