/**
 * @file ps2-mouse.c
 * @author Poncho (AbsurdPoncho) & Pradosh (pradoshgame@gmail.com)
 * @brief The main source code for PS/2 Mouse input
 * @version 0.1
 * @date 2023-12-10
 * 
 */
#include <ps2-mouse.h>

extern int64 fb_width;
extern int64 fb_height;

void ps2_mouse_wait(){
    int64 timeout = 100000;
    while (timeout--){
        if ((inb(0x64) & 0b10) == 0){
            return;
        }
    }
}

void ps2_mouse_wait_input(){
    int64 timeout = 100000;
    while (timeout--){
        if (inb(0x64) & 0b1){
            return;
        }
    }
}

void ps2_mouse_write(int8 value){
    ps2_mouse_wait();
    outb(0x64, 0xD4);
    ps2_mouse_wait();
    outb(0x60, value);
}

int8 ps2_mouse_read(){
    ps2_mouse_wait_input();
    return inb(0x60);
}

int8 mouse_cycle = 0;
int64 mouse_packet[4];
bool isMousePacketReady = false;
ivec2 current_mouse_position;
ivec2 previous_mouse_position;
MouseMovementHandler mMovementHandler = NULL;

// https://bitmap-code-generator.benalman.com/share/mouse_cursor/8/16/g30e1s7ovjvfvv7gs200000000
const int8 mouse_cursor[] = {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xfc, 0xf0, 0xe0, 0x80, 0x00, 0x00, 0x00, 0x00};

void process_mouse(InterruptFrame* frame){
    int8 data = inb(0x60);
    handle_ps2_mouse(data);

    outb(0x20, 0x20); // End PIC Master
    outb(0xA0, 0x20); // End PIC Slave
}

void handle_ps2_mouse(int8 data){
    process_mouse_packet();
    static bool skip = true;
    if (skip) {
        skip = false;
        return;
    }

    switch(mouse_cycle){
        case 0:
            if ((data & 0b00001000) == 0) break;
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

void SetMouseHandler(MouseMovementHandler handler)
{
    mMovementHandler = handler;
}

ivec2 GetMousePosition()
{
    return current_mouse_position;
}

ivec2 GetLastMousePosition()
{
    return previous_mouse_position;
}

void process_mouse_packet(){
    if (!isMousePacketReady)
        return;

    bool xNegative, yNegative, xOverflow, yOverflow;

    xNegative = mouse_packet[0] & PS2_x;
    yNegative = mouse_packet[0] & PS2_y;
    xOverflow = mouse_packet[0] & PS2_x_overflow;
    yOverflow = mouse_packet[0] & PS2_y_overflow;

    if (!xNegative){
        current_mouse_position.x += mouse_packet[1];
        if (xOverflow){
            current_mouse_position.x += (int64)255;
        }
    } else
    {
        mouse_packet[1] = ((int64)256) - mouse_packet[1];
        current_mouse_position.x -= mouse_packet[1];
        if (xOverflow){
            current_mouse_position.x -= (int64)255;
        }
    }

    if (!yNegative)
    {
        current_mouse_position.y -= mouse_packet[2];
        if (yOverflow)
            current_mouse_position.y -= (int64)255;
    }
    else
    {
        mouse_packet[2] = ((int64)256) - mouse_packet[2];
        current_mouse_position.y += mouse_packet[2];
        if (yOverflow)
            current_mouse_position.y += (int64)255;
    }

    if (current_mouse_position.x < 0) current_mouse_position.x = 0;
    if (current_mouse_position.x > fb_width) current_mouse_position.x = fb_width - 1;

    if (current_mouse_position.y < 0) current_mouse_position.y = 0;
    if (current_mouse_position.y > fb_height) current_mouse_position.y = fb_height - 1;

    int64_t deltaX = current_mouse_position.x - previous_mouse_position.x;
    int64_t deltaY = current_mouse_position.y - previous_mouse_position.y;

    if (mMovementHandler != NULL)
        mMovementHandler(deltaX, deltaY);

    if (mouse_packet[0] & PS2_left_button){
        handle_click(PS2_left_button, current_mouse_position);
    }
    if (mouse_packet[0] & PS2_middle_button){
        handle_click(PS2_middle_button, current_mouse_position);
    }
    if (mouse_packet[0] & PS2_right_button){
        handle_click(PS2_right_button, current_mouse_position);
    }

    isMousePacketReady = false;
    previous_mouse_position = current_mouse_position;
}

void handle_click(int64 type, ivec2 position){
    switch (type)
    {
        case PS2_left_button:
            debug_println("left!");
            printf("{%d, %d}", position.x, position.y);
            break;
        case PS2_right_button:
            debug_println("right!");
            // printf("{%d, %d}", position.x, position.y);
            break;
        case PS2_middle_button:
            debug_println("middle!");
            // printf("{%d, %d}", position.x, position.y);
            break;
        default:
            break;
    }
}

void init_ps2_mouse(){
    outb(0x64, 0xA8); //enabling the auxiliary device - mouse

    ps2_mouse_wait();
    outb(0x64, 0x20); //tells the keyboard controller that we want to send a command to the mouse
    ps2_mouse_wait_input();
    int8 status = inb(0x60);
    status |= 0b10;
    ps2_mouse_wait();
    outb(0x64, 0x60);
    ps2_mouse_wait();
    outb(0x60, status); // setting the correct bit is the "compaq" status byte

    ps2_mouse_write(0xF6);
    ps2_mouse_read();

    ps2_mouse_write(0xF4);
    ps2_mouse_read();
}