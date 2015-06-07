#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>

void hexdump(FILE *file, const void *ptr, uint32_t address, size_t len)
{
	size_t count;
	int i;
	const void *data = ptr;

	for (count = 0 ; count < len; count += 16) {
		fprintf(file, "0x%08lx: ", address  + count);
		fprintf(file, "%08x %08x %08x %08x |", *(const uint32_t *)data, *(const uint32_t *)(data + 4), *(const uint32_t *)(data + 8), *(const uint32_t *)(data + 12));
    
		for (i=0; i < 16; i++) {
			char c = *(const char *)(data + ((i / 4) * 4) + (3 - (i % 4)));
			if (isalnum(c)) {
				fprintf(file, "%c", c);
			} else {
				fprintf(file, ".");
			}
		}
		fprintf(file, "|\n");
		data += 16;
	}
}
