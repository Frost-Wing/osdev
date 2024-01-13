#
# This python code was created by pradosh-arduino (@github)
#

import struct
import os

def copy_file(source_path, destination_path):
    try:
        with open(source_path, 'rb') as source_file:
            with open(destination_path, 'wb') as destination_file:
                destination_file.write(source_file.read())
        print(f"File '{source_path}' copied to '{destination_path}' successfully.")
    except FileNotFoundError:
        print(f"Error: File '{source_path}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

def check_endian(file_path):
    with open(file_path, 'rb') as file:
        data = file.read(4)

    if len(data) == 4:
        little_endian_value = int.from_bytes(data, byteorder='little')
        big_endian_value = int.from_bytes(data, byteorder='big')

        if little_endian_value == big_endian_value:
            print(f"The file '{file_path}' has an indeterminate endian.")
            exit(1)
        elif little_endian_value < big_endian_value:
            print(f"The file '{file_path}' is little-endian.")
            return 1
        else:
            print(f"The file '{file_path}' is big-endian.")
            return 2
    else:
        print(f"The file '{file_path}' does not contain enough data to determine its endian.")
        exit(1)

def append_header(file_path, architecture, file_size):
    """
    Appends a custom header to a binary file.

    Args:
        file_path (str): Path to the binary file.
        architecture (int): Architecture information (32 or 64 bits).
        file_size (int): Size of the file in bytes.
    """
    signature = b'FWDE' # stands for Frost Wing Deployed Executable
    architecture_byte = struct.pack('B', architecture)  # 'B' format code packs an unsigned char (1 byte)
    size_byte = struct.pack('B', 0x69)  # 'B' format code packs an unsigned char (1 byte)

    endian = check_endian(file_path) # 0 = error; 1 = little; 2 = big

    endian_byte = struct.pack('B', endian)  # 'B' format code packs an unsigned char (1 byte)

    with open(file_path, 'rb') as file:
        existing_content = file.read()

    with open(file_path, 'wb') as file:
        file.write(signature + architecture_byte + size_byte + endian_byte)
        file.write(existing_content)

file_path = 'desktop-manager.bin'
architecture = 1  # 1 = 64 bits; 2 = 32 bits; 3 = 16 bits; 4 = 8 bits
file_size = os.path.getsize(file_path)

copy_file(file_path, 'desktop-manager.raw')

append_header(file_path, architecture, file_size)