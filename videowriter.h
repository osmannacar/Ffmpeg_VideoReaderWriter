#ifndef VIDEOWRITER_H
#define VIDEOWRITER_H

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>

// hwaccel
#include "libavcodec/vdpau.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_vdpau.h"

// lib swresample
#include <libswscale/swscale.h>
}


#include <QString>

class VideoWriter {
public:

    VideoWriter(const QString &pFileName);
    ~VideoWriter();

    bool init(int pWidth, int pHeight, int pFpsrate, int pBitrate);

    void addFrame(uint8_t *data);

    bool finish();

private:
     static void avLogCallBack(void *, int level, const char * fmt, va_list vargs);

    AVOutputFormat *mOformat = nullptr;
    AVFormatContext *mOfctx = nullptr;

    AVFormatContext *mIfmt_ctx = nullptr, *mOfmt_ctx = nullptr;

    AVStream *mVideoStream = nullptr;
    AVFrame *mVideoFrame = nullptr;

    AVCodec *mCodec = nullptr;
    AVCodecContext *mCctx = nullptr;

    SwsContext *mSwsCtx = nullptr;

    int mFrameCounter;

    int mFps;

    void free();

    bool remux();
    void clearOutputContext();

    QString mFileName;
};
#endif // VIDEOWRITER_H
