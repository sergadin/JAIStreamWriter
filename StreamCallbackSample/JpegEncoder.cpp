#include "stdafx.h"

#include "JpegEncoder.h"
#include "avi_writer.h"

unsigned long JpegEncoder::encode(JSAMPLE *src, int width, int height, unsigned char **mem, unsigned long *mem_size) {
    // src is an array of (m_cinfo.input_components = 3) * m_cinfo.image_width * m_cinfo.image_height values
    jpeg_mem_dest(&m_cinfo, mem, mem_size);

    jpeg_start_compress(&m_cinfo, TRUE);

    JSAMPROW row_pointer[1];
    int row_stride = m_cinfo.image_width * m_cinfo.input_components; /* JSAMPLEs per row in image_buffer */
    for (unsigned int next_scanline = 0; next_scanline < m_cinfo.image_height; next_scanline++) {
        row_pointer[0] = &src[next_scanline * row_stride];
        jpeg_write_scanlines(&m_cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&m_cinfo);

    return *mem_size;
}

void JpegEncoder::setupStreamParameters(unsigned int width, unsigned int height, int quality) {
    m_cinfo.image_width = width; 	/* image width and height, in pixels */
    m_cinfo.image_height = height;
    m_cinfo.input_components = 3;		/* # of color components per pixel */
    m_cinfo.in_color_space = JCS_EXT_BGR; 	/* colorspace of input image */
    jpeg_set_defaults(&m_cinfo);
    jpeg_set_quality(&m_cinfo, quality, TRUE /* limit to baseline-JPEG values */);
}

