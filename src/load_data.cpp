#include "load_data.hpp"

bool load_data(char* filename, datastruct* datastate)
{
    if(!(datastate->av_format_ctx = avformat_alloc_context())){
        cout<<"ERROR: Could not setup AVFormatContext!"<<endl;
        return false;
    }

    if(avformat_open_input(&datastate->av_format_ctx, filename, NULL, NULL) != 0){
        cout<<"ERROR: Could not open file!"<<endl;
        return false;
    }

    for(int i=0; i < datastate->av_format_ctx->nb_streams; i++){
        if(datastate->av_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            datastate->av_codec_params = datastate->av_format_ctx->streams[i]->codecpar;
            datastate->av_codec = avcodec_find_decoder(datastate->av_codec_params->codec_id);
            datastate->video_stream_index = i;
            break;
        }
    }

    if(datastate->video_stream_index == -1 && datastate->audio_stream_index == -1){
        cout<<"ERROR: Could not find any streams in the file!"<<endl;
        return false;
    }

    if(!(datastate->av_codec_ctx = avcodec_alloc_context3(datastate->av_codec))){
        cout<<"ERROR: Could not setup AVCodecContext!"<<endl;
        return false;
    }

    if(avcodec_parameters_to_context(datastate->av_codec_ctx, datastate->av_codec_params) < 0){
        cout<<"ERROR: Could not pass the parameters to AVCodecContext!"<<endl;
        return false;
    }

    if(avcodec_open2(datastate->av_codec_ctx, datastate->av_codec, NULL) < 0){
        cout<<"ERROR: Could not open codec!"<<endl;
        return false;
    }

    if(!(datastate->av_packet = av_packet_alloc())){
        cout<<"ERROR: Couldn't allocate AVPacket!"<<endl;
        return false;
    }

    if(!(datastate->av_frame = av_frame_alloc())){
        cout<<"ERROR: Could not allocate AVFrame!"<<endl;
        return false;
    }

    if(!(datastate->decoded_frame = av_frame_alloc())){
        cout<<"ERROR: Could not allocate AVFrame!"<<endl;
        return false;
    }

    /* Check resolution - Experimental */
    if(datastate->t_width <= datastate->av_codec_ctx->width && datastate->t_height <= datastate->av_codec_ctx->height){
        datastate->t_width = datastate->av_codec_ctx->width;
        datastate->t_height = datastate->av_codec_ctx->height;
    }

    datastate->num_bytes = av_image_get_buffer_size(   
        AV_PIX_FMT_YUV420P,
        datastate->t_width,
        datastate->t_width,
        32
    );

    datastate->buffer = (uint8_t *)av_malloc(datastate->num_bytes * sizeof(uint8_t));

    av_image_fill_arrays(   
        datastate->decoded_frame->data,
        datastate->decoded_frame->linesize,
        datastate->buffer,
        AV_PIX_FMT_YUV420P,
        datastate->t_width,
        datastate->t_height,
        32
    );

    return true;
}

void close_data(datastruct* datastate)
{
    av_packet_free(&datastate->av_packet);
    av_frame_free(&datastate->av_frame);
    av_frame_free(&datastate->decoded_frame);
    avformat_close_input(&datastate->av_format_ctx);
    avcodec_free_context(&datastate->av_codec_ctx);
    avcodec_close(datastate->av_codec_ctx);
    av_freep(&datastate->buffer);
    sws_freeContext(datastate->sws_ctx);
    cout<<"Cleanup complete."<<endl;
}