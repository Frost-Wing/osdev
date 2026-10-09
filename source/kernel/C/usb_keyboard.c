/**
 * @file usb_keyboard.c
 * @brief USB HID boot-keyboard support.
 */
#include <graphics.h>
#include <keyboard.h>
#include <pit.h>
#include <usb.h>
#include <usb_keyboard.h>

#define USB_HID_CLASS 3U
#define USB_HID_BOOT_SUBCLASS 1U
#define USB_HID_KEYBOARD_PROTOCOL 1U
#define USB_HID_SET_IDLE 10U
#define USB_HID_SET_PROTOCOL 11U
#define USB_HID_KEYBOARD_REPORT_SIZE 8U
#define USB_HID_KEYBOARD_COUNT 16U
#define USB_HID_POLL_TICKS 1U

typedef struct {
    usb_device_t *device;
    uint8_t interface_number;
    uint8_t endpoint_address;
    uint8_t endpoint_id;
    uint8_t report[USB_HID_KEYBOARD_REPORT_SIZE] __attribute__((aligned(8)));
    uint8_t previous_report[USB_HID_KEYBOARD_REPORT_SIZE];
    uint64_t last_poll;
    uint8_t report_error_logged;
} usb_hid_keyboard_t;

static usb_hid_keyboard_t keyboards[USB_HID_KEYBOARD_COUNT];
static void process_report(usb_hid_keyboard_t *keyboard,
    const uint8_t *report);

static int hid_keyboard_find(usb_device_t *device,
    const usb_interface_t *interface) {
    for (uint8_t i = 0; i < USB_HID_KEYBOARD_COUNT; ++i) {
        if (keyboards[i].device == device &&
            keyboards[i].interface_number == interface->number)
            return i;
    }
    return -1;
}

static void usb_hid_keyboard_callback(usb_device_t *device,
    const usb_interface_t *interface, bool connected) {
    if (!device || !interface)
        return;

    int current = hid_keyboard_find(device, interface);
    if (!connected) {
        if (current >= 0) {
            uint8_t released[USB_HID_KEYBOARD_REPORT_SIZE] = {0};
            process_report(&keyboards[current], released);
            keyboards[current] = (usb_hid_keyboard_t){0};
        }
        return;
    }

    if (current >= 0)
        return;
    uint8_t index = 0;
    while (index < USB_HID_KEYBOARD_COUNT && keyboards[index].device)
        ++index;
    if (index == USB_HID_KEYBOARD_COUNT) {
        warn("USB boot-keyboard limit reached", __FILE__);
        return;
    }

    const usb_endpoint_t *interrupt_in = NULL;
    for (uint8_t i = 0; i < device->endpoint_count; ++i) {
        const usb_endpoint_t *endpoint = &device->endpoints[i];
        if (endpoint->interface_number == interface->number &&
            (endpoint->attributes & 3U) == 3U &&
            (endpoint->address & 0x80U) &&
            endpoint->max_packet_size >= USB_HID_KEYBOARD_REPORT_SIZE) {
            interrupt_in = endpoint;
            break;
        }
    }
    if (!interrupt_in) {
        warn("USB keyboard interface %u has no suitable interrupt-IN endpoint",
            __FILE__, interface->number);
        return;
    }

    if (usb_control_request(device, 0x21U, USB_HID_SET_PROTOCOL, 0,
            interface->number, NULL, 0) != 0) {
        warn("Unable to select boot protocol for USB keyboard interface %u",
            __FILE__, interface->number);
        return;
    }
    if (usb_control_request(device, 0x21U, USB_HID_SET_IDLE, 0x0100U,
            interface->number, NULL, 0) != 0) {
        warn("Unable to set idle rate for USB keyboard interface %u",
            __FILE__, interface->number);
    }

    keyboards[index].device = device;
    keyboards[index].interface_number = interface->number;
    keyboards[index].endpoint_address = interrupt_in->address;
    keyboards[index].endpoint_id = interrupt_in->endpoint_id;
    keyboards[index].last_poll = pit_ticks - USB_HID_POLL_TICKS;
    info("USB HID boot keyboard connected on interface %u", __FILE__,
        interface->number);
}

int usb_keyboard_init(void) {
    return usb_register_class_driver(USB_HID_CLASS, USB_HID_BOOT_SUBCLASS,
        USB_HID_KEYBOARD_PROTOCOL, usb_hid_keyboard_callback);
}

