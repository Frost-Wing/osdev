/**
 * @file ps2-mouse.c
 * @author Poncho (AbsurdPoncho) & Pradosh (pradoshgame@gmail.com)
 * @brief The main source code for PS/2 Mouse input
 * @version 0.1
 * @date 2023-12-10
 * 
 */
#include <ps2-mouse.h>

extern uint64 fb_width;
extern uint64 fb_height;

static void handle_ps2_mouse(uint8 data);
static void process_mouse_packet(void);
static void handle_click(uint8_t type, ivec2 position);

static void ps2_mouse_wait(void){
    uint64 timeout = 100000;
    while (timeout--){
        if ((inb(0x64) & 0x02U) == 0){
            return;
        }
    }
}

static void ps2_mouse_wait_input(void){
    uint64 timeout = 100000;
    while (timeout--){
        if (inb(0x64) & 0x01U){
            return;
        }
    }
}

static void ps2_mouse_write(uint8 value){
    ps2_mouse_wait();
    outb(0x64, 0xD4);
    ps2_mouse_wait();
    outb(0x60, value);
}

static uint8 ps2_mouse_read(void){
    ps2_mouse_wait_input();
    return inb(0x60);
}

uint8 mouse_cycle = 0;
uint8_t mouse_packet[4];
bool isMousePacketReady = false;
ivec2 current_mouse_position;
ivec2 previous_mouse_position;
MouseButtonHandler mButtonHandler = NULL;
MouseMovementHandler mMovementHandler = NULL;
uint8_t mLastMouseButtonState, mMouseButtonState;

const bool mouse_cursor[] = {
    true, false, false, false, false, false, false, false,
    true, true, false, false, false, false, false, false,
    true, true, true, false, false, false, false, false,
    true, true, true, true, false, false, false, false,
    true, true, true, true, true, false, false, false,
    true, true, true, true, true, true, false, false,
    true, true, true, true, true, true, true, false,
    true, true, true, true, true, true, true, true,
    true, true, true, true, true, true, false, false,
    true, true, true, true, true, false, false, false,
    true, true, true, false, false, false, false, false,
    true, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false
};

void process_mouse(InterruptFrame* frame){
    (void)frame;
    uint8 data = inb(0x60);
    handle_ps2_mouse(data);

    outb(0x20, 0x20); // End PIC Master
    outb(0xA0, 0x20); // End PIC Slave
}

