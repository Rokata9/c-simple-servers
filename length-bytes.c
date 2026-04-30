#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LENGTH_BYTES 5

uint8_t *
convert_to_content_length_bytes(uint32_t payload_length)
{
    int      processed_bytes = 0;
    int      shift = 0;
    uint8_t *result = calloc(MAX_LENGTH_BYTES, sizeof(uint8_t));
    uint32_t remaining = payload_length;

    while (1)
    {
        uint32_t bits_mask = 0x7F << shift;
        uint32_t cur_seven_bits_value = bits_mask & payload_length;

        // cap it to max 127
        uint8_t stripped_byte_before = cur_seven_bits_value >> shift;

        remaining -= cur_seven_bits_value;

        uint8_t stripped_byte_final = stripped_byte_before;

        if (remaining > 0)
            stripped_byte_final |= 0x80; // set bit 8 as 1 if there's more

        *(result + processed_bytes) = stripped_byte_final;

        printf("Byte #%d bits_mask = %d, cur_seven_bits_value = %d, "
               "remaining = %d, stripped_byte_before = %d, stripped_byte_final = %d\n",
               processed_bytes, bits_mask, cur_seven_bits_value, remaining, stripped_byte_before, stripped_byte_final);

        if (remaining <= 0)
            break;

        ++processed_bytes;

        printf("----------\n");
        shift += 7;
    }

    printf("----------\n");
    printf("Total count: %d\n", processed_bytes + 1);

    return result;
}

int
main()
{
    uint32_t content_length = UINT32_MAX; // [255, 255, 255, 255, 15]
    uint8_t *result;

    result = convert_to_content_length_bytes(content_length);

    for (int i = 0; i < MAX_LENGTH_BYTES; i++)
        printf("byte %d: %d\n", i, *(result + i));

    return 0;
}
