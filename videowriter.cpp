#include "videowriter.h"


#define VIDEO_TMP_FILE "tmp.h264"

#include <QDebug>

VideoWriter::VideoWriter(const QString &pFileName)
    :mFileName(pFileName)
{

    mOformat = nullptr;
    mOfctx = nullptr;
    mVideoStream = nullptr;
    mVideoFrame = nullptr;
    mSwsCtx = nullptr;
    mFrameCounter = 0;

    // Initialize libavcodec
    av_register_all();
    av_log_set_callback(VideoWriter::avLogCallBack);
}

VideoWriter::~VideoWriter(){
    free();
}
bool VideoWriter::init(int pWidth, int pHeight, int pFpsrate, int pBitrate) {

    mFps = pFpsrate;

    int err;

    if (!(mOformat = av_guess_format(NULL, VIDEO_TMP_FILE, NULL))) {
        qDebug() << "Failed to define output format" << 0;
        return false;
    }

    if ((err = avformat_alloc_output_context2(&mOfctx, mOformat, NULL, VIDEO_TMP_FILE) < 0)) {
        qDebug() << "Failed to allocate output context" << err;
        free();
        return false;
    }

    if (!(mCodec = avcodec_find_encoder(mOformat->video_codec))) {
        qDebug() << "Failed to find encoder" << 0;
        free();
        return false;
    }

    if (!(mVideoStream = avformat_new_stream(mOfctx, mCodec))) {
        qDebug() << "Failed to create new stream" <<  0;
        free();
        return false;
    }

    if (!(mCctx = avcodec_alloc_context3(mCodec))) {
        qDebug() << "Failed to allocate codec context" << 0;
        free();
        return false;
    }

    mVideoStream->codecpar->codec_id = mOformat->video_codec;
    mVideoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    mVideoStream->codecpar->width = pWidth;
    mVideoStream->codecpar->height = pHeight;
    mVideoStream->codecpar->format = AV_PIX_FMT_YUV420P;
    mVideoStream->codecpar->bit_rate = pBitrate * 1000;
    mVideoStream->time_base = { 1, mFps };

    avcodec_parameters_to_context(mCctx, mVideoStream->codecpar);
    mCctx->time_base = { 1, mFps };
    mCctx->max_b_frames = 2;
    mCctx->gop_size = 12;
    if (mVideoStream->codecpar->codec_id == AV_CODEC_ID_H264) {
        av_opt_set(mCctx, "preset", "ultrafast", 0);
    }
    if (mOfctx->oformat->flags & AVFMT_GLOBALHEADER) {
        mCctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    avcodec_parameters_from_context(mVideoStream->codecpar, mCctx);

    if ((err = avcodec_open2(mCctx, mCodec, NULL)) < 0) {
        qDebug() << "Failed to open codec" << err;
        free();
        return false;
    }

    if (!(mOformat->flags & AVFMT_NOFILE)) {
        if ((err = avio_open(&mOfctx->pb, VIDEO_TMP_FILE, AVIO_FLAG_WRITE)) < 0) {
            qDebug() << "Failed to open file" << err;
            free();
            return false;
        }
    }

    if ((err = avformat_write_header(mOfctx, NULL)) < 0) {
        qDebug() << "Failed to write header" << err;
        free();
        return false;
    }

    av_dump_format(mOfctx, 0, VIDEO_TMP_FILE, 1);
    return true;
}

void VideoWriter::addFrame(uint8_t *data) {
    int err;
    if (!mVideoFrame) {

        mVideoFrame = av_frame_alloc();
        mVideoFrame->format = AV_PIX_FMT_YUV420P;
        mVideoFrame->width = mCctx->width;
        mVideoFrame->height = mCctx->height;

        if ((err = av_frame_get_buffer(mVideoFrame, 32)) < 0) {
            qDebug() << "Failed to allocate picture" << err;
            return;
        }
    }

    if (!mSwsCtx) {
        mSwsCtx = sws_getContext(mCctx->width, mCctx->height, AV_PIX_FMT_RGB24, mCctx->width, mCctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);
    }

    int inLinesize[1] = { 3 * mCctx->width };

    // From RGB to YUV
    sws_scale(mSwsCtx, (const uint8_t * const *)&data, inLinesize, 0, mCctx->height, mVideoFrame->data, mVideoFrame->linesize);

    mVideoFrame->pts = mFrameCounter++;

    if ((err = avcodec_send_frame(mCctx, mVideoFrame)) < 0) {
        qDebug() << "Failed to send frame" << err;
        return;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    if (avcodec_receive_packet(mCctx, &pkt) == 0) {
        pkt.flags |= AV_PKT_FLAG_KEY;
        av_interleaved_write_frame(mOfctx, &pkt);
        av_packet_unref(&pkt);
    }
}

bool VideoWriter::finish() {
    //DELAYED FRAMES
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    for (;;) {
        avcodec_send_frame(mCctx, NULL);
        if (avcodec_receive_packet(mCctx, &pkt) == 0) {
            av_interleaved_write_frame(mOfctx, &pkt);
            av_packet_unref(&pkt);
        }
        else {
            break;
        }
    }

    av_write_trailer(mOfctx);
    if (!(mOformat->flags & AVFMT_NOFILE)) {
        int err = avio_close(mOfctx->pb);
        if (err < 0) {
            qDebug() << "Failed to close file" << err;
        }
    }

    free();

    return remux();
}

void VideoWriter::avLogCallBack(void *, int level, const char *fmt, va_list vargs)
{
    (void)level;
    static char message[8192];
    vsnprintf(message, sizeof(message), fmt, vargs);
    qDebug() << message;
}

void VideoWriter::free() {
    if (mVideoFrame) {
        av_frame_free(&mVideoFrame);
        mVideoFrame = nullptr;
    }
    if (mCctx) {
        avcodec_free_context(&mCctx);
        mCctx = nullptr;
    }
    if (mOfctx) {
        avformat_free_context(mOfctx);
        mOfctx = nullptr;
    }
    if (mSwsCtx) {
        sws_freeContext(mSwsCtx);
        mSwsCtx = nullptr;
    }
}

bool VideoWriter::remux() {
    int err;
    if ((err = avformat_open_input(&mIfmt_ctx, VIDEO_TMP_FILE, 0, 0)) < 0) {
        qDebug() << "Failed to open input file for remuxing" << err;
        this->clearOutputContext();
        return false;
    }
    if ((err = avformat_find_stream_info(mIfmt_ctx, 0)) < 0) {
        qDebug() << "Failed to retrieve input stream information" << err;
        this->clearOutputContext();
        return false;
    }
    if ((err = avformat_alloc_output_context2(&mOfmt_ctx, NULL, NULL, mFileName.toStdString().c_str()))) {
        qDebug() << "Failed to allocate output context" << err;
        this->clearOutputContext();
        return false;
    }

    AVStream *inVideoStream = mIfmt_ctx->streams[0];
    AVStream *outVideoStream = avformat_new_stream(mOfmt_ctx, NULL);
    if (!outVideoStream) {
        qDebug() << "Failed to allocate output video stream" << 0;
        this->clearOutputContext();
        return false;
    }
    outVideoStream->time_base = { 1, mFps };
    avcodec_parameters_copy(outVideoStream->codecpar, inVideoStream->codecpar);
    outVideoStream->codecpar->codec_tag = 0;

    if (!(mOfmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((err = avio_open(&mOfmt_ctx->pb, mFileName.toStdString().c_str(), AVIO_FLAG_WRITE)) < 0) {
            qDebug() << "Failed to open output file" << err;
            this->clearOutputContext();
            return false;
        }
    }

    if ((err = avformat_write_header(mOfmt_ctx, 0)) < 0) {
        qDebug() << "Failed to write header to output file" << err;
        this->clearOutputContext();
        return false;
    }

    AVPacket videoPkt;
    int ts = 0;
    while (true) {
        if ((err = av_read_frame(mIfmt_ctx, &videoPkt)) < 0) {
            break;
        }
        videoPkt.stream_index = outVideoStream->index;
        videoPkt.pts = ts;
        videoPkt.dts = ts;
        videoPkt.duration = av_rescale_q(videoPkt.duration, inVideoStream->time_base, outVideoStream->time_base);
        ts += videoPkt.duration;
        videoPkt.pos = -1;

        if ((err = av_interleaved_write_frame(mOfmt_ctx, &videoPkt)) < 0) {
            qDebug() << "Failed to mux packet" << err;
            av_packet_unref(&videoPkt);
            break;
        }
        av_packet_unref(&videoPkt);
    }

    av_write_trailer(mOfmt_ctx);
    return true;
}

void VideoWriter::clearOutputContext()
{
    if (mIfmt_ctx) {
        avformat_close_input(&mIfmt_ctx);
        mIfmt_ctx = nullptr;
    }
    if (mOfmt_ctx && !(mOfmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&mOfmt_ctx->pb);
    }
    if (mOfmt_ctx) {
        avformat_free_context(mOfmt_ctx);
        mOfmt_ctx = nullptr;
    }
}
