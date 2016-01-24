#ifndef __AVI_WRITER_H__
#define __AVI_WRITER_H__

#include <mmc.h>
#include <cstdio>
#include <list>
#include <vector>

#include "avifmt.h"

struct FrameData
{
	FrameData(DWORD _size, DWORD _offset) : size(_size), offset(_offset) {}
	DWORD size;  // size of the frame in bytes
	DWORD offset; // offset from the beginning of stream
};

typedef unsigned long off64_t;

const off64_t MAX_RIFF_SZ = 2147483648L; // 2 GB limit

class AVIHeader {
public:
	bool readHeader(FILE *avi_file);
	bool writeHeader(FILE *avi_file);

	void setRate(int frames, int seconds);
	void setWidth(int width);
	void setHeight(int height);

	int width() const { return m_hdr.hdrl.avih.width; }
	int height() const { return m_hdr.hdrl.avih.height; }
	int totalFrames() const { return m_hdr.hdrl.avih.tot_frames; }

private:
	struct AVI_file_header m_hdr;
};


class AVIReader {
public:
	AVIReader(LPCWSTR inputFileName);
	~AVIReader();
	bool readFrame(std::vector<unsigned char>& data);
	AVIHeader &header() { return m_aviHeader; }
private:
	FILE *m_aviFile;
	AVIHeader m_aviHeader;
};

class AVIWriter {
public:
	AVIWriter(LPCWSTR outputFileName);
	~AVIWriter();
	void start(int width, int height, int fps=25);
	bool addFrame(const unsigned char *buf, unsigned long size);
	bool finalize();
private:
	bool writeHeader();
	bool initialize();
	bool switchToNewFile();
	inline off64_t bytesRemained() { return MAX_RIFF_SZ - m_tnbw - 1024*20; } // Return approximate number of bytes before 2GB limit

	FILE *out;
	unsigned long m_frameCount;     // number of frames written
	unsigned long m_tnbw;           // total number of bytes written
	unsigned long m_jpg_sz_64;
	unsigned long m_riff_sz_64;
	int m_width;                    // image size
	int m_height;
	int m_fps;
	bool m_finalized;
	LPCWSTR m_baseFilename;   // All files are named by appending _nnn.avi to this prefix
	int m_fileSequenceNumber; // Sequence number of current output file
	std::list<FrameData> m_framesList;
	typedef std::list<FrameData>::iterator FrameDataIt;
};

#endif
