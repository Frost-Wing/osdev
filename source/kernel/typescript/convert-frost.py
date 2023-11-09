try:
    file_path = "./typescript-source.c"
    libraries = "#include <kernel.h>\n#include <graphics.h>\n#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n"
    with open(file_path, 'r') as file:
        content = file.read()

    # Replace the old text with the new text
    modified_content = content.replace("main", "typescript_main").replace("printf", "print").replace("#include <stdio.h>", "")
    
    modified_content = libraries + modified_content;

    with open(file_path, 'w') as file:
        file.write(modified_content)
except FileNotFoundError:
    print(f"File '{file_path}' not found.")
except Exception as e:
    print(f"An error occurred: {str(e)}")