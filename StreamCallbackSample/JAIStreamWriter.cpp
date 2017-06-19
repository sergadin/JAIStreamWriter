#include "stdafx.h"

#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <Jai_Factory.h>
#include <jpeglib.h>

#include "JAIStreamWriter.h"
#include "JpegEncoder.h"


const int max_image_width = 3000;
const int max_image_height = 2500;
const int color_planes = 3;

JAIStreamWriter::JAIStreamWriter(LPCWSTR sFileName, LPCWSTR sFileNameForExtra)
    : m_aviWriter(sFileName),
    m_aviWriter_extra(sFileNameForExtra),
    m_convertedImages(m_nEncoderThreads)
{
    m_FramesCount = m_nFramesWritten = 0;
    m_nInversions = m_nFailures = 0;

    m_iLastEncodedFrame = 0;
    m_iLastJPEGFrameNumber = 0;
    m_iFrameToDuplicate = 0;
    m_iDuplicationCount = 0;
    m_iSubstitutionsCount = 0;
    m_StreamFilename = sFileName; 

    GetSystemTime(&m_t0);

    InitializeCriticalSection(&m_csOutput);
    InitializeCriticalSection(&m_csLogging);

    m_log_file.open("STREAM.LOG", std::ios::binary | std::ios::out);
    log_message(MSG_TRACE, _T("Starting JAIStreamWriter."));

    m_hJpegReadyEvents = new HANDLE[m_nEncoderThreads];
    m_encoders = new EncoderInfo[m_nEncoderThreads];
    for (int k = 0; k < m_nEncoderThreads; k++) {
        m_encoders[k].id = k;
        m_encoders[k].enc = new JpegEncoder();
        m_encoders[k].hSrcDataReadyEvent = CreateEvent(NULL, FALSE /* use manual reset */, FALSE /* initial value */, NULL);
        m_encoders[k].hJpegReadyEvent = CreateEvent(NULL, FALSE /* use manual reset */, TRUE /* initial value */, NULL);
        m_encoders[k].mem = new unsigned char[m_outputBufferSize];
        m_encoders[k].mem_size = m_outputBufferSize;
        m_encoders[k].pStreamWriter = this;
        m_encoders[k].hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)process, &m_encoders[k], NULL, NULL);
        // Copy event handle for dedicated array compatible with WaitForMultipleObjects.
        m_hJpegReadyEvents[k] = m_encoders[k].hJpegReadyEvent;
    }

    // Allocate the buffer to hold converted the image
    for (int k = 0; k < m_nEncoderThreads; k++) {
        J_tIMAGE_INFO dummy;
        dummy.iSizeX = max_image_width;
        dummy.iSizeY = max_image_height;
        dummy.iImageSize = max_image_width * max_image_height * color_planes;
        dummy.iPixelType = PF_24BIT_SWAP_RB;
        dummy.iOffsetX = dummy.iOffsetY = 0;
        //if (J_Image_Malloc(&dummy, &m_convertedImages[k]) != J_ST_SUCCESS) {
        //    log_message(MSG_ERROR, _T("Unable to allocate memory for converted image for encoding thread %d."), k);        
        //}
    }
}


JAIStreamWriter::~JAIStreamWriter() {
    for (int k = 0; k < m_nEncoderThreads; k++) {
        // Free up the image buffer
        if (J_Image_Free(&m_convertedImages[k]) != J_ST_SUCCESS) {
            log_message(MSG_ERROR, _T("Unable to free memory for converted image for encoding thread %d."), k);
        }
    }
    m_log_file.close();
}


// Converts a raw image, encoded it by JPEG encoder and saved the result to an avi-file
BOOL JAIStreamWriter::addFrame(J_tIMAGE_INFO* ptRawImageInfo)
{
    ++m_FramesCount;

    // JAI memory allocation functions require pointer to raw image info, so this initialization happens when the first frame arrives. 
    if (m_FramesCount == 1) {
        log_message(MSG_TRACE, _T("First frame arrived, resetting local time..."));
        GetSystemTime(&m_t0);

        // Allocate the buffer to hold converted the image
        for (int k = 0; k < m_nEncoderThreads; k++) {
            if (J_Image_Malloc(ptRawImageInfo, &m_convertedImages[k]) != J_ST_SUCCESS) {
                log_message(MSG_ERROR, _T("Unable to allocate memory for converted image for encoding thread %d."), k);
                return FALSE;
            }
        }

        m_aviWriter.start(m_width, m_height);
    }

    int encoderIndex = getFreeThread();
    if (encoderIndex < 0) {
        log_message(MSG_ERROR, _T("All encoders are busy."));
        goto failure;
    }

    J_tIMAGE_INFO& CnvImageInfo = m_convertedImages[encoderIndex];

    // Convert the JAI raw image to image format
    if (J_Image_FromRawToImage(ptRawImageInfo, &CnvImageInfo) != J_ST_SUCCESS) {
        log_message(MSG_ERROR, _T("Unable to covert JAI raw image."));
        goto failure;
    }

    int image_width = min(CnvImageInfo.iSizeX, max_image_width);
    int image_height = min(CnvImageInfo.iSizeY, max_image_height);

    if (!(CnvImageInfo.iPixelType == J_GVSP_PIX_BGR8_PACKED || CnvImageInfo.iPixelType == J_PF_BGR8_PACKED)) {
        log_message(MSG_ERROR, _T("Unsupported JAI pixel format: %d"), CnvImageInfo.iPixelType);
        goto failure;
    }

    if (CnvImageInfo.iImageSize != image_height*image_width * color_planes) {
        log_message(MSG_ERROR, _T("Something wrong, image size mismatch."));
        goto failure;
    }

    submitEncodingJob(encoderIndex, CnvImageInfo.pImageBuffer, image_width, image_height);
    m_iLastEncodedFrame = m_FramesCount;
    return TRUE;
failure:
    log_message(MSG_ERROR, _T("*** FRAME %d will be skipped ***"), m_FramesCount);
    ++m_nFailures;
    return FALSE;
}

