#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint64_t pthread_create_addr;
  uint64_t sandbox_consume_addr;
  uint64_t dlopen_addr;
  uint64_t payload_path;
  uint64_t sandbox_token;
} shellcode_offsets_t;

void save_shellcode_file(const char *data, size_t size, shellcode_offsets_t *offsets) {
  FILE *f = fopen("shellcode.h", "w");
  if (!f) {
    perror("failed to open output file");
    return;
  }

  fprintf(f, "// shellcode.s\n"
             "// auto-generated shellcode header\n\n"
             "#ifndef SHELLCODE_H\n"
             "#define SHELLCODE_H\n\n"
             "#define PTHREAD_CREATE   %llu\n"
             "#define SANDBOX_CONSUME  %llu\n"
             "#define DLOPEN           %llu\n"
             "#define PAYLOAD_PATH     %llu\n"
             "#define SANDBOX_TOKEN    %llu\n\n"
             "static char shell_code[] = \"",
          offsets->pthread_create_addr,
          offsets->sandbox_consume_addr,
          offsets->dlopen_addr,
          offsets->payload_path,
          offsets->sandbox_token);

  for (size_t i = 0; i < size; i++)
    fprintf(f, "\\x%02X", (unsigned char)data[i]);

  fprintf(f, "\";\n\n#endif // SHELLCODE_H\n");

  fclose(f);
}

void extract_text_section(const char *filename) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    perror("failed to open file");
    return;
  }

  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buffer = malloc(file_size);
  fread(buffer, 1, file_size, f);
  fclose(f);

  struct mach_header_64 *header = (struct mach_header_64 *)buffer;

  if (header->magic != MH_MAGIC_64) {
    fprintf(stderr, "not a 64-bit mach-o file\n");
    free(buffer);
    return;
  }

  struct symtab_command *symtab = NULL;
  uint64_t text_section_addr = 0;
  uint64_t text_section_offset = 0;
  uint64_t text_section_size = 0;

  struct load_command *lc = (struct load_command *)(buffer + sizeof(struct mach_header_64));

  for (uint32_t i = 0; i < header->ncmds; i++) {
    if (lc->cmd == LC_SYMTAB) {
      symtab = (struct symtab_command *)lc;
    } else if (lc->cmd == LC_SEGMENT_64) {
      struct segment_command_64 *seg = (struct segment_command_64 *)lc;

      if (strcmp(seg->segname, SEG_TEXT) == 0) {
        struct section_64 *sec = (struct section_64 *)((char *)seg + sizeof(struct segment_command_64));

        for (uint32_t j = 0; j < seg->nsects; j++) {
          if (strcmp(sec[j].sectname, SECT_TEXT) == 0) {
            text_section_addr = sec[j].addr;
            text_section_offset = sec[j].offset;
            text_section_size = sec[j].size;
          }
        }
      }
    }
    lc = (struct load_command *)((char *)lc + lc->cmdsize);
  }

  if (!symtab || text_section_offset == 0) {
    fprintf(stderr, "required sections not found\n");
    free(buffer);
    return;
  }

  struct nlist_64 *symbols = (struct nlist_64 *)(buffer + symtab->symoff);
  char *string_table = buffer + symtab->stroff;

  shellcode_offsets_t offsets = {0};

  for (uint32_t i = 0; i < symtab->nsyms; i++) {
    char *name = string_table + symbols[i].n_un.n_strx;
    uint64_t value = symbols[i].n_value;

    if (strcmp(name, "_pthread_create_addr") == 0) {
      offsets.pthread_create_addr = value - text_section_addr;
    } else if (strcmp(name, "_sandbox_consume_addr") == 0) {
      offsets.sandbox_consume_addr = value - text_section_addr;
    } else if (strcmp(name, "_dlopen_addr") == 0) {
      offsets.dlopen_addr = value - text_section_addr;
    } else if (strcmp(name, "_payload_path") == 0) {
      offsets.payload_path = value - text_section_addr;
    } else if (strcmp(name, "_sandbox_token") == 0) {
      offsets.sandbox_token = value - text_section_addr;
    }
  }

  save_shellcode_file(buffer + text_section_offset, text_section_size, &offsets);
  free(buffer);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
    return 1;
  }

  extract_text_section(argv[1]);
  return 0;
}
