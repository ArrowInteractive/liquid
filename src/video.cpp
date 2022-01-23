#include "video.hpp"

void load_video_frame(datastruct* datastate)
{
    while(av_read_frame(datastate->video_format_ctx, datastate->video_packet) >= 0)
    {
        if(datastate->video_packet->stream_index == datastate->video_stream_index)
        {
            datastate->response = avcodec_send_packet(datastate->video_codec_ctx, datastate->video_packet);
            if(datastate->response < 0){
                cout<<"Failed to decode packet!"<<endl;
                break;
            }

            if(datastate->sws_ctx == NULL)
            {
                /*
                    If sws_ctx is not set to NULL, it causes a seg fault on Linux based systems
                    when running sws_scale()
                    Setup sws_context here and send the decoded frame to renderer using decoded_frame
                    May cause crashes
                */
                datastate->sws_ctx = sws_getContext(   
                    datastate->video_codec_ctx->width, datastate->video_codec_ctx->height, datastate->video_codec_ctx->pix_fmt,
                    datastate->t_width, datastate->t_height, AV_PIX_FMT_YUV420P,
                    SWS_LANCZOS, NULL, NULL, NULL
                );
            }

            datastate->response = avcodec_receive_frame(datastate->video_codec_ctx, datastate->video_frame);
            if(datastate->response == AVERROR(EAGAIN))
            {
                av_packet_unref(datastate->video_packet);
                continue;
            }
            else if(datastate->response == AVERROR_EOF)
            {
                av_packet_unref(datastate->video_packet);
                break;
            }
            else if(datastate->response < 0)
            {
                cout<<"Failed to receive frame!"<<endl;
                av_packet_unref(datastate->video_packet);
                av_frame_unref(datastate->video_frame);
                break;
            }
            av_packet_unref(datastate->video_packet);
        }
        else
        {
            av_packet_unref(datastate->video_packet);
            av_frame_unref(datastate->video_frame);
            continue;
        }

        /* If End of File, then stop processing frames */
        if(datastate->response != AVERROR_EOF)
        {
            /* Scale the image and send it via decoded_frame */
            sws_scale(  
                datastate->sws_ctx,
                (uint8_t const * const *)datastate->video_frame->data,
                datastate->video_frame->linesize,
                0,
                datastate->video_frame->height,
                datastate->decoded_video_frame->data,
                datastate->decoded_video_frame->linesize
            );
        }
        av_frame_unref(datastate->video_frame);
        break;
    }
}