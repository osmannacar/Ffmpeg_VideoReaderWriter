#include "videoreader.h"
#include <qdebug.h>

VideoReader::VideoReader(QString pFilename)
    : mFilename(pFilename), mFps(0), mWidht(0), mHeight(0)
{
    av_register_all();
    av_log_set_callback(VideoReader::avLogCallBack);

}

VideoReader::~VideoReader()
{
    if(mDecframe != nullptr){
        av_frame_free(&mDecframe);
        mDecframe = nullptr;
    }
    if(mOutputframe != nullptr){
        av_frame_free(&mOutputframe);
        mOutputframe = nullptr;
    }
    if(mSwsctx != nullptr){
        sws_freeContext(mSwsctx);
        mSwsctx = nullptr;
    }
    if(mVideostrm != nullptr){
        avcodec_close(mVideostrm->codec);
        mVideostrm = nullptr;
    }
    if(mInctx != nullptr){
        avformat_close_input(&mInctx);
        mInctx = nullptr;
    }

}

bool VideoReader::init()
{
    // open input file context
    if((mRet = avformat_open_input(&mInctx, mFilename.toStdString().c_str(), nullptr, nullptr)) < 0){
        qDebug() << "fail to avforamt_open_input(\"" << mFilename << "\"): ret=" << mRet;
        return false;
    }

    // retrive input stream information
    if((mRet =  avformat_find_stream_info(mInctx, nullptr)) < 0){
        qDebug() << "fail to avformat_find_stream_info: mRet=" << mRet;
        return false;
    }

    // find primary video stream
    if((mRet = av_find_best_stream(mInctx, AVMEDIA_TYPE_VIDEO, -1, -1, &mVideocodec, 0)) < 0){
        qDebug() << "fail to av_find_best_stream: mRet=" << mRet;
        return false;
    }

    mVstrm_idx = mRet;
    mVideostrm = mInctx->streams[mVstrm_idx];

    // open video decoder context
    if((mRet = avcodec_open2(mVideostrm->codec, mVideocodec, nullptr))){
        qDebug() << "fail to avcodec_open2: mRet=" << mRet;
        return false;
    }

    // print input video stream informataion
    qDebug()
            << "infile: " << mFilename << "\n"
            << "format: " << mInctx->iformat->name << "\n"
            << "vcodec: " << mVideocodec->name << "\n"
            << "size:   " << (mWidht = mVideostrm->codec->width) << 'x' << (mHeight = mVideostrm->codec->height) << "\n"
            << "fps:    " << (mFps = av_q2d(mVideostrm->codec->framerate)) << " [fps]\n"
            << "length: " << av_rescale_q(mVideostrm->duration, mVideostrm->time_base, {1,1000}) / 1000. << " [sec]\n"
            << "pixfmt: " << av_get_pix_fmt_name(mVideostrm->codec->pix_fmt) << "\n"
            << "frame:  " << mVideostrm->nb_frames << "\n";

    mSwsctx = sws_getCachedContext(nullptr, mVideostrm->codec->width, mVideostrm->codec->height, mVideostrm->codec->pix_fmt,
                                   mWidht, mHeight, mDstPixFmt, SWS_BICUBIC, nullptr, nullptr, nullptr);

    if (mSwsctx == nullptr) {
        qDebug() << "fail to sws_getCachedContext";
        return false;
    }
    mOutputframe = av_frame_alloc();
    mFramebuf =  std::vector<uint8_t>(avpicture_get_size(mDstPixFmt, mWidht, mHeight));
    avpicture_fill(reinterpret_cast<AVPicture*>(mOutputframe), mFramebuf.data(), mDstPixFmt, mWidht, mHeight);

    mDecframe = av_frame_alloc();

    return true;
}

int VideoReader::widht() const
{
    return mWidht;
}

int VideoReader::height() const
{
    return mHeight;
}

double VideoReader::fps() const
{
    return mFps;
}

uint8_t *VideoReader::currentData() const
{
    return (uint8_t*)(mFramebuf.data());
}

bool VideoReader::readNext()
{
    bool hasNext = true;
    //read packet from input file
    mRet = av_read_frame(mInctx, &mPkt);
    if(mRet < 0 && mRet != AVERROR_EOF){
        qDebug() << "fail to av_read_frame: ret=" << mRet;
        hasNext = false;
    }else if (mRet == AVERROR_EOF) {
        qDebug() << "end file " << mRet;
        hasNext = false;
    }else {
        // decode video frame
        avcodec_decode_video2(mVideostrm->codec, mDecframe, &mGotPicturePtr, &mPkt);
        sws_scale(mSwsctx, mDecframe->data, mDecframe->linesize, 0, mDecframe->height, mOutputframe->data, mOutputframe->linesize);
        av_free_packet(&mPkt);
    }
    return hasNext;
}

void VideoReader::avLogCallBack(void *, int level, const char *fmt, va_list vargs)
{
    (void)level;
    static char message[8192];
    vsnprintf(message, sizeof(message), fmt, vargs); //vsnprintf_s
    qDebug() << message;
}