static int hid_usage_scancode(uint8_t usage, uint8_t *scancode,
    uint8_t *extended) {
    static const uint8_t letters[26] = {
        0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24,
        0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14,
        0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
    };
    static const uint8_t digits[10] = {
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B
    };
    *extended = 0;

    if (usage >= 0x04U && usage <= 0x1DU) {
        *scancode = letters[usage - 0x04U];
        return 1;
    }
    if (usage >= 0x1EU && usage <= 0x27U) {
        *scancode = digits[usage - 0x1EU];
        return 1;
    }

    switch (usage) {
        case 0x28: *scancode = 0x1C; return 1;
        case 0x29: *scancode = 0x01; return 1;
        case 0x2A: *scancode = 0x0E; return 1;
        case 0x2B: *scancode = 0x0F; return 1;
        case 0x2C: *scancode = 0x39; return 1;
        case 0x2D: *scancode = 0x0C; return 1;
        case 0x2E: *scancode = 0x0D; return 1;
        case 0x2F: *scancode = 0x1A; return 1;
        case 0x30: *scancode = 0x1B; return 1;
        case 0x31: *scancode = 0x2B; return 1;
        case 0x33: *scancode = 0x27; return 1;
        case 0x34: *scancode = 0x28; return 1;
        case 0x35: *scancode = 0x29; return 1;
        case 0x36: *scancode = 0x33; return 1;
        case 0x37: *scancode = 0x34; return 1;
        case 0x38: *scancode = 0x35; return 1;
        case 0x39: *scancode = 0x3A; return 1;
        case 0x3A: *scancode = 0x3B; return 1;
        case 0x3B: *scancode = 0x3C; return 1;
        case 0x3C: *scancode = 0x3D; return 1;
        case 0x3D: *scancode = 0x3E; return 1;
        case 0x3E: *scancode = 0x3F; return 1;
        case 0x3F: *scancode = 0x40; return 1;
        case 0x40: *scancode = 0x41; return 1;
        case 0x41: *scancode = 0x42; return 1;
        case 0x42: *scancode = 0x43; return 1;
        case 0x43: *scancode = 0x44; return 1;
        case 0x44: *scancode = 0x57; return 1;
        case 0x45: *scancode = 0x58; return 1;
        case 0x4A: *scancode = 0x47; *extended = 1; return 1;
        case 0x4B: *scancode = 0x49; *extended = 1; return 1;
        case 0x4C: *scancode = 0x53; *extended = 1; return 1;
        case 0x4D: *scancode = 0x4F; *extended = 1; return 1;
        case 0x4E: *scancode = 0x51; *extended = 1; return 1;
        case 0x4F: *scancode = 0x4D; *extended = 1; return 1;
        case 0x50: *scancode = 0x4B; *extended = 1; return 1;
        case 0x51: *scancode = 0x50; *extended = 1; return 1;
        case 0x52: *scancode = 0x48; *extended = 1; return 1;
        default: return 0;
    }
}

static int report_has_usage(const uint8_t *report, uint8_t usage) {
    for (uint8_t i = 2; i < USB_HID_KEYBOARD_REPORT_SIZE; ++i) {
        if (report[i] == usage)
            return 1;
    }
    return 0;
}

static void emit_usage(uint8_t usage, bool released) {
    uint8_t scancode, extended;
    if (!hid_usage_scancode(usage, &scancode, &extended))
        return;
    if (extended)
        keyboard_process_scancode(0xE0U);
    keyboard_process_scancode((uint8_t)(scancode |
        (released ? 0x80U : 0U)));
}

static void emit_modifier(uint8_t scancode, bool extended,
    bool released) {
    if (extended)
        keyboard_process_scancode(0xE0U);
    keyboard_process_scancode((uint8_t)(scancode |
        (released ? 0x80U : 0U)));
}

static void process_report(usb_hid_keyboard_t *keyboard,
    const uint8_t *report) {
    uint8_t rollover = 0;
    for (uint8_t i = 2; i < USB_HID_KEYBOARD_REPORT_SIZE; ++i) {
        if (report[i] == 1U)
            rollover = 1;
    }

    static const uint8_t modifier_scancodes[8] = {
        0x1D, 0x2A, 0x38, 0, 0x1D, 0x36, 0x38, 0
    };
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t mask = (uint8_t)(1U << i);
        if ((keyboard->previous_report[0] & mask) &&
            !(report[0] & mask) && modifier_scancodes[i])
            emit_modifier(modifier_scancodes[i], i == 4U || i == 6U,
                true);
    }
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t mask = (uint8_t)(1U << i);
        if (!(keyboard->previous_report[0] & mask) &&
            (report[0] & mask) && modifier_scancodes[i])
            emit_modifier(modifier_scancodes[i], i == 4U || i == 6U,
                false);
    }

    if (rollover) {
        keyboard->previous_report[0] = report[0];
        return;
    }

    for (uint8_t i = 2; i < USB_HID_KEYBOARD_REPORT_SIZE; ++i) {
        uint8_t usage = keyboard->previous_report[i];
        if (usage && !report_has_usage(report, usage))
            emit_usage(usage, true);
    }
    for (uint8_t i = 2; i < USB_HID_KEYBOARD_REPORT_SIZE; ++i) {
        uint8_t usage = report[i];
        if (usage && !report_has_usage(keyboard->previous_report, usage))
            emit_usage(usage, false);
    }
    memcpy(keyboard->previous_report, report, USB_HID_KEYBOARD_REPORT_SIZE);
}

void usb_keyboard_poll(void) {
    for (uint8_t i = 0; i < USB_HID_KEYBOARD_COUNT; ++i) {
        usb_hid_keyboard_t *keyboard = &keyboards[i];
        if (!keyboard->device ||
            pit_ticks - keyboard->last_poll < USB_HID_POLL_TICKS)
            continue;
        keyboard->last_poll = pit_ticks;
        uint32_t actual = 0;
        int result = usb_interrupt_poll(keyboard->device,
            keyboard->endpoint_address, keyboard->endpoint_id,
            keyboard->report, sizeof(keyboard->report), &actual);
        if (result < 0) {
            if (!keyboard->report_error_logged) {
                warn("Unable to read USB keyboard interrupt report on interface %u",
                    __FILE__, keyboard->interface_number);
                keyboard->report_error_logged = 1;
            }
            continue;
        }
        keyboard->report_error_logged = 0;
        if (result > 0)
            continue;
        if (actual == 0)
            continue;
        if (actual < USB_HID_KEYBOARD_REPORT_SIZE) {
            warn("Short USB keyboard report on interface %u (%u bytes)",
                __FILE__, keyboard->interface_number, actual);
            continue;
        }
        process_report(keyboard, keyboard->report);
    }
}
