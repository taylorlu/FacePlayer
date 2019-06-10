//
//  mediaPipeline.h
//  FacePlayer
//
//  Created by LuDong on 2019/1/18.
//  Copyright © 2019年 LuDong. All rights reserved.
//

#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/avassert.h"
#include "libswscale/swscale.h"
#include "libavutil/pixfmt.h"
#include "libavformat/avformat.h"
#include "libavutil/samplefmt.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include "mediaPipeline.h"

AVFormatContext *pFormatCtx;
AVCodecContext *pVideoCodecCtx;
AVCodecContext *pAudioCodecCtx;
int video_stream_index;
int audio_stream_index;
uint8_t *blendBuffer;
uint8_t *rgbBuffer;
AVFormatContext *ofmt_ctx;
struct SwsContext *yuv_rgb_ctx;
struct SwsContext *rgb_yuv_ctx;
uint8_t *rgbFrameBuffer;
uint8_t *yuvFrameBuffer;

int initModule(const char *filename, const char *outputFile, uint64_t *nb_frames, int *w, int *h) {
    
    av_register_all();
    avformat_network_init();
    
    blendBuffer = NULL;
    rgbBuffer = NULL;
    yuv_rgb_ctx = NULL;
    rgb_yuv_ctx = NULL;
    
    int ret;
    AVCodec *dec;
    
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "probesize", "60000000", 0);
    av_dict_set(&opts, "analyzeduration", "60000000", 0);
    
    if ((ret = avformat_open_input(&pFormatCtx, filename, NULL, &opts)) < 0) {
        printf("Cannot open input \"%s\"\n", filename);
        return ret;
    }
    
    if ((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0) {
        printf("Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        printf("Cannot find a video stream in the input file\n");
        return ret;
    }
    video_stream_index = ret;
    pVideoCodecCtx = pFormatCtx->streams[video_stream_index]->codec;
    *w = pVideoCodecCtx->width;
    *h = pVideoCodecCtx->height;
    *nb_frames = pFormatCtx->streams[video_stream_index]->nb_frames;

    /* init the video decoder */
    if ((ret = avcodec_open2(pVideoCodecCtx, dec, NULL)) < 0) {
        printf("Cannot open video decoder\n");
        return ret;
    }

    /* select the audio stream */
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
    if (ret < 0) {
        printf("Cannot find a audio stream in the input file\n");
        return ret;
    }
    audio_stream_index = ret;

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, outputFile);
    if (!ofmt_ctx) {
        return AVERROR_UNKNOWN;
    }

    AVRational muxTimeBase;
    muxTimeBase.num = 1;
    muxTimeBase.den = 90000;
    
    AVCodec *videoEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if(!videoEncoder) {
        return AVERROR_UNKNOWN;
    }
    AVStream *videoOutStream = avformat_new_stream(ofmt_ctx, videoEncoder);
    videoOutStream->time_base = muxTimeBase;
    AVCodecContext *videoEnc_ctx = videoOutStream->codec;
    videoEnc_ctx->bit_rate = pVideoCodecCtx->bit_rate;
    videoEnc_ctx->width = pVideoCodecCtx->width;
    videoEnc_ctx->height = pVideoCodecCtx->height;
    videoEnc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    videoEnc_ctx->time_base = pVideoCodecCtx->time_base;
    videoEnc_ctx->max_b_frames = 0;
        videoEnc_ctx->profile = FF_PROFILE_H264_MAIN;
    videoEnc_ctx->gop_size = 25;
    videoEnc_ctx->b_frame_strategy = 0;
    
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        videoEnc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    videoEnc_ctx->codec_tag = 0;
    
    ret = avcodec_open2(videoEnc_ctx, videoEncoder, NULL);
    if (ret < 0) {
        return ret;
    }
    
    rgbFrameBuffer = (uint8_t *)malloc(pVideoCodecCtx->width*pVideoCodecCtx->height*3);
    yuvFrameBuffer = (uint8_t *)malloc(pVideoCodecCtx->width*pVideoCodecCtx->height*3/2);
    
    AVStream *audioOutStream = avformat_new_stream(ofmt_ctx, NULL);
    audioOutStream->time_base = muxTimeBase;
    
    AVCodecContext *audioEnc_ctx = audioOutStream->codec;
    ret = avcodec_copy_context(audioEnc_ctx, pFormatCtx->streams[audio_stream_index]->codec);
    if (ret < 0) {
        return ret;
    }
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        audioEnc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    audioEnc_ctx->codec_tag = 0;
    
    if (!(ofmt_ctx->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, outputFile, AVIO_FLAG_WRITE);
        if (ret < 0) {
            return ret;
        }
    }
    
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

void yuv2rgb(AVFrame *src, AVFrame *dst) {
    
    if (yuv_rgb_ctx == NULL) {
        yuv_rgb_ctx = sws_alloc_context();
        av_opt_set_int(yuv_rgb_ctx, "sws_flags", SWS_FAST_BILINEAR | SWS_PRINT_INFO, 0);
        av_opt_set_int(yuv_rgb_ctx, "srcw", src->width, 0);
        av_opt_set_int(yuv_rgb_ctx, "srch", src->height, 0);
        av_opt_set_int(yuv_rgb_ctx, "src_format", src->format, 0);
        av_opt_set_int(yuv_rgb_ctx, "dstw", dst->width, 0);
        av_opt_set_int(yuv_rgb_ctx, "dsth", dst->height, 0);
        av_opt_set_int(yuv_rgb_ctx, "dst_format", dst->format, 0);
        
        sws_init_context(yuv_rgb_ctx, NULL, NULL);
    }
    sws_scale(yuv_rgb_ctx, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
}

void rgb2yuv(AVFrame *src, AVFrame *dst) {
    
    if (rgb_yuv_ctx == NULL) {
        rgb_yuv_ctx = sws_alloc_context();
        av_opt_set_int(rgb_yuv_ctx, "sws_flags", SWS_FAST_BILINEAR | SWS_PRINT_INFO, 0);
        av_opt_set_int(rgb_yuv_ctx, "srcw", src->width, 0);
        av_opt_set_int(rgb_yuv_ctx, "srch", src->height, 0);
        av_opt_set_int(rgb_yuv_ctx, "src_format", src->format, 0);
        av_opt_set_int(rgb_yuv_ctx, "dstw", dst->width, 0);
        av_opt_set_int(rgb_yuv_ctx, "dsth", dst->height, 0);
        av_opt_set_int(rgb_yuv_ctx, "dst_format", dst->format, 0);
        
        sws_init_context(rgb_yuv_ctx, NULL, NULL);
    }
    sws_scale(rgb_yuv_ctx, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
}

void *pushCycle(void *opaque) {

    AVPacket packet;
    int got_frame;
    int ret = 0;
    int frameCount = 0;
    while (1) {
        
        av_init_packet(&packet);
        packet.data = NULL;
        packet.size = 0;
        ret = av_read_frame(pFormatCtx, &packet);
        if (ret < 0)
            break;
        
        int64_t start_time = pFormatCtx->streams[packet.stream_index]->start_time;
        if (packet.dts != AV_NOPTS_VALUE && start_time != AV_NOPTS_VALUE)
            packet.dts -= start_time;
        if (packet.pts != AV_NOPTS_VALUE && start_time != AV_NOPTS_VALUE)
            packet.pts -= start_time;
        
        if (packet.stream_index == video_stream_index) {
            
            got_frame = 0;
            AVFrame *pFrame = av_frame_alloc();
            
            av_packet_rescale_ts(&packet, pFormatCtx->streams[video_stream_index]->time_base, pVideoCodecCtx->time_base);
            ret = avcodec_decode_video2(pVideoCodecCtx, pFrame, &got_frame, &packet);
            if (ret < 0 || !got_frame) {
                continue;
            }
            pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
            
            /************************************************************************/
            /*                                                                      */
            /************************************************************************/
            
            AVFrame* rgbFrame = av_frame_alloc();
            rgbFrame->width = pVideoCodecCtx->width;
            rgbFrame->height = pVideoCodecCtx->height;
            rgbFrame->format = AV_PIX_FMT_RGB24;
            avpicture_fill((AVPicture *)rgbFrame, rgbFrameBuffer, rgbFrame->format, rgbFrame->width, rgbFrame->height);
            
            yuv2rgb(pFrame, rgbFrame);
            
            int faceCount = RenderFace(opaque, rgbFrameBuffer, frameCount++);
            printf("faceCount = %d, frame = %d\n", faceCount, frameCount);
//            if(frameCount>100){
//                break;
//            }

            AVFrame* yuvFrame = av_frame_alloc();
            yuvFrame->width = pVideoCodecCtx->width;
            yuvFrame->height = pVideoCodecCtx->height;
            yuvFrame->format = AV_PIX_FMT_YUV420P;
            avpicture_fill((AVPicture *)yuvFrame, yuvFrameBuffer, yuvFrame->format, yuvFrame->width, yuvFrame->height);
            
            rgb2yuv(rgbFrame, yuvFrame);
            yuvFrame->pts = pFrame->pts;
            yuvFrame->pkt_dts = pFrame->pkt_dts;
            yuvFrame->pkt_pts = pFrame->pkt_pts;
            
            av_frame_free(&pFrame);
            av_frame_free(&rgbFrame);
            
            AVPacket enc_pkt;
            av_init_packet(&enc_pkt);
            enc_pkt.data = NULL;
            enc_pkt.size = 0;
            ret = avcodec_encode_video2(ofmt_ctx->streams[packet.stream_index]->codec, &enc_pkt, yuvFrame, &got_frame);
            if (ret < 0 || !got_frame)
                continue;
            
            av_frame_free(&yuvFrame);
            
            /* prepare packet for muxing */
            av_packet_rescale_ts(&enc_pkt,
                                 ofmt_ctx->streams[packet.stream_index]->codec->time_base,
                                 ofmt_ctx->streams[packet.stream_index]->time_base);
            
            av_copy_packet(&packet, &enc_pkt);
            av_free_packet(&enc_pkt);

            /************************************************************************/
            /*                                                                      */
            /************************************************************************/
            
        }
        else {
            av_packet_rescale_ts(&packet,
                                 pFormatCtx->streams[packet.stream_index]->time_base,
                                 ofmt_ctx->streams[packet.stream_index]->time_base);
        }
        
        /* mux encoded frame */
        ret = av_interleaved_write_frame(ofmt_ctx, &packet);
        if(ret<0) {
            break;
        }
    }
    av_write_trailer(ofmt_ctx);
    avformat_close_input(&pFormatCtx);
    avformat_close_input(&ofmt_ctx);
    renderFinish(opaque);
    
    printf("avformat_close_input....\n");
    return NULL;
}

