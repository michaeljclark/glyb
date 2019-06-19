// See LICENSE for license details.

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cctype>

#include <string>
#include <vector>
#include <memory>

#include "logger.h"
#include "image.h"
#include "file.h"

#include <png.h>

#include <string.h>
#include <setjmp.h>

static char class_name[] = "image";

int image::iid_seq = 0;

image_io_png  image::PNG;

const char* image::formatname[] = {
    "None",
    "Alpha",
    "RGB",
    "RGBA",
    "ARGB",
    "RGB555",
    "RGB565",
    "Luminance",
    "LuminanceAlpha",
    "Compressed",
};

image_io* image::getImageIO(unsigned char magic[8])
{
    static const unsigned char PNG_MAGIC[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
    };

    if (memcmp(magic, PNG_MAGIC, sizeof(PNG_MAGIC)) == 0) {
        return &PNG;
    } else {
        return NULL;
    }
}

image_io* image::getImageIO(file_ptr rsrc)
{
    size_t bytesRead;
    unsigned char magic[8];    

    if ((bytesRead = rsrc->read(magic, sizeof(magic))) != sizeof(magic)) {
        Error("%s: error reading magic: %s: %s\n", __func__,
            rsrc->getPath().c_str(), strerror(errno));
        rsrc->close();
        return NULL;
    }
    rsrc->close();
    
    if (bytesRead != sizeof(magic)) {
        Error("%s: error reading magic: %s: short read\n", __func__,
            rsrc->getPath().c_str());
        return NULL;
    }

    return getImageIO(magic);
}

image_io* image::getImageIO(std::string filename)
{
    FILE *file;
    size_t bytesRead;
    unsigned char magic[8];
    
    if ((file = fopen(filename.c_str(), "r")) == NULL) {
        Error("%s: error opening file: %s: %s\n", __func__,
            filename.c_str(), strerror(errno));
        return NULL;
    }
    
    if ((bytesRead = fread(magic, 1, sizeof(magic), file)) != sizeof(magic)) {
        Error("%s: error reading magic: %s: %s\n", __func__,
            filename.c_str(), strerror(errno));
        fclose(file);
        return NULL;
    }
    fclose(file);
    
    if (bytesRead != sizeof(magic)) {
        Error("%s: error reading magic: %s: short read\n", __func__,
            filename.c_str());
        return NULL;
    }

    return getImageIO(magic);
}

image_io* image::getImageIOFromExt(std::string pathname)
{
    size_t dotoffset = pathname.find_last_of('.');
    if (dotoffset == std::string::npos) {
        return NULL;
    }
    
    std::string type = pathname.substr(dotoffset + 1);
    if (type == "png") {
        return &PNG;
    } else {
        return NULL;
    }
}

void image::saveToFile(std::string filename,
    const image_ptr &image, image_io *imageio)
{
    if (!imageio) {
        imageio = getImageIOFromExt(filename);
    }
    if (imageio) {
        imageio->save(image.get(), filename);
    } else {
        Error("%s: error unknown format: %s\n", __func__, filename.c_str());
    }
}

image_ptr image::createBitmap(uint width, uint height, pixel_format format,
    uint8_t *pixels)
{
    image_ptr img;
    if (pixels) {
        img = image_ptr(new image(file_ptr(), width, height, format, pixels));
    } else {
        img = image_ptr(new image(file_ptr(), width, height, format));
    }
    return img;
}

image_ptr image::createFromFile(std::string filename, image_io *imageio,
    pixel_format optformat)
{    
    file_ptr rsrc = file::getFile(filename);
    image_ptr image = createFromResouce(rsrc, imageio, optformat);
    return image;
}

image_ptr image::createFromResouce(std::string rsrcName, image_io *imageio,
    pixel_format optformat)
{
    file_ptr rsrc = file::getResource(rsrcName);
    image_ptr image = createFromResouce(rsrc, imageio, optformat);
    return image;
}

image_ptr image::createFromResouce(file_ptr rsrc, image_io *imageio,
    pixel_format optformat)
{
    std::string rsrcName = rsrc->getPath();
    std::string name = rsrcName.substr(0, rsrcName.find_last_of('.'));
    std::string ext = rsrcName.substr(rsrcName.find_last_of('.')+1);

    image_ptr image;
    
    if (rsrcName.size() == 0) {
        Error("%s: error file does not exist: %s\n", __func__,
            rsrcName.c_str());
        return image;
    }
    
    Debug("%s rsrc=%s\n", __func__, rsrcName.c_str());
    if (!imageio) {
        imageio = getImageIOFromExt(rsrcName);
    }
    if (!imageio) {
        imageio = getImageIO(rsrc);
    }
    if (imageio) {
        image = image_ptr(imageio->load(rsrc, optformat));
    }
    if (image) {
        Debug("%s width=%d height=%d format=%s\n", __func__,
            (int)image->width, (int)image->height, formatname[image->format]);
    } else {
        Error("%s: error could not load %s\n", __func__, rsrcName.c_str());
    }

    return image;
}

