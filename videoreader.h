#ifndef VIDEOREADER_H
#define VIDEOREADER_H

// FFmpeg
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <QString>
#include <vector>

class VideoReader
{
public:
    VideoReader(QString pFilename);
    ~VideoReader();
    bool init();
    int widht() const;
    int height() const;
    double fps() const;
    uint8_t *currentData() const;
    bool readNext();

private:
    static void avLogCallBack(void *, int level, const char * fmt, va_list vargs);

    QString mFilename;
    AVFormatContext* mInctx = nullptr;
    AVCodec* mVideocodec = nullptr;
    AVStream* mVideostrm  = nullptr;
    const AVPixelFormat mDstPixFmt = AV_PIX_FMT_RGB24;
    SwsContext* mSwsctx = nullptr;
    AVFrame* mOutputframe = nullptr, *mDecframe = nullptr;
    AVPacket mPkt;

    std::vector<uint8_t> mFramebuf;

    int mRet;
    int mVstrm_idx;
    double mFps;
    int mWidht;
    int mHeight;
    int mGotPicturePtr = 0;
};

#endif // VIDEOREADER_H