double JAIStreamWriter::secondsFromStart() const {
    SYSTEMTIME t1;
    GetSystemTime(&t1);

    FILETIME ft0, ft1;
    __int64 t0_100nano, t1_100nano;
    SystemTimeToFileTime(&m_t0, &ft0);
    SystemTimeToFileTime(&t1, &ft1);

    ULARGE_INTEGER v_ui;
    v_ui.LowPart = ft0.dwLowDateTime;
    v_ui.HighPart = ft0.dwHighDateTime;
    t0_100nano = v_ui.QuadPart;

    v_ui.LowPart = ft1.dwLowDateTime;
    v_ui.HighPart = ft1.dwHighDateTime;
    t1_100nano = v_ui.QuadPart;

    double time_interval_sec = (t1_100nano - t0_100nano) / (10.0 * 1000.0 * 1000.0);
    return time_interval_sec;
}

double JAIStreamWriter::getRate(double *elapsed_time_sec /* =NULL */) const {
    double time_intrval_sec = secondsFromStart();
    double fps = (m_FramesCount) / time_intrval_sec;

    if (elapsed_time_sec) {
        *elapsed_time_sec = time_intrval_sec;
    }
    return fps;
}

BOOL JAIStreamWriter::setupStreamParameters(long width, long height, int quality) {
    m_width = width;
    m_height = height;
    m_quality = quality;
    log_message(MSG_INFO, _T("Setting up stream parameters: image size = %ld by %ld, JPEG quality = %d"), width, height, quality);
    for (int k = 0; k < m_nEncoderThreads; k++) {
        m_encoders[k].enc->setupStreamParameters(width, height, quality);
    }
    return TRUE;
}


BOOL JAIStreamWriter::stop() {
    log_message(MSG_TRACE, _T("Stop recording. %d frames processed; %d frames replaced; %d outdated frames stord in extra AVI file."),
        m_FramesCount, m_iSubstitutionsCount, m_nInversions);
    return TRUE;
}



void JAIStreamWriter::submitEncodingJob(int encoderIndex, JSAMPLE *src, int width, int height) {
    EncoderInfo& freeEncoderInfo = m_encoders[encoderIndex];
    freeEncoderInfo.width = width;
    freeEncoderInfo.height = height;
    freeEncoderInfo.src = src;
    freeEncoderInfo.frame_number = m_FramesCount;

    SetEvent(freeEncoderInfo.hSrcDataReadyEvent);
}

DWORD JAIStreamWriter::process(JAIStreamWriter::EncoderInfo *encinfo) {
    while (1) {
        WaitForSingleObject(encinfo->hSrcDataReadyEvent, INFINITE);
        unsigned char *mem = encinfo->mem;
        unsigned long jpeg_size = encinfo->mem_size;
        encinfo->enc->encode(encinfo->src, encinfo->width, encinfo->height, &mem, &jpeg_size);
        encinfo->pStreamWriter->writeJpegImage(encinfo->frame_number, mem, jpeg_size);

        SetEvent(encinfo->hJpegReadyEvent);
    }
    return 0;
}


int JAIStreamWriter::getFreeThread() {
    DWORD eventID = WaitForMultipleObjects(m_nEncoderThreads, m_hJpegReadyEvents, FALSE /* Wait All? */, m_encoderTimeoutMS);
    if (eventID >= WAIT_OBJECT_0 && eventID < WAIT_OBJECT_0 + m_nEncoderThreads) {
        return eventID - WAIT_OBJECT_0;
    }
    return -1;
}

void JAIStreamWriter::writeJpegImage(unsigned long frame_number, unsigned char *mem, unsigned long image_size) {
    EnterCriticalSection(&m_csOutput);
    if (frame_number > m_iLastJPEGFrameNumber) {
        int repetitions = frame_number - m_iLastJPEGFrameNumber;
        m_iSubstitutionsCount += (repetitions - 1);
        while (repetitions-- > 0) {
            if (repetitions == 0)
                log_message(MSG_TRACE, _T("Writing frame # %d."), frame_number);
            else
                log_message(MSG_TRACE, _T("Writing frame # %d as a replacement of missing frame."), frame_number);
            m_aviWriter.addFrame(reinterpret_cast<const unsigned char *>(mem), image_size);
            m_nFramesWritten++;
        }
        m_iLastJPEGFrameNumber = frame_number;
    }
    else {
        //  Later frame was encoded faster. This situation should be avoided by some means
        log_message(MSG_TRACE, _T("Outdated frame # %d SKIPPED; added as frame # %d into extra avi."), frame_number, m_nInversions);
        m_aviWriter_extra.addFrame(reinterpret_cast<const unsigned char *>(mem), image_size);
        m_nInversions++;
    }
    LeaveCriticalSection(&m_csOutput);
}


void JAIStreamWriter::log_message(MessageLevel level, LPCWSTR message, ...)
{
    va_list format_args;
    CString msg;
    va_start(format_args, message);
    msg.FormatV(message, format_args);
    va_end(format_args);

    double timestamp = secondsFromStart();
    char timestamp_str[128];
    sprintf(timestamp_str, "%.4f: ", timestamp);
    EnterCriticalSection(&m_csLogging);
    m_log_file << timestamp_str;
    m_log_file << msg.GetString();
    m_log_file << std::endl;
    //MessageBox(msg, _T("Error message"), MB_ICONERROR | MB_OK);
    LeaveCriticalSection(&m_csLogging);
}
