import socket, time

LINE = 'python -c "import fcntl; b=bytearray(44); print(fcntl.ioctl(0,0x802c542a,b),list(b[:4]))"'

KEYS = {
    ' ': 'spc', '-': 'minus', '_': 'shift-minus', '=': 'equal', '+': 'shift-equal',
    ',': 'comma', '.': 'dot', ';': 'semicolon', ':': 'shift-semicolon',
    '"': 'shift-apostrophe', "'": 'apostrophe',
    '(': 'shift-9', ')': 'shift-0', '/': 'slash', '\\': 'backslash',
    '[': 'bracket_left', ']': 'bracket_right',
    '{': 'shift-bracket_left', '}': 'shift-bracket_right',
    '<': 'shift-comma', '>': 'shift-dot', '?': 'shift-slash', '|': 'shift-backslash',
    '`': 'grave_accent', '~': 'shift-grave_accent',
    '!': 'shift-1', '@': 'shift-2', '#': 'shift-3', '$': 'shift-4', '%': 'shift-5',
    '^': 'shift-6', '&': 'shift-7', '*': 'shift-8',
}

def key_for(ch):
    if ch in KEYS:
        return KEYS[ch]
    if ch.isupper():
        return 'shift-' + ch.lower()
    return ch  # lowercase letters and digits map to themselves

s = socket.create_connection(("127.0.0.1", 4444))
time.sleep(0.5)
for ch in LINE:
    s.send(f"sendkey {key_for(ch)}\n".encode())
    time.sleep(0.05)
s.send(b"sendkey ret\n")
s.close()