static void handle_ps2_mouse(uint8 data){
    process_mouse_packet();
    static bool skip = true;
    if (skip) {
        skip = false;
        return;
    }

    switch(mouse_cycle){
        case 0:
            if ((data & 0x08U) == 0) break;
            mouse_packet[0] = data;
            mouse_cycle++;
            break;
        case 1:
            mouse_packet[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_packet[2] = data;
            isMousePacketReady = true;
            mouse_cycle = 0;
            break;
    }
}

void SetMouseMovementHandler(MouseMovementHandler handler)
{
    mMovementHandler = handler;
}

void SetMouseButtonHandler(MouseButtonHandler handler)
{
    mButtonHandler = handler;
}

ivec2 GetMousePosition(void)
{
    return current_mouse_position;
}

ivec2 GetLastMousePosition(void)
{
    return previous_mouse_position;
}

static void process_mouse_packet(void){
    if (!isMousePacketReady)
        return;

    bool xNegative = (mouse_packet[0] & (int64_t)PS2_x) != 0;
    bool yNegative = (mouse_packet[0] & (int64_t)PS2_y) != 0;
    bool xOverflow = (mouse_packet[0] & (int64_t)PS2_x_overflow) != 0;
    bool yOverflow = (mouse_packet[0] & (int64_t)PS2_y_overflow) != 0;

    if (!xNegative){
        current_mouse_position.x += (int64_t)mouse_packet[1];
        if (xOverflow){
            current_mouse_position.x += 255;
        }
    } else
    {
        mouse_packet[1] = (uint8_t)(256U - mouse_packet[1]);
        current_mouse_position.x -= (int64_t)mouse_packet[1];
        if (xOverflow){
            current_mouse_position.x -= 255;
        }
    }

    if (!yNegative)
    {
        current_mouse_position.y -= (int64_t)mouse_packet[2];
        if (yOverflow)
            current_mouse_position.y -= 255;
    }
    else
    {
        mouse_packet[2] = (uint8_t)(256U - mouse_packet[2]);
        current_mouse_position.y += (int64_t)mouse_packet[2];
        if (yOverflow)
            current_mouse_position.y += 255;
    }

    if (current_mouse_position.x < 0) current_mouse_position.x = 0;
    if (fb_width > 0U && current_mouse_position.x >= (int64_t)fb_width) current_mouse_position.x = (int64_t)(fb_width - 1U);

    if (current_mouse_position.y < 0) current_mouse_position.y = 0;
    if (fb_height > 0U && current_mouse_position.y >= (int64_t)fb_height) current_mouse_position.y = (int64_t)(fb_height - 1U);

    int64_t deltaX = current_mouse_position.x - previous_mouse_position.x;
    int64_t deltaY = current_mouse_position.y - previous_mouse_position.y;

    if (mMovementHandler != NULL && (deltaX != 0 || deltaY != 0))
        mMovementHandler(deltaX, deltaY);

    mLastMouseButtonState = mMouseButtonState;
    mMouseButtonState = 0;
    mMouseButtonState |= (uint8_t)(mouse_packet[0] & (int64_t)PS2_left_button);
    mMouseButtonState |= (uint8_t)(mouse_packet[0] & (int64_t)PS2_middle_button);
    mMouseButtonState |= (uint8_t)(mouse_packet[0] & (int64_t)PS2_right_button);

    if (mLastMouseButtonState != mMouseButtonState && mButtonHandler != NULL)
    {
        if ((mLastMouseButtonState & MOUSE_BUTTON_LEFT) != (mMouseButtonState & MOUSE_BUTTON_LEFT))
            mButtonHandler(MOUSE_BUTTON_LEFT, (mMouseButtonState & MOUSE_BUTTON_LEFT) > 0 ? MOUSE_BUTTON_PRESS : MOUSE_BUTTON_RELEASE);

        if ((mLastMouseButtonState & MOUSE_BUTTON_RIGHT) != (mMouseButtonState & MOUSE_BUTTON_RIGHT))
            mButtonHandler(MOUSE_BUTTON_RIGHT, (mMouseButtonState & MOUSE_BUTTON_RIGHT) > 0 ? MOUSE_BUTTON_PRESS : MOUSE_BUTTON_RELEASE);

        if ((mLastMouseButtonState & MOUSE_BUTTON_MIDDLE) != (mMouseButtonState & MOUSE_BUTTON_MIDDLE))
            mButtonHandler(MOUSE_BUTTON_MIDDLE, (mMouseButtonState & MOUSE_BUTTON_MIDDLE) > 0 ? MOUSE_BUTTON_PRESS : MOUSE_BUTTON_RELEASE);
    }

    isMousePacketReady = false;
    previous_mouse_position = current_mouse_position;
}

__attribute__((unused)) static void handle_click(uint8_t type, ivec2 position){
    (void)position;
    switch (type)
    {
        case PS2_left_button:
            debug_println("left!");
            break;
        case PS2_right_button:
            debug_println("right!");
            break;
        case PS2_middle_button:
            debug_println("middle!");
            break;
        default:
            break;
    }
}

void init_ps2_mouse(void){
    outb(0x64, 0xA8); //enabling the auxiliary device - mouse

    ps2_mouse_wait();
    outb(0x64, 0x20); //tells the keyboard controller that we want to send a command to the mouse
    ps2_mouse_wait_input();
    uint8 status = inb(0x60);
    status |= 0x02U;
    ps2_mouse_wait();
    outb(0x64, 0x60);
    ps2_mouse_wait();
    outb(0x60, status); // setting the correct bit is the "compaq" status byte

    ps2_mouse_write(0xF6);
    (void)ps2_mouse_read();

    ps2_mouse_write(0xF4);
    (void)ps2_mouse_read();
}