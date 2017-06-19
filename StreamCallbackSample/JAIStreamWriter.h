#pragma once

#include "stdafx.h"

#include <iostream>
#include <vector>
#include <Jai_Factory.h>
#include "JpegEncoder.h"

typedef enum { MSG_DEBUG = 0, MSG_TRACE, MSG_INFO, MSG_WARNING, MSG_ERROR } MessageLevel;

// JAIStreamWrite performs image convertion in parallel and writes the resulting JPEG images in AVI file
class JAIStreamWriter {
public:
    JAIStreamWriter(LPCWSTR sFileName, LPCWSTR sFileNameForExtraFrames);
    ~JAIStreamWriter();
    
    BOOL setupStreamParameters(long width, long height, int quality); // Stream dimentions and JPEG quality [0-100]
    BOOL addFrame(J_tIMAGE_INFO* ptRawImageInfo); // Convert JAI internal format into JPEG and save 
    BOOL stop();

    // Access to encoder's statistics
    double getRate(double *elapsed_time_sec=NULL) const; // Get frames rate, i.e. number of frames per second. If elspased_time_sec is not NULL, then it is updated
    unsigned long framesProcessed() const { return m_FramesCount; }
    unsigned long framesWritten() const { return m_nFramesWritten; }
    unsigned long framesDropped() const { return m_nFailures; }
    unsigned long frmaesInverted() const { return m_nInversions; }
private:
    const int m_nEncoderThreads = 3; // Number of parallel encoders
    const unsigned long m_outputBufferSize = 1024 * 1024 * 40; // Maximum size of each JPEG image = 40M
    const unsigned int  m_encoderTimeoutMS = 1; /* How many miliseconds encode method waits for empty encoder */

    // Information about JPEG images
    unsigned int m_width;
    unsigned int m_height;
    unsigned int m_quality;

    AVIWriter    m_aviWriter;
    AVIWriter    m_aviWriter_extra; // AVI stream for outdated frames that were not included into the main stream
    std::wofstream   m_log_file;   // File for output logging information

    // Variables used by frames replacement methods. If all encoders are busy last encoded frame is duplicated appropriate number of times
    unsigned int m_iLastJPEGFrameNumber; // Frame number of last saved JPEG image
    unsigned int m_iLastEncodedFrame;  // Index of last frame submitted for encoding
    unsigned int m_iFrameToDuplicate;   // Which frame have to be duplicated due to impossibility of proper encoding
    unsigned int m_iDuplicationCount;   // How many times this frame should be repeated
    unsigned int m_iSubstitutionsCount; // Number of frames written as a replacement of missing ones

    struct EncoderInfo {
        JpegEncoder *enc;
        int id;
        HANDLE hSrcDataReadyEvent; /* Event indicating that new image is ready for encoding */
        HANDLE hJpegReadyEvent;    /* Event indicating that jpeg is ready */
        unsigned char *mem;
        unsigned long mem_size;
        HANDLE hThread;
        JAIStreamWriter *pStreamWriter;
        // Description of current job. Updated when new frame arrives.
        JSAMPLE *src;
        int width;
        int height;
        unsigned long frame_number;
    };

    SYSTEMTIME m_t0;  // Timestamp of first frame
    unsigned long m_FramesCount; // Number of frames processed
    unsigned long m_nFramesWritten; // Number of frames written to the output avi file
    unsigned long m_nFailures; // Number of frames dropped
    unsigned long m_nInversions; // Number of frames written later they have to
    LPCWSTR m_StreamFilename; // Output filename
    std::vector<J_tIMAGE_INFO> m_convertedImages;

    EncoderInfo   *m_encoders;
    HANDLE        *m_hJpegReadyEvents; // Vector of events. True, if corresponding thread wrote its data to the stream and ready for new job 
    CRITICAL_SECTION m_csOutput; // Serialize output of JPEG images into avi stream
    CRITICAL_SECTION m_csLogging; // Serialize output of log file


    int JAIStreamWriter::getFreeThread();
    void submitEncodingJob(int encoderIndex, JSAMPLE *src, int width, int height);
    void writeJpegImage(unsigned long frame_number, unsigned char *mem, unsigned long image_size);
    static DWORD process(EncoderInfo *encinfo); // Thread entry point

    double secondsFromStart() const;
    void log_message(MessageLevel level, LPCWSTR message_format, ...); // Write the message to log file using CString.Format
};
