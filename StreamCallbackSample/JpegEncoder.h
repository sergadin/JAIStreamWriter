// Classes related to JPEG encoding

#pragma once

#include "stdafx.h"

#include <map>
#include <cstdlib>

#include <jpeglib.h>

#include "avi_writer.h"

typedef unsigned int image_dimention;

class JpegEncoder {
public:
    JpegEncoder(image_dimention width = 1024, image_dimention height = 768, int quality = 90) {
        m_cinfo.err = jpeg_std_error(&m_jerr);

        /* Now we can initialize the JPEG compression object. */
        jpeg_create_compress(&m_cinfo);

        // Setup default parameter
        setupStreamParameters(width, height, quality);
    }
    ~JpegEncoder() {
        /* This is an important step since it will release a good deal of memory. */
        jpeg_destroy_compress(&m_cinfo);
    }

    void setupStreamParameters(unsigned int width, unsigned int height, int quality);
    unsigned long encode(JSAMPLE *src, int width, int height, unsigned char **mem, unsigned long *mem_size);

private:
    jpeg_compress_struct m_cinfo;
    jpeg_error_mgr m_jerr;
};

