import socket, time

HOST, PORT = "127.0.0.1", 4444
KEY_DELAY = 0.05    # delay between keystrokes
LINE_DELAY = 0.4    # pause after each Enter

# Each entry is typed and then Enter is pressed. Single quotes keep the guest
# shell from expanding $ and friends while the files are being written.
XFER = "/usr/local/bin/xfer.sh"

# xfer.sh with retry + resume: if the connection dies mid-download (curl 56),
# curl re-requests from the current file size instead of failing the sync.
# Re-run pacman afterwards; wait for any running download to finish first.
LINES = [
    "pacman --config /root/pacman.conf -Syy",
]

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
    if ch.isalnum():
        return ch
    raise ValueError(f"no key mapping for {ch!r}")


# fail early, before touching the VM, if any character can't be typed
for line in LINES:
    for ch in line:
        key_for(ch)

s = socket.create_connection((HOST, PORT))
time.sleep(0.5)
for line in LINES:
    for ch in line:
        s.send(f"sendkey {key_for(ch)}\n".encode())
        time.sleep(KEY_DELAY)
    s.send(b"sendkey ret\n")
    time.sleep(LINE_DELAY)
time.sleep(0.5)
s.close()