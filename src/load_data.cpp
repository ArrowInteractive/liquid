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
        if(datastate->av_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && datastate->video_stream_index == -1)
        {
            datastate->video_stream_index = i;
        }
        
        if(datastate->av_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && datastate->audio_stream_index == -1)
        {
            datastate->audio_stream_index = i;
        }   
    }
    if(datastate->video_stream_index == -1){
        cout<<"ERROR: Could not find any video streams in the file!"<<endl;
        return false;
    }

    /*
        Audio decode setup
        Only allocate audio vars if audio stream is present
    */
    if(datastate->audio_stream_index  != -1){
        datastate->audio_codec_params = datastate->av_format_ctx->streams[datastate->audio_stream_index]->codecpar;
        datastate->audio_codec = avcodec_find_decoder(datastate->audio_codec_params->codec_id);

        if(!(datastate->audio_codec_ctx = avcodec_alloc_context3(datastate->audio_codec))){
            cout<<"ERROR: Could not setup AVCodecContext!"<<endl;
            return false;
        }

        if(avcodec_parameters_to_context(datastate->audio_codec_ctx, datastate->audio_codec_params) < 0){
            cout<<"ERROR: Could not pass the parameters to AVCodecContext!"<<endl;
            return false;
        }

        if(avcodec_open2(datastate->audio_codec_ctx, datastate->audio_codec, NULL) < 0){
            cout<<"ERROR: Could not open codec!"<<endl;
            return false;
        }
    }
    
    /* 
        Video decode setup
    */
    datastate->video_codec_params = datastate->av_format_ctx->streams[datastate->video_stream_index]->codecpar;
    datastate->video_codec = avcodec_find_decoder(datastate->video_codec_params->codec_id);

    if(!(datastate->video_codec_ctx = avcodec_alloc_context3(datastate->video_codec))){
        cout<<"ERROR: Could not setup AVCodecContext!"<<endl;
        return false;
    }

    if(avcodec_parameters_to_context(datastate->video_codec_ctx, datastate->video_codec_params) < 0){
        cout<<"ERROR: Could not pass the parameters to AVCodecContext!"<<endl;
        return false;
    }

    if(avcodec_open2(datastate->video_codec_ctx, datastate->video_codec, NULL) < 0){
        cout<<"ERROR: Could not open codec!"<<endl;
        return false;
    }

    if(!(datastate->video_packet = av_packet_alloc())){
        cout<<"ERROR: Couldn't allocate AVPacket!"<<endl;
        return false;
    }

    if(!(datastate->video_frame = av_frame_alloc())){
        cout<<"ERROR: Could not allocate AVFrame!"<<endl;
        return false;
    }

    if(!(datastate->decoded_video_frame = av_frame_alloc())){
        cout<<"ERROR: Could not allocate AVFrame!"<<endl;
        return false;
    }

    /* Check resolution - Experimental */
    if(datastate->t_width <= datastate->video_codec_ctx->width && datastate->t_height <= datastate->video_codec_ctx->height){
        datastate->t_width = datastate->video_codec_ctx->width;
        datastate->t_height = datastate->video_codec_ctx->height;
    }

    datastate->num_bytes = av_image_get_buffer_size(   
        AV_PIX_FMT_YUV420P,
        datastate->t_width,
        datastate->t_width,
        32
    );

    datastate->video_buffer = (uint8_t *)av_malloc(datastate->num_bytes * sizeof(uint8_t));

    av_image_fill_arrays(   
        datastate->decoded_video_frame->data,
        datastate->decoded_video_frame->linesize,
        datastate->video_buffer,
        AV_PIX_FMT_YUV420P,
        datastate->t_width,
        datastate->t_height,
        32
    );

    return true;
}

void close_data(datastruct* datastate)
{
    av_packet_free(&datastate->video_packet);
    av_frame_free(&datastate->video_frame);
    av_frame_free(&datastate->decoded_video_frame);
    avformat_close_input(&datastate->av_format_ctx);
    avcodec_free_context(&datastate->video_codec_ctx);
    avcodec_close(datastate->video_codec_ctx);
    av_freep(&datastate->video_buffer);
    sws_freeContext(datastate->sws_ctx);
    cout<<"Cleanup complete."<<endl;
}