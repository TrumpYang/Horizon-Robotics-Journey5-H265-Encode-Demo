#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <iostream>
#include "hb_media_codec.h"
#include "hb_media_error.h"

// 这是一个编码示例
#define MAX_FILE_PATH 512
#define TAG "MediaCodecTest"
#include <sys/time.h>
// typedef void* hb_ptr;

using namespace std;

typedef uint64_t Uint64;
typedef int32_t hb_s32;

typedef struct MediaCodecTestContext
{
    media_codec_context_t *context;
    char *inputFileName;
    char *outputFileName;
    int32_t duration; // ms
} MediaCodecTestContext;
// 获取当前时间的函数
long long get_current_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000); // 毫秒
}

Uint64 osal_gettime(void)
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return ((Uint64)tp.tv_sec * 1000 + tp.tv_nsec / 1000000);
}
// 同步编码
static void do_sync_encoding(void *arg)
{
    hb_s32 ret = 0;
    FILE *inFile;
    FILE *outFile;
    int noMoreInput = 0;
    int lastStream = 0;
    Uint64 lastTime = 0;
    Uint64 curTime = 0;
    int needFlush = 1;
    MediaCodecTestContext *ctx = (MediaCodecTestContext *)arg;
    media_codec_context_t *context = ctx->context;
    char *inputFileName = ctx->inputFileName;
    char *outputFileName = ctx->outputFileName;
    media_codec_state_t state = MEDIA_CODEC_STATE_NONE;

    inFile = fopen(inputFileName, "rb");
    if (!inFile)
    {
        printf("Failed to open input file.\n");
        goto ERR;
    }

    outFile = fopen(outputFileName, "wb");
    if (!outFile)
    {
        printf("Failed to open output file.\n");
        goto ERR;
    }

    // get current time
    lastTime = osal_gettime();
    ret = hb_mm_mc_initialize(context);
    if (ret)
    {
        printf("hb_mm_mc_initialize failed\n");
        goto ERR;
    }
    cout << "hb_mm_mc_initialize success!" << endl;
    ret = hb_mm_mc_configure(context);
    if (ret)
    {
        printf("hb_mm_mc_configure failed\n");
        goto ERR;
    }
    cout << "hb_mm_mc_configure success!" << endl;
    mc_av_codec_startup_params_t startup_params;
    startup_params.video_enc_startup_params.receive_frame_number = 0;
    ret = hb_mm_mc_start(context, &startup_params);
    if (ret)
    {
        printf("hb_mm_mc_start failed\n");
        goto ERR;
    }
    cout << "hb_mm_mc_start success!" << endl;
    // ret = hb_mm_mc_pause(context);
    // if (ret)
    // {
    //     printf("hb_mm_mc_pause failed\n");
    //     goto ERR;
    // }

    cout << "===========准备开始编码=============" << endl;
    do
    {
        if (!noMoreInput)
        {
            media_codec_buffer_t inputBuffer;
            memset(&inputBuffer, 0x00, sizeof(media_codec_buffer_t));
            ret = hb_mm_mc_dequeue_input_buffer(context, &inputBuffer, 100);

            auto start_encoder_time = get_current_time_ms();
            if (!ret)
            {
                curTime = osal_gettime();
                if ((curTime - lastTime) < (uint32_t)ctx->duration)
                {
                    ret = fread(inputBuffer.vframe_buf.vir_ptr[0], 1,
                                inputBuffer.vframe_buf.size, inFile);
                    if (ret <= 0)
                    {
                        if (fseek(inFile, 0, SEEK_SET))
                        {
                            printf("Failed to rewind input file\n");
                        }
                        else
                        {
                            ret = fread(inputBuffer.vframe_buf.vir_ptr[0], 1,
                                        inputBuffer.vframe_buf.size, inFile);
                            if (ret <= 0)
                            {
                                printf("Failed to read input file\n");
                            }
                        }
                    }
                }
                else
                {
                    printf("Time up(%d)\n", ctx->duration);
                    ret = 0;
                }
                if (!ret)
                {
                    printf("There is no more input data!\n");
                    inputBuffer.vframe_buf.frame_end = 1;
                    noMoreInput = 1;
                }

                ret = hb_mm_mc_queue_input_buffer(context, &inputBuffer, 100);
                cout << "hb_mm_mc_queue_input_buffer ret is " << ret << endl;
                if (ret)
                {
                    printf("Queue input buffer failed.\n");
                    break;
                }
                // else
                // {
                //     if (ret != (int32_t)HB_MEDIA_ERR_WAIT_TIMEOUT)
                //     {
                //         printf("Dequeue input buffer failed.\n");
                //         break;
                //     }
                // }

                if (!lastStream)
                {
                    media_codec_buffer_t outputBuffer;
                    media_codec_output_buffer_info_t info;
                    memset(&outputBuffer, 0x00, sizeof(media_codec_buffer_t));
                    memset(&info, 0x00, sizeof(media_codec_output_buffer_info_t));
                    ret = hb_mm_mc_dequeue_output_buffer(context, &outputBuffer, &info, 3000);
                    if (!ret && outFile)
                    {
                        cout << " outputBuffer.vstream_buf.size:" << outputBuffer.vstream_buf.size << endl;
                        fwrite(outputBuffer.vstream_buf.vir_ptr,
                               outputBuffer.vstream_buf.size, 1, outFile);
                        ret = hb_mm_mc_queue_output_buffer(context, &outputBuffer, 100);
                        if (ret)
                        {
                            printf("Queue output buffer failed.\n");
                            break;
                        }
                        if (outputBuffer.vstream_buf.stream_end)
                        {
                            printf("There is no more output data!\n");
                            lastStream = 1;
                            break;
                        }
                    }
                    else
                    {
                        if (ret != (int32_t)HB_MEDIA_ERR_WAIT_TIMEOUT)
                        {
                            printf("Dequeue output buffer failed.\n");
                            break;
                        }
                    }
                }

                if (needFlush)
                {
                    ret = hb_mm_mc_flush(context);
                    needFlush = 0;
                    if (ret)
                    {
                        printf("Flush failed.\n");
                        break;
                    }
                }
            }
            // 记录编码后的时间，作差，计算延迟
            long long end_time = get_current_time_ms();       // 记录结束时间
            long long encoding_delay = end_time - start_encoder_time; // 计算编码延迟
            printf("Encoding delay: %lld ms\n", encoding_delay);
        }

    } while (!lastStream); // This is the correct exit condition for the loop

    // Stop and release resources
    hb_mm_mc_stop(context);
    hb_mm_mc_release(context);
    context = NULL;

ERR:
    // Error handling and resource cleanup
    // hb_mm_mc_get_state(context, &state);
    // if (context && state != MEDIA_CODEC_STATE_UNINITIALIZED)
    // {
    //     hb_mm_mc_stop(context);
    //     hb_mm_mc_release(context);
    // }
    if (inFile)
        fclose(inFile);
    if (outFile)
        fclose(outFile);
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        printf("Usage: %s <input_file> <output_file> <duration_ms>\n", argv[0]);
        return -1;
    }

    hb_s32 ret = 0;
    char inputFileName[MAX_FILE_PATH] = "./output_480p_30fps.yuv";
    char outputFileName[MAX_FILE_PATH] = "./output.stream";
    // Copy arguments to variables
    strncpy(inputFileName, argv[1], MAX_FILE_PATH - 1);
    inputFileName[MAX_FILE_PATH - 1] = '\0';
    strncpy(outputFileName, argv[2], MAX_FILE_PATH - 1);
    outputFileName[MAX_FILE_PATH - 1] = '\0';
    int  duration = atoi(argv[3]);
    mc_video_codec_enc_params_t *params;
    media_codec_context_t context;
    memset(&context, 0x00, sizeof(media_codec_context_t));
    context.codec_id = MEDIA_CODEC_ID_H265;
    context.encoder = 1;
    params = &context.video_enc_params;
    params->width = 1920;
    params->height = 1080;
    params->pix_fmt = MC_PIXEL_FORMAT_YUV420P;
    params->frame_buf_count = 5;
    params->external_frame_buf = false;
    params->bitstream_buf_count = 5;
    params->rc_params.mode = MC_AV_RC_MODE_H265CBR;
    ret = hb_mm_mc_get_rate_control_config(&context, &params->rc_params);
    if (ret)
    {
        return -1;
    }
    params->rc_params.h265_cbr_params.bit_rate = 8000;
    params->rc_params.h265_cbr_params.frame_rate = 30;
    params->rc_params.h265_cbr_params.intra_period = 30;
    params->gop_params.decoding_refresh_type = 2;
    params->gop_params.gop_preset_idx = 2;
    params->rot_degree = MC_CCW_0;
    params->mir_direction = MC_DIRECTION_NONE;
    params->frame_cropping_flag = false;
    // 输入的参数

    MediaCodecTestContext ctx;
    memset(&ctx, 0x00, sizeof(ctx));
    ctx.context = &context;
    cout<<inputFileName<<endl;
    ctx.inputFileName = inputFileName;
    ctx.outputFileName = outputFileName;
    ctx.duration = duration; // ms  大概播放的时长  单位为ms

    do_sync_encoding(&ctx);

    return 0;
}