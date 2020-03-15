#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <digivoice.h>
#include <c3file.h>

int main(int argc, char *argv[]) {
    struct c3_header in_hdr;
    FILE *fin;
    FILE *fout;
    int stat;

    if (argc < 3) {
        printf("usage: decode InputIndexFile OutputRawspeechFile\n");
        printf("e.g    decode hts.c3 result.raw\n");
        exit(1);
    }

    if (strcmp(argv[1], "-") == 0)
        fin = stdin;
    else if ((fin = fopen(argv[1], "rb")) == NULL) {
        fprintf(stderr, "Error opening InputIndexFile: %s: %s.\n",
                argv[1], strerror(errno));
        exit(1);
    }

    if (strcmp(argv[2], "-") == 0)
        fout = stdout;
    else if ((fout = fopen(argv[2], "wb")) == NULL) {
        fprintf(stderr, "Error opening OutputRawspeechFile: %s: %s.\n",
                argv[2], strerror(errno));
        exit(1);
    }

    // Attempt to detect a .c3 file with a header
    char *ext = strrchr(argv[1], '.');

    if ((ext != NULL) && (strcmp(ext, ".c3") == 0)) {
        fread(&in_hdr, sizeof (in_hdr), 1, fin);

        if (memcmp(in_hdr.magic, c3_file_magic, sizeof (c3_file_magic)) == 0) {
            fprintf(stderr, "Detected Codec2 file version %d.%d in mode %d\n",
                    in_hdr.version_major,
                    in_hdr.version_minor,
                    in_hdr.mode);
        } else {
            fprintf(stderr, "Codec2 file specified but no header detected\n");
            fseek(fin, 0, SEEK_SET);
        }
    }

    if (in_hdr.mode != CODEC2_MODE_700C) {
        fprintf(stderr, "Codec2 file is not mode 700C\n");
        return -1;
    }

    if ((stat = codec_create()) == 0) {
        int nsam = codec_samples_per_frame();
        int narray = codec_indexes_per_frame();
        uint16_t indexes[narray];
        int16_t speech[nsam];
        
        while (fread(indexes, sizeof (uint16_t), narray, fin) == narray) {
            codec_decode(speech, indexes);

            fwrite(speech, sizeof (int16_t), nsam, fout);

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
