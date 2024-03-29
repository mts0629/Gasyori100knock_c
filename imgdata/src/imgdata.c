#include "imgdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "png.h"

Imgdata *Imgdata_alloc(const int width, const int height, const int channel)
{
    if ((width < 1) || (height < 1) || (channel < 1)) {
        printf("[error] invalid image size: (W,H,C)=(%d,%d,%d)\n", width, height, channel);
        return NULL;
    }

    Imgdata *img = (Imgdata*)malloc(sizeof(Imgdata));
    if (img == NULL) {
        printf("[error] failed to allocate Imgdata\n");
        return NULL;
    }

    img->width   = width;
    img->height  = height;
    img->channel = channel;
    img->stride  = width * channel;

    img->data = malloc(sizeof(int32_t) * height * width * channel);
    if (img->data == NULL) {
        printf("[error] failed to allocate Imgdata\n");
        free(img);
        img = NULL;
        return NULL;
    }

    return img;
}

void Imgdata_free(Imgdata **img)
{
    free((*img)->data);
    (*img)->data = NULL;

    free(*img);
    *img = NULL;
}

Imgdata *Imgdata_read_png(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("[error] faied to open file \"%s\"", filename);
        return NULL;
    }

    // check PNG signature (first 8 Bytes)
    uint8_t signature[8];
    size_t read_size = fread(signature, 1, 8, fp);
    if (png_sig_cmp(signature, 0, 8)) {
        printf("[error] signature error: png_sig_cmp()\n");
        fclose(fp);
        return NULL;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        printf("[error] failed to create data structure: png_create_read_struct()\n");
        fclose(fp);
        return NULL;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        printf("[error] failed to create info structure: png_create_info_struct()\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    png_infop end_info = png_create_info_struct(png_ptr);
    if (end_info == NULL) {
        printf("[error] failed to create info structure: png_create_info_struct()\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
        return NULL;
    }

    // initialize stream
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, read_size);

    // read data
    png_read_png(png_ptr, info_ptr, (PNG_TRANSFORM_PACKING | PNG_TRANSFORM_STRIP_16), NULL);

    // check color type
    png_byte type = png_get_color_type(png_ptr, info_ptr);
    int channel;
    if (type == PNG_COLOR_TYPE_GRAY) {
        channel = 1;
    } else if (type == PNG_COLOR_TYPE_RGB) {
        channel = 3;
    } else if (type == PNG_COLOR_TYPE_RGB_ALPHA) {
        channel = 4;
    } else {
        printf("[error] invalid color type: only GRAY/RGB/RGB_ALPHA are supported\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }

    // allocate Imgdata
    int width  = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);

    Imgdata *img = Imgdata_alloc(width, height, channel);
    if (img == NULL) {
        printf("[error] failed to allocate data\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return NULL;
    }

    // copy data per row
    png_bytepp datap = png_get_rows(png_ptr, info_ptr);
    const int w_elems = img->width * img->channel;
    for (int i = 0; i < img->height; i++) {
        uint8_t *src_p = datap[i];
        int32_t *dst_p = (img->data + i * img->stride);
        // data copy with cast
        for (int j = 0; j < w_elems ; j++) {
            dst_p[j] = (int32_t)src_p[j];
        }
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    return img;
}

bool Imgdata_write_png(const Imgdata *img, const char *filename)
{
    if (img == NULL) {
        return false;
    }

    const int size = img->height * img->width * img->channel;
    for (int i = 0; i < size; i++) {
        int pixel = img->data[i];
        if ((pixel < 0) || (pixel > UINT8_MAX)) {
            printf("[error] the pixel value (%d) exceeds the range of unsigned 8bit data\n", pixel);
            return false;
        }
    }

    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        return false;
    }

    png_structp png_ptr  = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        printf("[error] failed to create data structure: png_create_write_struct()\n");
        fclose(fp);
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        printf("[error] failed to create info structure: png_create_info_struct()\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return false;
    }

    png_byte type;
    if (img->channel == 1) {
        type = PNG_COLOR_TYPE_GRAY;
    } else if (img->channel == 3) {
        type = PNG_COLOR_TYPE_RGB;
    } else if (img->channel == 4) {
        type = PNG_COLOR_TYPE_RGB_ALPHA;
    } else {
        printf("[error] invalid channel num=%d, only 1(GRAY)/3(RGB)/4(RGB_ALPHA) are supported\n", img->channel);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return false;
    }

    // initialize stream
    png_init_io(png_ptr, fp);

    // set header info
    const int bit_depth = 8;
    png_set_IHDR(png_ptr, info_ptr, img->width, img->height, bit_depth, type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    // copy data per row
    png_bytepp datap = png_malloc(png_ptr, sizeof(png_bytep) * img->height);
    png_set_rows(png_ptr, info_ptr, datap);

    const int w_elems = img->width * img->channel;
    for (int i = 0; i < img->height; i++) {
        datap[i] = png_malloc(png_ptr, img->stride);
        int32_t *src_p = (img->data + i * w_elems);
        uint8_t *dst_p = datap[i];
        // data copy with cast
        for (int j = 0; j < w_elems; j++) {
            dst_p[j] = (uint8_t)src_p[j];
        }
    }

    // write data
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    for (int i = 0; i < img->height; i++) {
        png_free(png_ptr, datap[i]);
    }
    png_free(png_ptr, datap);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose(fp);

    return true;
}
