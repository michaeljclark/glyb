#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#define FLOAT32 "%.9g"

struct atlas_ent
{
    int bin_id, font_id, glyph;
    short x, y, ox, oy, w, h;
};

int main(int argc, char **argv)
{
    FILE *in = nullptr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    if (!(in = fopen(argv[1], "r"))) {
        fprintf(stderr, "error: fopen: %s, %s\n", argv[1], strerror(errno));
        exit(1);
    }

    const int num_fields = 9;

    int ret;
    do {
        atlas_ent ent;
        ret = fscanf(in, "%d,%d,%d,%hd,%hd,%hd,%hd,%hd,%hd\n",
            &ent.bin_id, &ent.font_id, &ent.glyph,
            &ent.x, &ent.y, &ent.ox, &ent.oy, &ent.w, &ent.h);
        if (ret == num_fields) {
            printf("%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                ent.bin_id, ent.font_id, ent.glyph,
                ent.x, ent.y, ent.ox, ent.oy, ent.w, ent.h);
        }
    } while (ret == num_fields);

    fclose(in);
}