image::~image()
{
    if (ownData && pixels) {
        delete [] pixels;
        pixels = nullptr;
    }
}

void image::create(pixel_format format, uint width, uint height)
{
    this->width = width;
    this->height = height;
    this->format = format;

    size_t size = (size_t)width * height * getBytesPerPixel();
    pixels = new uint8_t[size];
    memset(pixels, 0, size);
    ownData = true;
}

void image::convertFormat(pixel_format newformat)
{
    if (format == newformat) return;
    Debug("%s converting from %s to %s\n", __func__,
        formatname[format], formatname[newformat]);
    size_t size = (size_t)width * height * getBytesPerPixel(newformat);
    uint8_t *newpixels = new uint8_t[size];
    uint8_t *src = pixels, *dest = newpixels;
    uint8_t c[4] = {};
    for (uint i=0; i < width * height; i++) {
        // load source pixel
        switch (format) {
            case pixel_format_rgba:                 
                c[0] = *(src++);
                c[1] = *(src++);
                c[2] = *(src++);
                c[3] = *(src++);
                break;
            case pixel_format_argb:
                c[1] = *(src++);
                c[2] = *(src++);
                c[3] = *(src++);
                c[0] = *(src++);
                break;
            case pixel_format_rgb:                  
                c[0] = *(src++);
                c[1] = *(src++);
                c[2] = *(src++);
                c[3] = 0xff;
                break;
            case pixel_format_rgb555:
                c[0] = (*((ushort*)src) & 0x7c00)>>7;
                c[1] = (*((ushort*)src) & 0x3e0)>>2;
                c[2] = (*((ushort*)src) & 0x1f)<<3;
                c[3] = 0xff;
                src += 2;
                break;
            case pixel_format_rgb565:
                c[0] = (*((ushort*)src) & 0xf800)>>8;
                c[1] = (*((ushort*)src) & 0x7e0)>>3;
                c[2] = (*((ushort*)src) & 0x1f)<<3;
                c[3] = 0xff;
                src += 2;
                break;
            case pixel_format_luminance:
                c[0] = c[1] = c[2] = *(src++);
                c[3] = 0xff;
                break;
            case pixel_format_alpha:
                c[0] = c[1] = c[2] = 0x00;
                c[3] = *(src++);
                break;
            default:
                break;
        }
        // store dest pixel
        switch (newformat) {
            case pixel_format_rgba:
                *(dest++) = c[0];
                *(dest++) = c[1];
                *(dest++) = c[2];
                *(dest++) = c[3];
		break;
            case pixel_format_argb:
                *(dest++) = c[1];
                *(dest++) = c[2];
                *(dest++) = c[3];
                *(dest++) = c[0];
                break;
            case pixel_format_rgb:
                *(dest++) = c[0];
                *(dest++) = c[1];
                *(dest++) = c[2];
                break;
            case pixel_format_rgb555:
                *((ushort*)dest) = (((ushort)c[0] << 7) & 0x7c00) |
                    (((ushort)c[1] << 2) & 0x3e0) | (((ushort)c[2] >> 3) & 0x1f);
                dest += 2;
                break;
            case pixel_format_rgb565:
                *((ushort*)dest) = (((ushort)c[0] << 8) & 0xf800) |
                    (((ushort)c[1] << 3) & 0x7e0) | (((ushort)c[2] >> 3) & 0x1f);
                dest += 2;
                break;
            case pixel_format_luminance:
                *(dest++) = (uint8_t)((c[0] + c[1] + c[2]) / 3);
                break;
            case pixel_format_alpha:
                *(dest++) = c[3];
                break;
            default:
                break;
        }
    }
    delete [] pixels;
    format = newformat;
    pixels = newpixels;
}

static void image_io_png_png_read_pixels(png_structp png_ptr, png_bytep pixels,
    png_size_t length)
{
    file *rsrc = (file*)png_get_io_ptr(png_ptr);
    if (!rsrc) {
        png_error(png_ptr, "read from NULL file");
    } else if (rsrc->read(pixels, length) < 0) {
        Debug("%s: error %s: %s\n", __func__,
            rsrc->getErrorMessage().c_str(), rsrc->getPath().c_str());
        png_error(png_ptr, rsrc->getErrorMessage().c_str());
    }
}

