/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2019 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/
#include "inc/hb_media_codec.h"
#include "inc/hb_media_error.h"
#include "media_codec/component/media_codec_app.h"
#include "media_codec/component/media_codec_descriptor.h"
#include "media_codec/component/media_codec_video.h"
#include "ffmpeg_audio/include/ffmpeg_audio_interface.h"

#define TAG "[MEDIACODEC]"
#define ENCODER_STR "Encoder"
#define DECODER_STR "Decoder"

static inline hb_s32 get_err_of_query_result(MCTaskQueryError queryErr)
{
	hb_s32 ret = 0;

	if (queryErr == MC_TASK_INVALID_PARAMS) {
		ret = HB_MEDIA_ERR_INVALID_PARAMS;
	} else if ((queryErr == MC_TASK_NOT_EXIST) ||
		(queryErr == MC_TASK_WRONG_APP_TYPE)) {
		ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
	} else if (queryErr == MC_TASK_WRONG_INST_IDX) {
		ret = HB_MEDIA_ERR_INVALID_INSTANCE;
	} else if (queryErr == MC_TASK_EXIST) {
		ret = 0;
	} else {
		ret = HB_MEDIA_ERR_UNKNOWN;
	}
	if (ret != 0) {
		VLOG(ERR, "%s <%s:%d> Fail to get codec task.(%s)\n",
			TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
	}

	return ret;
}

const media_codec_descriptor_t *hb_mm_mc_get_descriptor(media_codec_id_t codec_id)
{
	if ((codec_id <= MEDIA_CODEC_ID_NONE) || (codec_id >= MEDIA_CODEC_ID_TOTAL)) {
		VLOG(ERR, "%s <%s:%d> Invalid codec id %d.\n", TAG, __FUNCTION__, __LINE__, codec_id);
		return NULL;
	}

	return mc_desc_get_desc(codec_id);
}

hb_s32 hb_mm_mc_get_default_context(media_codec_id_t codec_id, hb_bool encoder,
		media_codec_context_t * context)
{
	hb_s32 ret = 0;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if ((codec_id <= MEDIA_CODEC_ID_NONE) || (codec_id >= MEDIA_CODEC_ID_TOTAL)) {
		VLOG(ERR, "%s <%s:%d> Invalid codec id %d.\n",
			TAG, __FUNCTION__, __LINE__, codec_id);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if ((encoder != TRUE) && (encoder != FALSE)) {
		VLOG(ERR, "%s <%s:%d> Invalid encoder parameter(%d). Should be [%d, %d].\n",
			TAG, __FUNCTION__, __LINE__, encoder, FALSE, TRUE);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	context->codec_id = codec_id;
	context->encoder = encoder;

	switch (codec_id) {
	case MEDIA_CODEC_ID_H264:
		if (encoder != FALSE) {
			mc_video_get_default_h264enc_params(&context->video_enc_params);
		} else {
			mc_video_get_default_h264dec_params(&context->video_dec_params);
		}
		break;
	case MEDIA_CODEC_ID_H265:
		if (encoder != FALSE) {
			mc_video_get_default_h265enc_params(&context->video_enc_params);
		} else {
			mc_video_get_default_h265dec_params(&context->video_dec_params);
		}
		break;
	case MEDIA_CODEC_ID_MJPEG:
		if (encoder != FALSE) {
			mc_video_get_default_mjpegenc_params(&context->video_enc_params);
		} else {
			mc_video_get_default_mjpegdec_params(&context->video_dec_params);
		}
		break;
	case MEDIA_CODEC_ID_JPEG:
		if (encoder != FALSE) {
			mc_video_get_default_jpegenc_params(&context->video_enc_params);
		} else {
			mc_video_get_default_jpegdec_params(&context->video_dec_params);
		}
		break;
	case MEDIA_CODEC_ID_AAC:
		if (encoder != FALSE) {
			mc_audio_get_default_aacEnc_params(&context->audio_enc_params);
		} else {
			mc_audio_get_default_aacDec_params(&context->audio_dec_params);
		}
		break;
	case MEDIA_CODEC_ID_FLAC:
		if (encoder != FALSE) {
			mc_audio_get_default_flacEnc_params(&context->audio_enc_params);
		} else {
			mc_audio_get_default_flacDec_params(&context->audio_dec_params);
		}
		break;
	case MEDIA_CODEC_ID_PCM_MULAW:
	case MEDIA_CODEC_ID_PCM_ALAW:
		if (encoder != FALSE) {
			mc_audio_get_default_g711Enc_params(&context->audio_enc_params);
		} else {
			mc_audio_get_default_g711Dec_params(&context->audio_dec_params);
		}
		break;
	case MEDIA_CODEC_ID_ADPCM_G726:
		if (encoder != FALSE) {
			mc_audio_get_default_g726Enc_params(&context->audio_enc_params);
		} else {
			mc_audio_get_default_g726Dec_params(&context->audio_dec_params);
		}
		break;
	case MEDIA_CODEC_ID_ADPCM:
		if (encoder != FALSE) {
			mc_audio_get_default_adpcmEnc_params(&context->audio_enc_params);
		} else {
			mc_audio_get_default_adpcmDec_params(&context->audio_dec_params);
		}
		break;
	default:
		VLOG(ERR, "%s <%s:%d> There is no default context for codec id %d.\n",
			TAG, __FUNCTION__, __LINE__, codec_id);
		ret = HB_MEDIA_ERR_INVALID_PARAMS;
		break;
	}

	return ret;
}

hb_s32 hb_mm_mc_initialize(media_codec_context_t * context)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	// initialize the media codec app environment
	ret = MCAppInitLocked(context->codec_id);

	if (ret == 0) {
		// try to get the corresponding MCTask
		queryErr = MCAPPGetTaskLocked(context, &task);
		if (queryErr == MC_TASK_NOT_EXIST) {
			// create new task and add it to app
			task = MCTaskCreate();
			if (task != NULL) {
				ret = MCTaskInitLocked(task, context->codec_id,
							context->encoder, (void *)context);
				if (ret == 0) {
					ret = MCAppAddTaskLocked(task);
					if (ret == 0) {
						VLOG(INFO, "%s%02d <%s:%d> Success to initialize the media codec(task=%p, instance id=%d).\n",
							TAG, task->instIdx, __FUNCTION__, __LINE__, task, task->instIdx);
						context->instance_index = task->instIdx;
					} else {
						VLOG(ERR, "%s <%s:%d> Fail to add codec task.(%s)\n",
							TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
					}
				} else {
					VLOG(ERR, "%s <%s:%d> Fail to initialize media codec.(%s)\n",
						TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
				}
			} else {
				ret = HB_MEDIA_ERR_INSUFFICIENT_RES;
				VLOG(ERR, "%s <%s:%d> Fail to allocate codec task.(%s)\n",
					TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
			}
		} else if ((queryErr == MC_TASK_INVALID_PARAMS) ||
			(queryErr == MC_TASK_WRONG_APP_TYPE)) {
			ret = HB_MEDIA_ERR_INVALID_PARAMS;
			VLOG(ERR, "%s <%s:%d> Fail to get codec task.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		} else if (queryErr == MC_TASK_WRONG_INST_IDX) {
			ret = HB_MEDIA_ERR_INVALID_INSTANCE;
			VLOG(ERR, "%s <%s:%d> Fail to get codec task.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		} else if (queryErr == MC_TASK_EXIST) {
			ret = MCTaskInitLocked(task, context->codec_id,
						context->instance_index, (void *)context);
		} else {
			ret = HB_MEDIA_ERR_UNKNOWN;
			VLOG(ERR, "%s <%s:%d> Fail to get codec task.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		}

		if (task != NULL) {
			MCTaskDecRef(task);
		}
	}

	return ret;
}

hb_s32 hb_mm_mc_set_callback(media_codec_context_t * context,
		const media_codec_callback_t * callback, hb_ptr userdata)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	if ((callback == NULL) || (userdata == NULL)) {
		VLOG(ERR, "%s <%s:%d> Invalid user callback data(userCallback=%p, userdata=%p).\n",
			TAG, __FUNCTION__, __LINE__, callback, userdata);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	if ((callback->on_input_buffer_available == NULL) ||
		(callback->on_output_buffer_available == NULL) ||
		(callback->on_media_codec_message == NULL)) {
		VLOG(ERR, "%s <%s:%d> Invalid user callback function(on_input_buffer_available=%p, "
			"on_output_buffer_available=%p, on_media_codec_message=%p).\n",
			TAG, __FUNCTION__, __LINE__, callback->on_input_buffer_available,
			callback->on_output_buffer_available,
			callback->on_media_codec_message);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskRegisterListernerLocked(task, callback, userdata, context);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_vlc_buffer_listener(
		media_codec_context_t * context,
		const media_codec_callback_t * callback, hb_ptr userdata)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	if ((callback == NULL) || (userdata == NULL)) {
		VLOG(ERR, "%s <%s:%d> Invalid user callback data(userCallback=%p, userdata=%p).\n",
			TAG, __FUNCTION__, __LINE__, callback, userdata);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	if (callback->on_vlc_buffer_message == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid user callback function(on_vlc_buffer_message=%p).\n",
			TAG, __FUNCTION__, __LINE__, callback->on_vlc_buffer_message);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	if ((callback->on_input_buffer_available != NULL) ||
		(callback->on_output_buffer_available != NULL) ||
		(callback->on_media_codec_message != NULL)) {
		VLOG(INFO, "%s <%s:%d> Callback on_input_buffer_available/"
			"on_output_buffer_available/on_media_codec_message "
			"is useless in this interface.\n", TAG, __FUNCTION__, __LINE__);
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskRegisterVLCBufListernerLocked(task, callback, userdata,
			  context);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_configure(media_codec_context_t * context)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskConfigureLocked(task, context);
		if (ret == 0) {
			VLOG(INFO, "%s%02d <%s:%d> Success to configure codec task.\n",
				TAG, task->instIdx, __FUNCTION__, __LINE__);
		} else {
			VLOG(ERR, "%s <%s:%d> Fail to configure codec task.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_start(media_codec_context_t * context,
		const mc_av_codec_startup_params_t * info)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskStartLocked(task, info);
		if (ret == 0) {
			VLOG(INFO, "%s%02d <%s:%d> Success to start codec task.\n",
				TAG, task->instIdx, __FUNCTION__, __LINE__);
		} else {
			VLOG(ERR, "%s <%s:%d> Fail to start codec task.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_stop(media_codec_context_t * context)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskStopLocked(task);
		if (ret == 0) {
			VLOG(INFO, "%s%02d <%s:%d> Success to stop task.\n",
				TAG, task->instIdx, __FUNCTION__, __LINE__);
		} else {
			VLOG(ERR, "%s <%s:%d> Fail to stop task.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_pause(media_codec_context_t * context)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskPauseLocked(task);
		if (ret == 0) {
			VLOG(INFO, "%s%02d <%s:%d> Success to pause task.\n",
				TAG, task->instIdx, __FUNCTION__, __LINE__);
		} else {
			VLOG(ERR, "%s <%s:%d> Fail to pause task.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_flush(media_codec_context_t * context)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskFlushLocked(task);
		if (ret == 0) {
			VLOG(INFO, "%s%02d <%s:%d> Success to flush task.\n",
				TAG, task->instIdx, __FUNCTION__, __LINE__);
		} else {
			VLOG(ERR, "%s <%s:%d> Fail to flush task.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_release(media_codec_context_t * context)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskRelease(task);
		if (ret == 0) {
			ret = MCAppDeleteTaskLocked(task);
			if (ret == 0) {
				VLOG(INFO, "%s%02d <%s:%d> Success to delete task.\n",
					TAG, task->instIdx, __FUNCTION__, __LINE__);
			} else {
				VLOG(ERR, "%s <%s:%d> Fail to delete task.(%s)\n",
					TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
			}
		} else {
			VLOG(ERR, "%s <%s:%d> Fail to release task.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_state(media_codec_context_t * context,
		media_codec_state_t * state)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (state == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL state.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetStateLocked(task, state);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		*state = MEDIA_CODEC_STATE_UNINITIALIZED;
		ret = 0;
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_status(media_codec_context_t *context,
				mc_inter_status_t *status)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (status == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL status.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetStatusLocked(task, context, status);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_queue_input_buffer(media_codec_context_t * context,
		media_codec_buffer_t * buffer, hb_s32 timeout)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (buffer == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL buffer.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskQueueInputBufferLocked(task, buffer, timeout);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_dequeue_input_buffer(media_codec_context_t * context,
		media_codec_buffer_t * buffer, hb_s32 timeout)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (buffer == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL buffer.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskDequeueInputBufferLocked(task, buffer, timeout);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_queue_output_buffer(media_codec_context_t * context,
		media_codec_buffer_t * buffer, hb_s32 timeout)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (buffer == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL buffer.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskQueueOutputBufferLocked(task, buffer, timeout);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_dequeue_output_buffer(media_codec_context_t * context,
		media_codec_buffer_t * buffer,
		media_codec_output_buffer_info_t * info,
		hb_s32 timeout)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (buffer == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL buffer.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskDequeueOutputBufferLocked(task, buffer, info, timeout);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_longterm_ref_mode(media_codec_context_t * context,
		mc_video_longterm_ref_mode_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_LONGTERM_REF, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_longtermRef_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}
	return ret;
}

hb_s32 hb_mm_mc_set_longterm_ref_mode(media_codec_context_t * context,
		const mc_video_longterm_ref_mode_t *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_LONGTERM_REF, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_intra_refresh_config(media_codec_context_t * context,
		mc_video_intra_refresh_params_t *params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_INTRA_REFRESH, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if (((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE))) {
			mc_video_get_default_intraRefresh_params(params, context->codec_id);
			ret = 0;
		} else {
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_intra_refresh_config(media_codec_context_t * context,
		const mc_video_intra_refresh_params_t *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_INTRA_REFRESH, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_rate_control_config(media_codec_context_t * context,
		mc_rate_control_params_t * params)
{
	hb_s32 ret;
	MCAppType appType;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_RATE_CONTROL, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		appType = MCTaskGetAppType(context->codec_id);
		if ((context->encoder != FALSE) &&
			((appType == MC_APP_VIDEO) || (appType == MC_APP_JPEG))) {
			ret = mc_video_get_default_rate_control_params(params);
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_rate_control_config(media_codec_context_t * context,
		const mc_rate_control_params_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_RATE_CONTROL, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_max_bit_rate_config(media_codec_context_t *context,
			hb_u32 *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_TRANS_BITRATE, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_transrate_params(params);
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_max_bit_rate_config(media_codec_context_t *context,
			hb_u32 params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_TRANS_BITRATE, (const void *)&params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_deblk_filter_config(media_codec_context_t * context,
		mc_video_deblk_filter_params_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_DEBLK_FILTER, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_deblkFilter_params(params, context->codec_id);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}
	return ret;
}

hb_s32 hb_mm_mc_set_deblk_filter_config(media_codec_context_t * context,
		const mc_video_deblk_filter_params_t *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_DEBLK_FILTER, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_sao_config(media_codec_context_t * context,
		mc_h265_sao_params_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_SAO, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((context->codec_id == MEDIA_CODEC_ID_H265)
			&& (context->encoder != FALSE)) {
			mc_video_get_default_sao_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_sao_config(media_codec_context_t * context,
		const mc_h265_sao_params_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_SAO, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_entropy_config(media_codec_context_t * context,
		mc_h264_entropy_params_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_ENTROPY, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((context->codec_id == MEDIA_CODEC_ID_H264) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_entropy_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_entropy_config(media_codec_context_t * context,
		const mc_h264_entropy_params_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_ENTROPY, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_vui_timing_config(media_codec_context_t * context,
		mc_video_vui_timing_params_t * params)
{
	hb_s32 ret;
	hb_u32 frameRate;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_VUI_TIMING, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			frameRate = mc_video_get_enc_frame_rate(context);
			if (frameRate != 0U) {
				mc_video_get_default_VUITiming_params(
					(mc_video_vui_timing_params_t *) params,
					context->codec_id, frameRate);
				ret = 0;
			} else {
				VLOG(ERR, "%s <%s:%d> Frame rate should not be 0.\n",
					TAG, __FUNCTION__, __LINE__);
			}
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_vui_timing_config(media_codec_context_t * context,
		const mc_video_vui_timing_params_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_VUI_TIMING, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_vui_config(media_codec_context_t * context,
		mc_video_vui_params_t * params)
{
	hb_s32 ret;
	hb_u32 frameRate;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_VUI, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			frameRate = mc_video_get_enc_frame_rate(context);
			if (frameRate != 0U) {
				mc_video_get_default_VUI_params(
					(mc_video_vui_params_t *) params,
					context->codec_id, frameRate);
				ret = 0;
			} else {
				VLOG(ERR, "%s <%s:%d> Frame rate should not be 0.\n",
					TAG, __FUNCTION__, __LINE__);
			}
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_vui_config(media_codec_context_t * context,
		const mc_video_vui_params_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_VUI,
					(const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_slice_config(media_codec_context_t * context,
		mc_video_slice_params_t * params)
{
	hb_s32 ret;
	MCAppType appType;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_SLICE, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		appType = MCTaskGetAppType(context->codec_id);
		if ((context->encoder != FALSE) &&
			((appType == MC_APP_VIDEO) || (appType == MC_APP_JPEG))) {
			mc_video_get_default_slice_params(params, context->codec_id);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_slice_config(media_codec_context_t * context,
		const mc_video_slice_params_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_SLICE, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_insert_user_data(media_codec_context_t * context,
		hb_u8 *data, hb_u32 length)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		mc_external_user_data_info_t info;
		info.size = length;
		info.virt_addr = data;
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_INSERT_USERDATA,
			(const void *)&info);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_request_idr_frame(media_codec_context_t * context)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_REQUEST_IDR, NULL);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_request_idr_header(
				media_codec_context_t *context, hb_u32 force_header)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context,
				ENC_CONFIG_REQUEST_IDR_HEADER, &force_header);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_enable_idr_frame(media_codec_context_t * context,
		hb_bool enable)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		hb_bool local_par = enable;
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_ENABLE_IDR, (const void *)&local_par);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_skip_pic(media_codec_context_t * context, hb_s32 src_idx)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_SKIP_PIC, &src_idx);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_3dnr_enc_config(media_codec_context_t * context,
		mc_video_3dnr_enc_params_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_3DNR, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE) && (context->codec_id == MEDIA_CODEC_ID_H265)) {
			mc_video_get_default_3dnr_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_3dnr_enc_config(media_codec_context_t * context,
		const mc_video_3dnr_enc_params_t *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_3DNR, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_smart_bg_enc_config(media_codec_context_t * context,
		mc_video_smart_bg_enc_params_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_SMART_BG, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_smartBG_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_smart_bg_enc_config(media_codec_context_t * context,
		const mc_video_smart_bg_enc_params_t *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_SMART_BG, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_monochroma_config(media_codec_context_t * context,
		mc_video_monochroma_params_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_MONOCHROMA, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_monochroma_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_monochroma_config(media_codec_context_t * context,
		const mc_video_monochroma_params_t *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_MONOCHROMA,
				    (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_pred_unit_config(media_codec_context_t * context,
		mc_video_pred_unit_params_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_PRED_UNIT, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_predUnit_params(params, context->codec_id);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_pred_unit_config(media_codec_context_t * context,
		const mc_video_pred_unit_params_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_PRED_UNIT, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_transform_config(media_codec_context_t * context,
		mc_video_transform_params_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_TRANSFORM, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_transform_params(params, context->codec_id);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_transform_config(media_codec_context_t * context,
		const mc_video_transform_params_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_TRANSFORM, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_roi_config(media_codec_context_t * context,
		mc_video_roi_params_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_ROI, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_roi_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_roi_config(media_codec_context_t * context,
		const mc_video_roi_params_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_ROI, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_roi_avg_qp(media_codec_context_t * context,
			hb_u32 * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_ROI_AVG_QP, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_roi_avgQP_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_roi_avg_qp(media_codec_context_t * context,
		hb_u32 params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_ROI_AVG_QP,
					(const void *)&params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_roi_config_ex(media_codec_context_t * context,
		hb_u32 roi_idx, mc_video_roi_params_ex_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		params->roi_idx = roi_idx;
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_ROI_EX, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_roi_params_ex(roi_idx, params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_roi_config_ex(media_codec_context_t * context,
		const mc_video_roi_params_ex_t * params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_ROI_EX,
					(const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_mode_decision_config(media_codec_context_t * context,
		mc_video_mode_decision_params_t *params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_MODE_DECISION, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((context->encoder != FALSE) &&
			(context->codec_id == MEDIA_CODEC_ID_H265)) {
			mc_video_get_default_modeDecision_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_mode_decision_config(media_codec_context_t * context,
		const mc_video_mode_decision_params_t *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_MODE_DECISION, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_encode_mode_config(media_codec_context_t * context,
		hb_s32 *mode)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (mode == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_ENCODE_MODE,
					(void *)mode);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if (context->encoder != FALSE) {
			mc_video_get_default_encodeMode_params(mode);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_encode_mode_config(
		media_codec_context_t *context, hb_s32 mode)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_ENCODE_MODE,
					(void *)&mode);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_user_data(media_codec_context_t * context,
		mc_user_data_buffer_t * params, hb_s32 timeout)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetUserData(task, params, timeout);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_release_user_data(media_codec_context_t * context,
		const mc_user_data_buffer_t * params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskReleaseUserData(task, params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_explicit_header_config(
			media_codec_context_t *context, hb_s32 *status)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (status == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_ENABLE_EXP_HEADER,
					(void *)status);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((MCTaskGetAppType(context->codec_id) == MC_APP_VIDEO) &&
			(context->encoder != FALSE)) {
			mc_video_get_default_explicitHeader_params(status);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_explicit_header_config(
		media_codec_context_t *context, hb_s32 status)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_ENABLE_EXP_HEADER,
				&status);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_mjpeg_config(media_codec_context_t *context,
		mc_mjpeg_enc_params_t *params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_MJPEG, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((context->encoder != FALSE) &&
			(context->codec_id == MEDIA_CODEC_ID_MJPEG)) {
			mc_video_get_default_mjpeg_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_mjpeg_config(media_codec_context_t *context,
		const mc_mjpeg_enc_params_t *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_MJPEG, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_jpeg_config(media_codec_context_t *context,
		mc_jpeg_enc_params_t *params)
{
	hb_s32 ret;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskGetConfig(task, context, ENC_CONFIG_JPEG, (void *)params);
	} else if ((queryErr != MC_TASK_INVALID_PARAMS) &&
		(queryErr != MC_TASK_WRONG_INST_IDX)) {
		if ((context->encoder != FALSE) &&
			(context->codec_id == MEDIA_CODEC_ID_JPEG)) {
			mc_video_get_default_jpeg_params(params);
			ret = 0;
		} else {
			VLOG(ERR, "%s <%s:%d> Not supported for codec id %d or %s.\n",
				TAG, __FUNCTION__, __LINE__,
				context->codec_id, (context->encoder != FALSE) ? ENCODER_STR : DECODER_STR);
			ret = HB_MEDIA_ERR_OPERATION_NOT_ALLOWED;
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_jpeg_config(media_codec_context_t *context,
		const mc_jpeg_enc_params_t *params)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (params == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_JPEG, (const void *)params);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_set_camera(media_codec_context_t *context, hb_s32 pipeline,
		hb_s32 channel_port_id)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		mc_video_cam_info_t info = {0};
		info.cam_pipline = pipeline;
		info.cam_channel = channel_port_id;
		ret = MCTaskSetConfig(task, context, ENC_CONFIG_CAMERA, (const void *)&info);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_get_fd(media_codec_context_t * context,
		hb_s32 *fd)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (fd == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskOpenFdLocked(task, context->codec_id, fd);
		if (ret != 0) {
			VLOG(ERR, "%s <%s:%d> Fail to get fd.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_close_fd(media_codec_context_t * context,
		hb_s32 fd)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskCloseFdLocked(task, context->codec_id, fd);
		if (ret == 0) {
			VLOG(INFO, "%s%02d <%s:%d> Success to close fd.\n",
				TAG, task->instIdx, __FUNCTION__, __LINE__);
		} else {
			VLOG(ERR, "%s <%s:%d> Fail to close fd.(%s)\n",
				TAG, __FUNCTION__, __LINE__, hb_mm_err2str(ret)); /* PRQA S 3469 */
		}
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}

hb_s32 hb_mm_mc_register_audio_encoder(hb_s32*handle,
		mc_audio_encode_param_t *encoder) {
	hb_s32 ret;

#ifdef ENABLE_AUDIO
	ret = MCAPPRegisterAudioEncoder(handle, encoder);
#else
	ret = HB_MEDIA_ERR_CODEC_NOT_FOUND;
#endif
	if (ret < 0) {
		VLOG(ERR, "%s <%s:%d> register audio encoder failed.\n",
			TAG, __FUNCTION__, __LINE__);
		return ret;
	}

	return ret;
}

hb_s32 hb_mm_mc_unregister_audio_encoder(hb_s32 handle) {
	hb_s32 ret;

#ifdef ENABLE_AUDIO
	ret = MCAPPUnregisterAudioEncoder(handle);
#else
	ret = HB_MEDIA_ERR_CODEC_NOT_FOUND;
#endif
	if (ret < 0) {
		VLOG(ERR, "%s <%s:%d> unregister audio encoder failed.\n",
			TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_CODEC_NOT_FOUND;
	}

	return ret;
}

hb_s32 hb_mm_mc_register_audio_decoder(hb_s32 *handle,
		mc_audio_decode_param_t *decoder) {
	hb_s32 ret;

#ifdef ENABLE_AUDIO
	ret = MCAPPRegisterAudioDecoder(handle, decoder);
#else
	ret = HB_MEDIA_ERR_CODEC_NOT_FOUND;
#endif
	if (ret < 0) {
		VLOG(ERR, "%s <%s:%d> register audio decoder failed.\n",
			TAG, __FUNCTION__, __LINE__);
		return ret;
	}

	return ret;
}

hb_s32 hb_mm_mc_unregister_audio_decoder(hb_s32 handle) {
	hb_s32 ret;

#ifdef ENABLE_AUDIO
	ret = MCAPPUnregisterAudioDecoder(handle);
#else
	ret = HB_MEDIA_ERR_CODEC_NOT_FOUND;
#endif
	if (ret < 0) {
		VLOG(ERR, "%s <%s:%d> unregister audio decoder failed.\n",
			TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_CODEC_NOT_FOUND;
	}

	return ret;
}

hb_s32 hb_mm_mc_set_status(media_codec_context_t *context,
				mc_user_status_t *status)
{
	hb_s32 ret = 0;
	MCTaskContext *task = NULL;
	MCTaskQueryError queryErr;

	if (context == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL context.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}
	if (status == NULL) {
		VLOG(ERR, "%s <%s:%d> Invalid NULL params.\n", TAG, __FUNCTION__, __LINE__);
		return HB_MEDIA_ERR_INVALID_PARAMS;
	}

	// try to get the corresponding MCTask
	queryErr = MCAPPGetTaskLocked(context, &task);
	if (queryErr == MC_TASK_EXIST) {
		ret = MCTaskSetStatusLocked(task, context, status);
	} else {
		ret = get_err_of_query_result(queryErr);
	}

	if (task != NULL) {
		MCTaskDecRef(task);
	}

	return ret;
}
