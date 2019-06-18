// See LICENSE for license details.

#pragma once

typedef unsigned int uint;
typedef unsigned short ushort;

struct image;
typedef std::shared_ptr<image> image_ptr;

struct file;
typedef std::shared_ptr<file> file_ptr;

struct image_io;
struct image_io_png;

/* pixel_format */

enum pixel_format
{
    pixel_format_none,
    pixel_format_alpha,
    pixel_format_rgb,
    pixel_format_rgba,
    pixel_format_argb,
    pixel_format_rgb555,
    pixel_format_rgb565,
    pixel_format_luminance,
    pixel_format_luminance_alpha,
};

/* image */

struct image
{
public:
    static const char* formatname[];

    static image_io_png PNG;

    static int iid_seq;

    int iid;
    file_ptr rsrc;
    uint width, height;
    pixel_format format;
    uint8_t *pixels;
    bool ownData;
    
    image();
    image(const image &image);
    image(file_ptr rsrc, uint width, uint height,
        pixel_format format = pixel_format_rgba, uint8_t *pixels = nullptr);

    virtual ~image();
    
    static uint getBytesPerPixel(pixel_format format)
    {
        switch (format) {
            case pixel_format_none:            return 0;
            case pixel_format_alpha:           return 1;
            case pixel_format_rgb:             return 3;
            case pixel_format_rgba:            return 4;
            case pixel_format_argb:            return 4;
            case pixel_format_rgb555:          return 2;
            case pixel_format_rgb565:          return 2;
            case pixel_format_luminance:       return 1;
            case pixel_format_luminance_alpha: return 2;
            default:                           return 0;
        }
    }
    
    uint getBytesPerPixel() { return getBytesPerPixel(format); }
    uint getWidth() { return width; }
    uint getHeight() { return height; }
    uint8_t* getData() { return pixels; }
    pixel_format getpixel_format() { return format; }
    uint8_t* move() { ownData = false; return pixels; }
        
    void create(pixel_format format, uint width, uint height);
    void convertFormat(pixel_format newformat);
    
    static image_io* getImageIO(unsigned char magic[8]);
    static image_io* getImageIO(std::string filename);
    static image_io* getImageIO(file_ptr rsrc);
    static image_io* getImageIOFromExt(std::string pathname);
    static void saveToFile(std::string filename,
        const image_ptr &image, image_io *imageio = NULL);
    static image_ptr createBitmap(uint width, uint height,
        pixel_format format, uint8_t *pixels = NULL);
    static image_ptr createFromFile(std::string filename,
        image_io *imageio = NULL, pixel_format optformat = pixel_format_none);
    static image_ptr createFromResouce(std::string rsrcName,
        image_io *imageio = NULL, pixel_format optformat = pixel_format_none);
    static image_ptr createFromResouce(file_ptr rsrc,
        image_io *imageio = NULL, pixel_format optformat = pixel_format_none);
};

inline image::image() :
    iid(++iid_seq), rsrc(), width(0), height(0), format(pixel_format_none),
    pixels(nullptr), ownData(false) {}

inline image::image(const image &image) :
    iid(++iid_seq), rsrc(image.rsrc), width(image.width), height(image.height),
    format(image.format), ownData(false)
{
    create(format, width, height);
    memcpy(pixels, image.pixels, (size_t)width * height * getBytesPerPixel());
}

inline image::image(file_ptr rsrc, uint width, uint height,
    pixel_format format, uint8_t *pixels) : iid(++iid_seq), rsrc(rsrc),
    width(width), height(height), format(format), pixels(pixels), ownData(false)
{
    if (!pixels) {
        create(format, width, height);
    }
}

/* image_io */

struct image_io
{
public:
    virtual image* load(file_ptr rsrc, pixel_format optformat) = 0;
    virtual void save(image* image, std::string filename) = 0;
    
    virtual ~image_io() {}
};

struct image_io_png : public image_io
{
public:
    image* load(file_ptr rsrc, pixel_format optformat);
    void save(image* image, std::string filename);
};