image* image_io_png::load(file_ptr rsrc, pixel_format optformat)
{
    png_structp png_ptr;
    png_infop info_ptr;
    int bit_depth, color_type;
    int i;
    size_t row_stride;
    png_bytepp row_arr = NULL;
    
    uint width, height;
    pixel_format format;
    uint8_t *pixels = NULL;
    
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        Debug("%s: error creating png read struct\n", __func__);
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        Debug("%s: error creating png infostruct\n", __func__);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        Debug("%s: error decompressing png\n", __func__);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        free(row_arr);
        free(pixels);
        return NULL;
    }

    png_set_read_fn(png_ptr, rsrc.get(), image_io_png_png_read_pixels);
    png_set_sig_bytes(png_ptr, 0);

    /* read all the info up to the image pixels  */
    png_read_info(png_ptr, info_ptr);

    png_uint_32 pwidth, pheight;
    png_get_IHDR(png_ptr, info_ptr, &pwidth, &pheight, &bit_depth, &color_type,
        NULL, NULL, NULL);
    width = pwidth;
    height = pheight;

    /* Set up some transforms. */
    if (color_type & PNG_COLOR_MASK_ALPHA) {
        format = pixel_format_rgba;
    } else {
        format = pixel_format_rgb;
    }
    if (bit_depth > 8) {
        png_set_strip_16(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }

    /* Update the png info struct.*/
    png_read_update_info(png_ptr, info_ptr);

    /* Rowsize in bytes. */
    row_stride = png_get_rowbytes(png_ptr, info_ptr);

    /* Allocate the image_pixels buffer. */
    if ((pixels = new uint8_t[row_stride * height]) == NULL) {
        Debug("%s: error allocating memory\n", __func__);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    /* allocate row pointers */
    if ((row_arr = (png_bytepp) malloc(height * sizeof(png_bytep))) == NULL) {
        Debug("%s: error allocating memory\n", __func__);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        free(pixels);
        return NULL;
    }

    /* set the individual row_arr to point at the correct offsets */
    for (i = 0; i < (int) height; ++i) {
        row_arr[i] = pixels + i * row_stride;
    }

    /* now we can go ahead and just read the whole image */
    png_read_image(png_ptr, row_arr);

    /* and we're done!  (png_read_end() can be omitted if no processing of
     * post-IDAT text/time/etc. is desired) */

    /* Clean up. */
    free(row_arr);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    rsrc->close();
    
    return new image(rsrc, width, height, format, pixels);
}

void image_io_png::save(image* image, std::string filename)
{
    FILE *outfile;
    png_structp png_ptr;
    png_infop info_ptr;
    uint8_t** row_arr = NULL;
    
    if ((outfile = fopen(filename.c_str(), "wb")) == NULL) {
        Debug("%s: error opening file %s\n", filename.c_str(), __func__);
        return;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        Debug("%s: error creating png write struct\n", __func__);
        fclose(outfile);
        return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        Debug("%s: error creating png info struct\n", __func__);
        fclose(outfile);
        png_destroy_write_struct(&png_ptr, NULL);
        return;
     }

    if (setjmp(png_jmpbuf(png_ptr))) {
        Debug("%s: error compressing png\n", __func__);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(outfile);
        free(row_arr);
        return;
    }

    uint8_t color_type;
    switch (image->getpixel_format()) {
        case pixel_format_alpha: color_type = PNG_COLOR_TYPE_GRAY; break;
        case pixel_format_luminance: color_type = PNG_COLOR_TYPE_GRAY; break;
        case pixel_format_rgb: color_type = PNG_COLOR_TYPE_RGB; break;
        case pixel_format_rgba: color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
        default:
            Debug("%s pixel format not supported\n", __func__);
            fclose(outfile);
            png_destroy_write_struct(&png_ptr, NULL);
            return;
    }
    png_init_io(png_ptr, outfile);
    png_set_IHDR(png_ptr, info_ptr, image->getWidth(), image->getHeight(), 8,
            color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
            PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    uint8_t *pixels = image->getData();
    row_arr = (uint8_t**) malloc(sizeof(uint8_t*) * image->getHeight());
    for (uint y = 0; y < image->getHeight(); y++) {
        row_arr[y] = pixels;
        pixels += image->getWidth() * image->getBytesPerPixel();
    }
    png_write_image(png_ptr, row_arr);
    png_write_end(png_ptr, NULL);

    /* Clean up */
    free(row_arr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(outfile);
}
