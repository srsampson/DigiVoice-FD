#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <digivoice.h>
#include <c3file.h>

int main(int argc, char **argv) {
    FILE *fin;
    FILE *fout;
    int stat;

    if (argc < 3) {
        printf("usage: encode InputRawspeechFile OutputIndexFile\n");
        printf("e.g    encode hts.raw hts.c3\n");
        exit(1);
    }

    if (strcmp(argv[1], "-") == 0) {
        fin = stdin;
    } else if ((fin = fopen(argv[1], "rb")) == NULL) {
        fprintf(stderr, "Error opening input speech file: %s: %s.\n",
                argv[1], strerror(errno));
        exit(1);
    }

    if (strcmp(argv[2], "-") == 0) {
        fout = stdout;
    } else if ((fout = fopen(argv[2], "wb")) == NULL) {
        fprintf(stderr, "Error opening output index file: %s: %s.\n",
                argv[2], strerror(errno));
        exit(1);
    }

    // Write a header if we're writing to a .c3 file
    char *ext = strrchr(argv[2], '.');

    if (ext != NULL) {
        if (strcmp(ext, ".c3") == 0) {
            struct c3_header out_hdr;

            memcpy(out_hdr.magic, c3_file_magic, sizeof (c3_file_magic));

            out_hdr.mode = CODEC2_MODE_700C;
            out_hdr.version_major = CODEC2_VERSION_MAJOR;
            out_hdr.version_minor = CODEC2_VERSION_MINOR;
            out_hdr.flags = 0;

            fwrite(&out_hdr, sizeof (out_hdr), 1, fout);
        }
    }

    if ((stat = codec_create()) == 0) {
        int nsam = codec_samples_per_frame();
        int narray = codec_indexes_per_frame();
        uint16_t indexes[narray];
        int16_t speech[nsam];

        while (fread(speech, sizeof (int16_t), nsam, fin) == nsam) {

            codec_encode(indexes, speech);

            fwrite(indexes, sizeof (uint16_t), narray, fout);

            if (fout == stdout)
                fflush(stdout);

            if (fin == stdin)
                fflush(stdin);
        }

        codec_destroy();
    }

    fclose(fin);
    fclose(fout);

    return stat;
}
