#include <math.h>

#include "imgdata.h"

Imgdata *bilinear(Imgdata *img, const double scale_x, const double scale_y)
{
    Imgdata *out = Imgdata_alloc((img->width * scale_x), (img->height * scale_y), img->channel);

    for (int y = 0; y < out->height; y++) {
        for (int x = 0; x < out->width; x++) {
            for (int c = 0; c < out->channel; c++) {
                int p_x = floor(x / scale_x);
                int p_y = floor(y / scale_y);

                double dx = x / scale_x - p_x;
                double dy = y / scale_y - p_y;

                int val = (1 - dx) * (1 - dy) * Imgdata_at(img, p_x, p_y)[c]
                          + dx * (1 - dy) * Imgdata_at(img, (p_x + 1), p_y)[c]
                          + (1 - dx) * dy * Imgdata_at(img, p_x, (p_y + 1))[c]
                          + dx * dy * Imgdata_at(img, (p_x + 1), (p_y + 1))[c];

                Imgdata_at(out, x, y)[c] = Imgdata_sat_u8(val);
            }
        }
    }

    return out;
}

int main(int argc, char *argv[])
{
    Imgdata *img = Imgdata_read_png("./imori_128x128.png");

    Imgdata *img_bilinear = bilinear(img, 1.5, 1.5);

    Imgdata_write_png(img_bilinear, "./026_bilinear.png");

    Imgdata_free(&img);
    Imgdata_free(&img_bilinear);

    return 0;
}
