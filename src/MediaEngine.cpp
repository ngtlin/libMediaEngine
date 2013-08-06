#include "MediaEngine.h"

#include <mediastreamer2/msvolume.h>
#include <mediastreamer2/msequalizer.h>
#include <mediastreamer2/msfileplayer.h>
#include <mediastreamer2/msjpegwriter.h>
#include <mediastreamer2/mseventqueue.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/dtmfgen.h>

#include <math.h>


#include <ortp/rtp.h>
#include <ortp/b64.h>

#if defined(ANDROID)
#include <android/log.h>
#endif


/**
 * Lowest volume measurement that can be returned by get_play_volume() or get_record_volume(), corresponding to pure silence.
 *
 * Note: keep this in sync with mediastreamer2/msvolume.h
 *
**/
#define ME_VOLUME_DB_LOWEST 		(-120) //dbm
#define DYNAMIC_PAYLOAD_TYPE_MIN 	(96)
#define DYNAMIC_PAYLOAD_TYPE_MAX 	(127)

// default parameters taken from http://www.linphone.org/eng/documentation/dev/tuning-linphone.html
/* Echo canceller */
#define ENABLE_ECHO_CANCELLATION	(1)
#define EC_DELAY					(0)
#define EC_TAIL_LEN					(60) 	// Tail length of echo canceller in milliseconds.
#define EC_FRAME_SIZE				(128)	// Frame size of echo canceller

/* Echo Limiter */
#define ENABLE_ECHO_LIMITER			(0)
#define EL_SPEED					(0.03f)
#define EL_THRESHOLD				(0.1f)  // Threshold above which the system becomes active. It is a normalized power, between 0 and 1.
#define EL_FORCE					(10)	// The proportional coefficient controlling the MIC attenuation.
#define EL_SUSTAIN					(100)

/* Noise gate */
#define ENABLE_NOISE_GATE			(0)
#define NG_THRESHOLD				(0.05f) // Noise gate threshold in linear power between 0 and 1.
											// Above this threshold the noise gate becomes bypass.
#define NG_FLOOR_GAIN				(0.0005f) // Noise gate's floor gain: gain applied to the signal when its energy is below the threshold.

/* Equalizer */
#define ENABLE_EQ					(0) //
#define EQ_GAINS					(300:0.1:100 700:0.2:250)

/* Automatic gain control */
#define ENABLE_AGC					(0) // Automatic gain control (of MIC input)
#define ENABLE_DC_REMOVAL			(0) // Enable or disable DC removal of mic input

/* Network Adaptive rate control */
#define ENABLE_ARC					(1) //default set it ON

#define DEFAULT_MIC_GAIN        	(1.0f)
#define DEFAULT_PLAYBACK_GAIN    	(0.0f)

#define DEFAULT_AUDIO_DSCP		 	(0x2e)  // 48, default for voice (realtime)

#define ENABLE_AUDIO_ADAPTIVE_JITT_COMP 	(1)		// Adative Jitter compensation for audio
#define ENABLE_AUDIO_NO_XMIT_ON_MUTE		(0)		// When audio is muted, no transmission
#define AUDIO_RTP_JITTER_TIME 	 			(100) 	// Nominal audio jitter buffer size in milliseconds
#define NO_RTP_TIMEOUT						(30) 	// RTP timeout in seconds: when no RTP or RTCP

#ifdef HAVE_ILBC
extern "C" void libmsilbc_init();
#endif

extern "C" void libmsilbc_init();
#ifdef HAVE_X264
extern "C" void libmsx264_init();
#endif
#ifdef HAVE_AMR
extern "C" void libmsamr_init();
#endif
#ifdef HAVE_SILK
extern "C" void libmssilk_init();
#endif
#ifdef HAVE_G729
extern "C" void libmsbcg729_init();
#endif


#ifdef ANDROID
#define LOG_DOMAIN "ME_mediastreamer"
static void ME_Android_Log_Handler(OrtpLogLevel lev, const char *fmt, va_list args) {
        int prio;
        switch(lev){
        case ORTP_DEBUG:        prio = ANDROID_LOG_DEBUG;       break;
        case ORTP_MESSAGE:      prio = ANDROID_LOG_INFO;        break;
        case ORTP_WARNING:      prio = ANDROID_LOG_WARN;        break;
        case ORTP_ERROR:        prio = ANDROID_LOG_ERROR;       break;
        case ORTP_FATAL:        prio = ANDROID_LOG_FATAL;       break;
        default:                prio = ANDROID_LOG_DEFAULT;     break;
        }
        __android_log_vprint(prio, LOG_DOMAIN, fmt, args);
}
#endif

// Constructors and destructors
MediaEngine::~MediaEngine()
{
	Uninitialize();
	//TODO delete engine data and free all the data structures contained
	ms_free(mData);
}

MediaEngine::MediaEngine()
{
	mData = ms_new(ME_PrivData, 1);
	memset (mData, 0, sizeof (ME_PrivData));
}

void MediaEngine::Enable_Trace(FILE *file) {
	if (file==NULL)
		file=stdout;
	ortp_set_log_file(file);
	ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
}


// Public functions
void MediaEngine::Initialize()
{
#ifdef DEBUG
	ms_message("**** Enable Debug ******\n");
	ortp_set_log_handler((OrtpLogFunc)ME_Android_Log_Handler);
	ortp_set_log_level_mask(ORTP_DEBUG|ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
#endif //#DEBUG_MEDIAENGINE

	ms_message("Initializing MediaEngine %d.%d", ME_MAJAR_VER, ME_MINOR_VER);

	memset(mData, 0, sizeof (ME_PrivData));

	ms_mutex_init(&mData->mutex,NULL);

#ifdef HAVE_ILBC
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "MediaEngine::Initialize, iLBC");
#endif
	libmsilbc_init(); // requires an fpu
#endif
#ifdef HAVE_X264
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "MediaEngine::Initialize, X264");
#endif
	libmsx264_init();
#endif
#ifdef HAVE_AMR
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "MediaEngine::Initialize, AMR");
#endif
	libmsamr_init();
#endif
#ifdef HAVE_SILK
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "MediaEngine::Initialize, SILK");
#endif
	libmssilk_init();
#endif
#ifdef HAVE_G729
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "MediaEngine::Initialize, G729");
#endif
	libmsbcg729_init();
#endif

	//Initialize oRTP stack
	ortp_init();
	mData->dyn_pt=DYNAMIC_PAYLOAD_TYPE_MIN;
	mData->default_profile=rtp_profile_new("default profile");

#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Assign payload ...");
#endif
	assign_payload_type(&payload_type_pcmu8000, 0, NULL);
	assign_payload_type(&payload_type_gsm,3,NULL);
	assign_payload_type(&payload_type_pcma8000,8,NULL);
	assign_payload_type(&payload_type_speex_nb,110,"vbr=on");
	assign_payload_type(&payload_type_speex_wb,111,"vbr=on");
	assign_payload_type(&payload_type_speex_uwb,112,"vbr=on");
	assign_payload_type(&payload_type_telephone_event,101,"0-11");
	assign_payload_type(&payload_type_g722,9,NULL);
	assign_payload_type(&payload_type_g729,18,"annexb=no");

	//add all payload type for which we don't care about the number
	assign_payload_type(&payload_type_ilbc, 102, "mode=30");
	assign_payload_type(&payload_type_amr,112,"octet-align=1");
	assign_payload_type(&payload_type_amrwb,113,"octet-align=1");
	assign_payload_type(&payload_type_lpc1015,-1,NULL);
	assign_payload_type(&payload_type_g726_16,-1,NULL);
	assign_payload_type(&payload_type_g726_24,-1,NULL);
	assign_payload_type(&payload_type_g726_32,-1,NULL);
	assign_payload_type(&payload_type_g726_40,-1,NULL);
	assign_payload_type(&payload_type_aal2_g726_16,-1,NULL);
	assign_payload_type(&payload_type_aal2_g726_24,-1,NULL);
	assign_payload_type(&payload_type_aal2_g726_32,-1,NULL);
	assign_payload_type(&payload_type_aal2_g726_40,-1,NULL);
	assign_payload_type(&payload_type_silk_nb,-1,NULL);
	assign_payload_type(&payload_type_silk_mb,-1,NULL);
	assign_payload_type(&payload_type_silk_wb,-1,NULL);
	assign_payload_type(&payload_type_silk_swb,-1,NULL);


	//Assign static payloads
	handle_static_payloads();

#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Init MS ...");
#endif
	ms_init();

	// Create a mediastreamer2 event queue and set it as global
	// This allows event's callback
	mData->msevq=ms_event_queue_new();
	ms_set_global_event_queue(mData->msevq);

    mData->rtp_conf.audio_rtp_min_port = 1024;  // Not used yet, take IANA the lowest unofficial port
	mData->rtp_conf.audio_rtp_max_port = 65535; //
	mData->rtp_conf.audio_jitt_comp = AUDIO_RTP_JITTER_TIME;
	mData->rtp_conf.nortp_timeout = NO_RTP_TIMEOUT;
	if (ENABLE_AUDIO_NO_XMIT_ON_MUTE == 0) {
		mData->rtp_conf.rtp_no_xmit_on_audio_mute = FALSE;
	} else {
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "*** NO RTP TRANSMIT ON MUTE  is ON ***");
#endif
		mData->rtp_conf.rtp_no_xmit_on_audio_mute = TRUE; // stop rtp xmit when audio muted
	}
	if (ENABLE_AUDIO_ADAPTIVE_JITT_COMP == 0) {
		mData->rtp_conf.audio_adaptive_jitt_comp_enabled = FALSE;
	} else {
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "*** Audio adaptive jittering is ON ***");
#endif
		mData->rtp_conf.audio_adaptive_jitt_comp_enabled = TRUE;
	}

#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Init sound ...");
#endif
	//init sound device properties
	init_sound();

	mData->state=ME_INITIALIZED;

	// Done, inform the observer
	//if (iObserver)
	//	iObserver->MediaEngineInitCompleted(KErrNone);
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "ME_INITIALIZED!!!");
#endif
}

bool_t MediaEngine::isInitialized() {
	if (mData && mData->state==ME_INITIALIZED)
		return TRUE;
	else
		return FALSE;
}

void MediaEngine::Uninitialize()
{
	if (!mData) return;

	mData->state = ME_TERMINATING;

	while (mData->sessions)
	{
		MediaSession *theSession = (MediaSession *)mData->sessions->data;
		DeleteSession(theSession);

		usleep(50000);
	}

	ms_event_queue_destroy(mData->msevq);
	mData->msevq=NULL;

	uninit_sound();

	free_payload_types();
	ortp_exit();

	ms_mutex_destroy(&mData->mutex);

	mData->state = ME_IDLE;
}

const char* MediaEngine::EngineName() const
{
	return ME_NAME;
}

void MediaEngine::GetEngineVersion(int& aMajor, int& aMinor) const
{
	aMajor = ME_MAJAR_VER;
	aMinor = ME_MINOR_VER;
}


ME_List* MediaEngine::GetAvailableAudioCodecs() const
{
	return mData->payload_types;
}

ME_List* MediaEngine::GetAvaliableVideoCodecs() const {
	return NULL;
}

ME_Codec* MediaEngine::GetCodec(int pt_number, int clk_rate, const char * name) const {
	MSList *elem;
	bool_t isDynamicPt = (pt_number>= DYNAMIC_PAYLOAD_TYPE_MIN && pt_number>= DYNAMIC_PAYLOAD_TYPE_MAX);

	ms_mutex_lock(&mData->mutex);
	for (elem=mData->payload_types;elem!=NULL;elem=elem->next) {
		PayloadType *it=(PayloadType*)elem->data;
		if (!isDynamicPt) {
			if (payload_type_get_number(it)==pt_number) {
				ms_mutex_unlock(&mData->mutex);
				return it;
			}
		}
		else {//TODO dynamic payload, find it by name
			if (payload_type_get_number(it)==pt_number && it && it->clock_rate == clk_rate) {
				ms_mutex_unlock(&mData->mutex);
				return it;
			}
		}
	}
	ms_mutex_unlock(&mData->mutex);
	return NULL;
}

const char* MediaEngine::GetAudioStreamSessionKey(MediaSession* session)
{
	const char *key = NULL;
	ms_mutex_lock(&mData->mutex);
	key = session->as->crypto[0].master_key;
	ms_mutex_unlock(&mData->mutex);
	return key;
}

MediaEngine::MediaSession* MediaEngine::CreateSession()
{
	ms_mutex_lock(&mData->mutex);
	if (ms_list_size(mData->sessions) > ME_MAX_NB_SESSIONS) {
		ms_message("Too many open media sessions!!!");
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Too many open media sessions!!!");
#endif
		ms_mutex_unlock(&mData->mutex);
		return NULL;
	}

	MediaSession* session = ms_new0(MediaSession, 1);
	if (session == NULL) {
		ms_message("Memory error ...");
		ms_mutex_unlock(&mData->mutex);
		return NULL;
	} else {
		session->as = (ME_AudioStream *)ms_new0(ME_AudioStream, 1);
		if (session->as == NULL) {
			ms_message("Memory error ...");
			ms_free(session);
			ms_mutex_unlock(&mData->mutex);
			return NULL;
		}
	}

	preempt_sound_resources();

	session->state = ME_SESSION_IDLE;
	session->media_start_time = 0;
	session->stats[MEDIA_TYPE_AUDIO].type = MEDIA_TYPE_AUDIO;
	session->stats[MEDIA_TYPE_AUDIO].received_rtcp = NULL;
	session->stats[MEDIA_TYPE_AUDIO].sent_rtcp = NULL;
	//generate session key, we support currently only one crypto algo
	session->as->crypto[0].tag = 1;
	session->as->crypto[0].algo = AES_128_SHA1_80;
	if (!generate_b64_crypto_key(30, session->as->crypto[0].master_key))
		session->as->crypto[0].algo = AES_128_NO_AUTH; //misuse the enum value

	if (add_session(session)!= 0)
	{
		ms_warning("Not possible at failing of add_session ... weird!");
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Not possible at failing of add_session ... weird!!!");
#endif
		ms_free(session);
		ms_free(session->as);
		ms_mutex_unlock(&mData->mutex);
		return NULL;
	}

	/* this session becomes now the current one */
	mData->curSession = session;

	ms_mutex_unlock(&mData->mutex);
	return session;
}

int MediaEngine::DeleteSession(MediaSession* session)
{
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_INFO, "*ME", "--->MediaEngine::DeleteSession");
#endif

	//stop streaming
	if (session->state != ME_SESSION_IDLE) {
		StopStreams(session);
	}

	ms_mutex_lock(&mData->mutex);

	MSList *it;
	MSList *theSessions = mData->sessions;

	if (session == mData->curSession) {
		mData->curSession = NULL;
	}

	it=ms_list_find(theSessions, session);
	if (it)
	{
		theSessions = ms_list_remove_link(theSessions,it);
	}
	else
	{
		ms_warning("could not find the call in the list\n");
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Warning, could not find the session in the list!");
#endif

		if (session->audioRecvCodecs) {
			ms_free(session->audioRecvCodecs);
			session->audioRecvCodecs = NULL;
		}
		ms_free(session->as);
	    ms_free(session);
		ms_mutex_unlock(&mData->mutex);
		return -1;
	}
	mData->sessions = theSessions;

	if (session->audioRecvCodecs) {
		ms_free(session->audioRecvCodecs);
		session->audioRecvCodecs = NULL;
	}
	ms_free(session->as);
    ms_free(session);
	ms_mutex_unlock(&mData->mutex);
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_INFO, "*ME", "<---MediaEngine::DeleteSession");
#endif
	return 0;
}

void MediaEngine::InitStreams(MediaSession* session, int local_audio_port, int local_video_port) {

	ms_mutex_lock(&mData->mutex);

	//save the port
	session->audio_port = local_audio_port;

    init_audio_stream(session, local_audio_port);

    audio_stream_prepare_sound(session->as->audiostream, mData->sound_conf.play_sndcard, mData->sound_conf.capt_sndcard);

	ms_mutex_unlock(&mData->mutex);
}

void MediaEngine::StartStreams(MediaSession* session, PayloadType* sendAudioCodec, ME_List* recAudioCodecs, const char *cname, const char *remIp, const int remAudioPort, const int remVideoPort, const bool_t sendAudio, const char* audio_rcv_key) {

	if (session == NULL) {
		ms_message("Session pointer NULL!!!");
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "ME::StartStreams, Session pointer NULL!");
#endif
		return;
	}

	ms_mutex_lock(&mData->mutex);

	if (session->as->audiostream == NULL) {
		ms_message("Audio stream not yet initialized!!!");
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "ME::StartStreams, Audio stream not yet initialized!");
#endif
		ms_mutex_unlock(&mData->mutex);
		return;
	}

	if (session->state == ME_SESSION_AUDIO_STREAMING) {
		ms_message("Audio stream already started!!!");
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "ME::StartStreams, Audio stream already started!");
#endif
		ms_mutex_unlock(&mData->mutex);
		return;
	}

	if (session != mData->curSession) {
		preempt_sound_resources();
	}

	if (session->audio_profile != NULL) {
		//only do it when stream is not yet started
		rtp_profile_clear_all(session->audio_profile);
		rtp_profile_destroy(session->audio_profile);
		session->audio_profile=NULL;
	}

	session->audio_profile = make_profile(recAudioCodecs);
	session->audioSendCodec = sendAudioCodec;


	start_audio_stream(session, cname, remIp, remAudioPort, session->all_muted, ENABLE_ARC, sendAudio, audio_rcv_key);

	ms_mutex_unlock(&mData->mutex);
}

void MediaEngine::StopStreams(MediaSession* session) {
	ms_mutex_lock(&mData->mutex);
	stop_media_streams(session);
	ms_mutex_unlock(&mData->mutex);
}

void MediaEngine::PauseStreams(MediaSession* session) {
	ms_mutex_lock(&mData->mutex);
	if (session->state == ME_SESSION_AUDIO_STREAMING) {
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "PauseStreams ...");
#endif
		pause_audio_stream(session);
	}
	ms_mutex_unlock(&mData->mutex);
}

void MediaEngine::ResumeStreams(MediaSession* session) {
	ms_mutex_lock(&mData->mutex);
	if (session->state == ME_SESSION_AUDIO_STREAMING) {
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "ResumeStreams ...");
#endif
		resume_audio_stream(session);
	}
	ms_mutex_unlock(&mData->mutex);
}

void MediaEngine::UpdateStatistics(MediaSession* session) {
	ms_mutex_lock(&mData->mutex);
	if (session->as->audiostream!=NULL) {
		//pumping
		media_stream_iterate(&session->as->audiostream->ms);
		const MSQualityIndicator* qi= media_stream_get_quality_indicator(&(session->as->audiostream->ms));
		if (qi) {
			session->stats[MEDIA_TYPE_AUDIO].local_loss_rate = ms_quality_indicator_get_local_loss_rate(qi);
			session->stats[MEDIA_TYPE_AUDIO].local_late_rate = ms_quality_indicator_get_local_late_rate(qi);
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "UpdateStatistics");
		char buf[256];
	    sprintf(buf, "UpdateStatistics, lossRate = %4.2f, lateRate=%4.2f\n", session->stats[MEDIA_TYPE_AUDIO].local_loss_rate, session->stats[MEDIA_TYPE_AUDIO].local_late_rate);
	    __android_log_write(ANDROID_LOG_INFO, "*ME_N*", buf);
#endif
		}
	}

	background_tasks(session, 1);

	ms_mutex_unlock(&mData->mutex);
}

/**
 * Mutes or unmute the local microphone.
**/
void MediaEngine::MuteMicphone(bool_t val) {

	ms_mutex_lock(&mData->mutex);

    MediaSession* session = mData->curSession;
    AudioStream *st=NULL;

    if (session==NULL){
    	ms_warning("ME::MuteMicphone(): No current call !");
    	ms_mutex_unlock(&mData->mutex);
        return;
    } else {
    	st=session->as->audiostream;
        session->audio_muted=val;
    }

    if (st!=NULL){
    	audio_stream_set_mic_gain(st, (val==TRUE) ? 0 : pow(10, mData->sound_conf.soft_mic_lev/10));
    	if (is_rtp_no_xmit_on_audio_mute_enabled()) {
    		audio_stream_mute_rtp(st,val);
    	}
    }

	ms_mutex_unlock(&mData->mutex);
}

void MediaEngine::SendDTMF(MediaSession* session, char dtmf) {
	ms_mutex_lock(&mData->mutex);
	send_dtmf(session, dtmf);
	ms_mutex_unlock(&mData->mutex);
}

bool_t MediaEngine::IsMediaStreamStarted(MediaSession* session, int mediaType) {
	bool_t result = FALSE;

	ms_mutex_lock(&mData->mutex);
	if (mediaType == MEDIA_TYPE_AUDIO && session->as->audiostream) {
		result = audio_stream_started(session->as->audiostream);
    }
	ms_mutex_unlock(&mData->mutex);

	return result;
}

void MediaEngine::SetPlaybackGain(float gain) {
	ms_mutex_lock(&mData->mutex);
	mData->sound_conf.soft_play_lev = gain;
#if defined(ANDROID)
		char buf[256];
	    sprintf(buf, "SetPlaybackGain, gain = %2.2f db\n", gain);
	    __android_log_write(ANDROID_LOG_INFO, "*ME_N*", buf);
#endif
	ms_mutex_unlock(&mData->mutex);
}

// Private functions
void MediaEngine::init_audio_stream(MediaSession *session, int local_port) {
	AudioStream *audiostream;
	int dscp; //what shall we do with DSCP?

	if (session->as->audiostream != NULL) return;

	//save the port
	session->audio_port = local_port;

	session->as->audiostream=audiostream=audio_stream_new(session->audio_port,session->audio_port+1, FALSE); //no IPv6 support
	//set default DSCP
	audio_stream_set_dscp(audiostream, DEFAULT_AUDIO_DSCP);

	if (is_echo_limiter_enabled()){
		if (mData->ecs == ME_ECS_HIGH) {
			audio_stream_enable_echo_limiter(audiostream, ELControlFull);
		} else if (mData->ecs == ME_ECS_NORMAL) {
			audio_stream_enable_echo_limiter(audiostream, ELControlMic);
		}
	}
	audio_stream_enable_gain_control(audiostream, TRUE);
	if (is_echo_cancellation_enabled()) {
		audio_stream_set_echo_canceller_params(audiostream, EC_TAIL_LEN, EC_DELAY, EC_FRAME_SIZE);
		const char *statestr = mData->echo_canceller_state_str;
		if (statestr && audiostream->ec) {
			ms_filter_call_method(audiostream->ec,MS_ECHO_CANCELLER_SET_STATE_STRING,(void*)statestr);
		}
	}

	audio_stream_enable_automatic_gain_control(audiostream, is_agc_enabled());

	if (is_ng_enabled()) {
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "*** NG enabled ***");
#endif
		audio_stream_enable_noise_gate(audiostream, TRUE);
	}

	//TODO check whether we need this
	rtp_session_set_pktinfo(audiostream->ms.session, TRUE);

	session->audiostream_app_evq = ortp_ev_queue_new();
	rtp_session_register_event_queue(audiostream->ms.session, session->audiostream_app_evq);
}

void MediaEngine::start_audio_stream(MediaSession* session, const char *cname, const char *remIp, const int remport, bool_t muted, bool_t use_arc,
		bool_t sendAudio, const char* rcv_key) {
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "start_audio_stream ...");
#endif

	int used_pt=-1;
	char rtcp_tool[128]={0};

	MSSndCard *playcard=mData->sound_conf.play_sndcard;
	MSSndCard *captcard=mData->sound_conf.capt_sndcard;

	bool_t use_ec;

	if (session->audio_profile && session->audioSendCodec) {
		//set tempo status
		session->state =ME_SESSION_AUDIO_STREAM_STARTING;

		if (playcard==NULL) {
			ms_warning("No card defined for playback !");
#if defined(ANDROID)
			__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "No card defined for playback !");
#endif
		}
		if (captcard==NULL) {
			ms_warning("No card defined for capture !");
#if defined(ANDROID)
			__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "No card defined for capture !");
#endif
		}

		if (session != mData->curSession) {
			ms_message("Sound resources are used by another call, not using soundcard.");
#if defined(ANDROID)
			__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Sound resources are used by another call, not using soundcards!");
#endif
			captcard=playcard=NULL;
		}
		use_ec=captcard==NULL ? FALSE : is_echo_cancellation_enabled();

		used_pt = payload_type_get_number(session->audioSendCodec);

		audio_stream_enable_adaptive_bitrate_control(session->as->audiostream, use_arc);
		audio_stream_enable_adaptive_jittcomp(session->as->audiostream, is_audio_adaptive_jittcomp_enabled());
		int result = audio_stream_start_now(
				session->as->audiostream,
				session->audio_profile, remIp, remport, remport+1,
				used_pt,
				mData->rtp_conf.audio_jitt_comp,
				playcard,
				captcard,
				use_ec);

#if defined(ANDROID)
		char buf[256];
		sprintf(buf, "Calling audio_stream_start_now with used_pt=%d, result=%d", used_pt, result);
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", buf);
#endif

		post_configure_audio_stream(session);

		if (muted) {
			audio_stream_set_mic_gain(session->as->audiostream, 0);
		}

		if (!sendAudio) {
			audio_stream_mute_rtp(session->as->audiostream, false);
		}

		audio_stream_set_rtcp_information(session->as->audiostream, cname, rtcp_tool);

		if (rcv_key) {
			 audio_stream_enable_srtp(session->as->audiostream,
					 session->as->crypto[0].algo,
					 session->as->crypto[0].master_key,
			         rcv_key);
			 session->audiostream_encrypted=TRUE;
		}

		session->state = ME_SESSION_AUDIO_STREAMING;
	} else if (!session->audio_profile){
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "*** No RTP profile ***");
#endif
	} else if (!session->audioSendCodec){
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "*** No send codec set ***");
#endif
	}
}

void MediaEngine::stop_audio_stream(MediaSession *session) {
	if (session->as->audiostream != NULL) {
		rtp_session_unregister_event_queue(session->as->audiostream->ms.session, session->audiostream_app_evq);
		ortp_ev_queue_flush(session->audiostream_app_evq);
		ortp_ev_queue_destroy(session->audiostream_app_evq);
		session->audiostream_app_evq=NULL;

		if (session->as->audiostream->ec) {
			const char *state_str=NULL;
			ms_filter_call_method(session->as->audiostream->ec, MS_ECHO_CANCELLER_GET_STATE_STRING, &state_str);
			if (state_str){
				ms_message("Writing echo canceler state, %i bytes",(int)strlen(state_str));
				if (mData->echo_canceller_state_str)
					free(mData->echo_canceller_state_str);
				mData->echo_canceller_state_str =ortp_strdup(state_str);
			}
		}

		audio_stream_stop(session->as->audiostream);
		session->as->audiostream=NULL;
		//TODO clean up as
	}
}

void MediaEngine::pause_audio_stream(MediaSession* session) {
	if (session->as->audiostream != NULL) {
		 audio_stream_mute_rtp(session->as->audiostream, true);
	}
}

void MediaEngine::resume_audio_stream(MediaSession* session) {
	if (session->as->audiostream != NULL) {
		 audio_stream_mute_rtp(session->as->audiostream, false);
	}
}

void MediaEngine::stop_media_streams(MediaSession *session)
{
	if (session->state == ME_SESSION_AUDIO_STREAMING) {
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "stop_media_streams ...");
#endif
		session->state = ME_SESSION_TERMINATING;
		stop_audio_stream(session);

		ms_event_queue_skip(mData->msevq);

		if (session->audio_profile) {
			rtp_profile_clear_all(session->audio_profile);
			rtp_profile_destroy(session->audio_profile);
			session->audio_profile=NULL;
		}

		session->state = ME_SESSION_IDLE;
	}
#if defined(ANDROID)
	else {
		char buf[256];
		sprintf(buf, "stop_media_streams at wrong state %d\n",session->state);
		__android_log_write(ANDROID_LOG_INFO, "*ME_N*", buf);
	}
#endif
}

void MediaEngine::enable_echo_cancellation(const MediaSession* session, bool_t enable)
{
	if (session != NULL && session->as->audiostream!=NULL && session->as->audiostream->ec) {
		bool_t bypass_mode = !enable;
		ms_filter_call_method(session->as->audiostream->ec, MS_ECHO_CANCELLER_SET_BYPASS_MODE, &bypass_mode);
	}
}

bool_t MediaEngine::is_echo_cancellation_enabled() {
	return mData->sound_conf.ec;
}

bool_t MediaEngine::is_echo_limiter_enabled() {
	return mData->sound_conf.ea;
}

bool_t MediaEngine::is_agc_enabled() {
	return mData->sound_conf.agc;
}

bool_t MediaEngine::is_ng_enabled() {
	return mData->sound_conf.ng;
}

bool_t MediaEngine::is_audio_adaptive_jittcomp_enabled() {
	return mData->rtp_conf.audio_adaptive_jitt_comp_enabled;
}

bool_t MediaEngine::is_rtp_no_xmit_on_audio_mute_enabled() {
	return mData->rtp_conf.rtp_no_xmit_on_audio_mute;
}


void MediaEngine::enable_echo_limiter(const MediaSession *session, bool_t val, bool_t isFull)
{
	if (session!=NULL && session->as->audiostream!=NULL ) {
		if (val) {
			if (isFull)
				audio_stream_enable_echo_limiter(session->as->audiostream, ELControlFull);
			else
				audio_stream_enable_echo_limiter(session->as->audiostream, ELControlMic);
		} else {
			audio_stream_enable_echo_limiter(session->as->audiostream, ELInactive);
		}
	}
}

/**
 * Send the specified dtmf.
 *
 * This function only works during calls. The dtmf is automatically played to the user.
 * @param session,  The media session object
 * @param dtmf The dtmf name specified as a char, such as '0', '#' etc...
 *
**/
void MediaEngine::send_dtmf(const MediaSession* session, char dtmf)
{
	if (session == NULL) {
		ms_warning("ME::send_dtmf(): media session is NULL!");
		return;
	}

	/* In Band DTMF */
	if (session->as->audiostream != NULL) {
		ms_message("ME::send_dtmf(): %d\n", dtmf);
		audio_stream_send_dtmf(session->as->audiostream, dtmf);
	}
	else {
		ms_error("we cannot send RFC2833 dtmf when we are not in communication");
	}
}

void MediaEngine::assign_payload_type(PayloadType *const_pt, int number, const char *recv_fmtp) {
	if (!mData) return;

	PayloadType *pt;
	pt=payload_type_clone(const_pt);
	if (number==-1) {
		/*look for a free number */
		MSList *elem;
		int i;
		for(i=mData->dyn_pt;i<RTP_PROFILE_MAX_PAYLOADS;++i) {
			bool_t already_assigned=FALSE;
			for (elem=mData->payload_types;elem!=NULL;elem=elem->next) {
				PayloadType *it=(PayloadType*)elem->data;
				if (payload_type_get_number(it)==i) {
					already_assigned=TRUE;
					break;
				}
			}
			if (!already_assigned) {
				number=i;
				mData->dyn_pt=i+1;
				break;
			}
		}
		if (number==-1){
			ms_fatal("FIXME: too many codecs, no more free numbers.");
		}
	}
	ms_message("assigning %s/%i payload type number %i",pt->mime_type,pt->clock_rate,number);
#if defined(ANDROID)
	char buf[256];
	sprintf(buf, "assigning %s/%i payload type number %i",pt->mime_type,pt->clock_rate,number);
	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", buf);
#endif
	payload_type_set_number(pt,number);
	if (recv_fmtp!=NULL)
		payload_type_set_recv_fmtp(pt,recv_fmtp);
	rtp_profile_set_payload(mData->default_profile,number,pt);
	mData->payload_types=ms_list_append(mData->payload_types,pt);
}

void MediaEngine::handle_static_payloads() {
	if (mData) {
		RtpProfile *prof=&av_profile;
		int i;
		for(i=0;i<RTP_PROFILE_MAX_PAYLOADS;++i) {
			PayloadType *pt=rtp_profile_get_payload(prof,i);
			if (pt) {
				if (payload_type_get_number(pt)!=i) {
					assign_payload_type(pt,i,NULL);
				}
			}
		}
	}
}

void MediaEngine::free_payload_types() {
	if (mData) {
		rtp_profile_clear_all(mData->default_profile);
		rtp_profile_destroy(mData->default_profile);
		ms_list_for_each(mData->payload_types,(void (*)(void*))payload_type_destroy);
		ms_list_free(mData->payload_types);
		mData->payload_types=NULL;
	}
}

void MediaEngine::init_sound()
{
	int tmp;
	const char *tmpbuf;
	const char *devid;

	// retrieve all sound devices
	build_sound_devices_table();

    //set default sound cards
    MSSndCardManager* cardManager = ms_snd_card_manager_get();
    if (cardManager == NULL) {
        ms_message("Unable to initialize sound cards. MSSndCardManager == NULL");
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_ERROR, "*ME_N*", "Unable to initialize sound cards. MSSndCardManager == NULL");
#endif
    }
	mData->sound_conf.capt_sndcard = ms_snd_card_manager_get_default_capture_card(cardManager);
    if (mData->sound_conf.capt_sndcard == NULL) {
        ms_message("Unable to initialize capture sound card.");
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_ERROR, "*ME_N*", "Unable to initialize capture sound card.");
#endif
    } else {
#if defined(ANDROID)
    	char buf[256];
    	sprintf(buf, "Card '%s' selected for capture", mData->sound_conf.capt_sndcard->name);
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", buf);
#endif
        ms_message("Card '%s' selected for capture", mData->sound_conf.capt_sndcard->name);
    }

	mData->sound_conf.play_sndcard = ms_snd_card_manager_get_default_playback_card(cardManager);
    if (mData->sound_conf.play_sndcard == NULL) {
        ms_message("Unable to initialize playout sound card.");
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_ERROR, "*ME_N*", "Unable to initialize playout sound card.");
#endif
    } else {
#if defined(ANDROID)
    	char buf[256];
    	sprintf(buf, "Card '%s' selected for playout", mData->sound_conf.play_sndcard->name);
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", buf);
#endif
        ms_message("Card '%s' selected for playout", mData->sound_conf.play_sndcard->name);
    }

	if (ENABLE_ECHO_CANCELLATION == 0) {
		mData->sound_conf.ec = FALSE;
	} else {
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_INFO, "*ME_N*", "*** Echo Cancellation  is ON ***");
#endif
		mData->sound_conf.ec = TRUE;
	}

	if (ENABLE_ECHO_LIMITER == 0) {
		mData->sound_conf.ea = FALSE;
	} else {
		mData->sound_conf.ea = TRUE;
	}

	mData->sound_conf.latency=0;

	if (ENABLE_AGC == 0) {
		mData->sound_conf.agc = FALSE;
	} else {
		mData->sound_conf.agc = TRUE;
	}

	if (ENABLE_NOISE_GATE == 0) {
		mData->sound_conf.ng = FALSE;
	} else {
		mData->sound_conf.ng = TRUE;
	}

	mData->sound_conf.soft_mic_lev = DEFAULT_MIC_GAIN; //
	mData->sound_conf.soft_play_lev = DEFAULT_PLAYBACK_GAIN; //
}

void MediaEngine::uninit_sound()
{
	ms_free(mData->sound_conf.cards);

	ms_snd_card_manager_destroy();
}


void MediaEngine::build_sound_devices_table() {
	const char **devices;
	const char **old;
	int ndev;
	int i;
	const MSList *elem=ms_snd_card_manager_get_list(ms_snd_card_manager_get());
	ndev = ms_list_size(elem);
	devices = (const char **)ms_malloc((ndev+1)*sizeof(const char *));
	for (i=0; elem!=NULL; elem=elem->next,i++) {
		devices[i]=ms_snd_card_get_string_id((MSSndCard *)elem->data);
	}
	devices[ndev]=NULL;
	old=mData->sound_conf.cards;
	mData->sound_conf.cards=devices;
	if (old!=NULL)
		ms_free(old);
}

/**
 * Allow to control play level before entering sound card: gain in db
**/
void MediaEngine::set_playback_gain_db (MediaSession *session, float gaindb) {
	if (session) {
		float gain=gaindb;
		AudioStream *st = session->as->audiostream;

		if (st==NULL) {
			ms_message("set_playback_gain_db(): session has no active audio stream.");
			return;
		}
		if (st->volrecv){
			ms_filter_call_method(st->volrecv,MS_VOLUME_SET_DB_GAIN, &gain);
		}else
			ms_warning("Could not apply gain: gain control wasn't activated.");
	}
}

/**
 * Set sound play back level
 * @param level,  in 0-100 scale
**/
void MediaEngine::set_play_level(int level) {
	if (mData) {
		MSSndCard *sndcard;
		mData->sound_conf.play_lev = level;
		sndcard=mData->sound_conf.play_sndcard;
		if (sndcard)
			ms_snd_card_set_level(sndcard,MS_SND_CARD_PLAYBACK,level);
	}
}

/**
 * Set sound capture level
  * @param level,  in 0-100 scale
**/
void MediaEngine::set_rec_level(int level) {
	if (mData) {
		MSSndCard *sndcard;
		mData->sound_conf.rec_lev=level;
		sndcard=mData->sound_conf.capt_sndcard;
		if (sndcard)
			ms_snd_card_set_level(sndcard, MS_SND_CARD_CAPTURE,level);
	}
}

int MediaEngine::add_session(MediaSession* session)
{
	//TODO lock engine
	mData->sessions = ms_list_append(mData->sessions, session);
	return 0;
}

int MediaEngine::delete_session(MediaSession* session)
{
	//TODO lock engine
	MSList* it;
	MSList* sessions = mData->sessions;

	it = ms_list_find(mData->sessions, session);
	if (it)
	{
		sessions = ms_list_remove_link(mData->sessions, it);
	}
	else
	{
		ms_warning("could not find the media session into the list\n");
		return -1;
	}
	mData->sessions = sessions;
	return 0;
}

void MediaEngine::preempt_sound_resources() {
	MediaSession* current_session = mData->curSession;
	if(current_session != NULL) {
#if defined(ANDROID)
		__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Stop automatically the current media session ...");
#endif
		ms_message("Stop automatically the current media session ...");
		stop_media_streams(current_session);
		mData->curSession = NULL;
	}
}

RtpProfile* MediaEngine::make_profile(MSList* payloads) {

	RtpProfile *prof=rtp_profile_new("Call profile");
	int number = -1;

	const MSList *elem;

	for (elem=payloads; elem!=NULL; elem=elem->next) {
		PayloadType *pt = (PayloadType*)elem->data;
		if (pt) {//sanity
			int number=payload_type_get_number(pt);
			if (rtp_profile_get_payload(prof, number) != NULL) {
				ms_warning("A payload type with number %i already exists in profile !",number);
#if defined(ANDROID)
				char buf[256];
				sprintf(buf, "Warning: A payload type with number %i already exists in profile !",number);
				__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", buf);
#endif
			} else {
				rtp_profile_set_payload(prof, number, pt);
			}
		}
	}
	return prof;
}

void MediaEngine::post_configure_audio_stream(const MediaSession* session) {

	if (session && session->as->audiostream) {
		float mic_gain = mData->sound_conf.soft_mic_lev;
		float thres = 0;
		float recv_gain;
		float ng_thres = NG_THRESHOLD;
		float ng_floorgain = NG_FLOOR_GAIN;
		AudioStream* st = session->as->audiostream;

		int dc_removal = ENABLE_DC_REMOVAL;

		if (!session->audio_muted)
			set_mic_gain_db(mic_gain);
		else
			audio_stream_set_mic_gain(session->as->audiostream, 0);

		recv_gain = mData->sound_conf.soft_play_lev;
		if (recv_gain != 0) {
			set_playback_gain_db(recv_gain);
		}

		if (st->volsend){
			ms_filter_call_method(st->volsend, MS_VOLUME_REMOVE_DC, &dc_removal);

			float speed = EL_SPEED;
			thres= EL_THRESHOLD;
			float force = EL_FORCE;
			int sustain = EL_SUSTAIN;

			MSFilter *f=NULL;
			f=st->volsend;
			if (speed==-1) speed=0.03;
			if (force==-1) force=25;
			ms_filter_call_method(f,MS_VOLUME_SET_EA_SPEED,&speed);
			ms_filter_call_method(f,MS_VOLUME_SET_EA_FORCE,&force);
			if (thres!=-1)
				ms_filter_call_method(f,MS_VOLUME_SET_EA_THRESHOLD,&thres);
			if (sustain!=-1)
				ms_filter_call_method(f,MS_VOLUME_SET_EA_SUSTAIN,&sustain);

			ms_filter_call_method(st->volsend,MS_VOLUME_SET_NOISE_GATE_THRESHOLD,&ng_thres);
			ms_filter_call_method(st->volsend,MS_VOLUME_SET_NOISE_GATE_FLOORGAIN,&ng_floorgain);
		}
		if (st->volrecv){
			/* parameters for a limited noise-gate effect, using echo limiter threshold */
			float floorgain = 1/pow(10,(mic_gain)/10);
			int spk_agc= 0; //speaker agc
			ms_filter_call_method(st->volrecv, MS_VOLUME_ENABLE_AGC, &spk_agc);
			ms_filter_call_method(st->volrecv, MS_VOLUME_SET_NOISE_GATE_THRESHOLD,&ng_thres);
			ms_filter_call_method(st->volrecv, MS_VOLUME_SET_NOISE_GATE_FLOORGAIN,&floorgain);
		}
	}
}

/**
 * Allow to control microphone level:  gain in db
 *
**/
void MediaEngine::set_mic_gain_db(float gaindb){
	float gain=gaindb;
	MediaSession* session = mData->curSession;
	AudioStream *st;

	mData->sound_conf.soft_mic_lev=gaindb;

	if (session == NULL || (st=session->as->audiostream) == NULL) {
		ms_message("ME::set_mic_gain_db(): no active call.");
		return;
	}
	if (st->volsend){
		ms_filter_call_method(st->volsend, MS_VOLUME_SET_DB_GAIN, &gain);
	}else
		ms_warning("Could not apply gain: gain control wasn't activated.");
}

/**
 * Allow to control play level before entering sound card:  gain in db
 *
**/
void MediaEngine::set_playback_gain_db(float gaindb){
	float gain=gaindb;
	MediaSession* session = mData->curSession;
    AudioStream *st;

    mData->sound_conf.soft_play_lev=gaindb;

    if (session==NULL || (st=session->as->audiostream)==NULL){
    	ms_message("ME::set_playback_gain_db(): no active call.");
    	return;
    }
    if (st->volrecv){
    	ms_filter_call_method(st->volrecv, MS_VOLUME_SET_DB_GAIN, &gain);
    }else
    	ms_warning("Could not apply gain: gain control wasn't activated.");
}

void MediaEngine::background_tasks(MediaSession *session, bool_t one_second_elapsed) {

	int disconnect_timeout = mData->rtp_conf.nortp_timeout;
	bool_t disconnected=FALSE;

	if (session->state == ME_SESSION_AUDIO_STREAMING && one_second_elapsed){
		RtpSession *as = NULL;
		float audio_load=0;
		if (session->as->audiostream!=NULL){
			as=session->as->audiostream->ms.session;
			if (session->as->audiostream->ms.ticker)
				audio_load=ms_ticker_get_average_load(session->as->audiostream->ms.ticker);
		}
		//report_bandwidth(session, as);
		ms_message("Thread processing load: audio=%f",audio_load);
	}

	if (session->as->audiostream != NULL) {
		OrtpEvent *ev;

		// Beware that the application queue should not depend on treatments from the
		// media streamer queue.
		audio_stream_iterate(session->as->audiostream);

		while (session->audiostream_app_evq && (NULL != (ev=ortp_ev_queue_get(session->audiostream_app_evq)))){
			OrtpEventType evt=ortp_event_get_type(ev);
			OrtpEventData *evd=ortp_event_get_data(ev);
			if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				session->stats[MEDIA_TYPE_AUDIO].round_trip_delay = rtp_session_get_round_trip_propagation(session->as->audiostream->ms.session);
				if(session->stats[MEDIA_TYPE_AUDIO].received_rtcp != NULL)
					freemsg(session->stats[MEDIA_TYPE_AUDIO].received_rtcp);
				session->stats[MEDIA_TYPE_AUDIO].received_rtcp = evd->packet;
				evd->packet = NULL;

			} else if (evt == ORTP_EVENT_RTCP_PACKET_EMITTED) {
				memcpy(&session->stats[MEDIA_TYPE_AUDIO].jitter_stats,
						rtp_session_get_jitter_stats(session->as->audiostream->ms.session),
						sizeof(jitter_stats_t));
				if(session->stats[MEDIA_TYPE_AUDIO].sent_rtcp != NULL)
					freemsg(session->stats[MEDIA_TYPE_AUDIO].sent_rtcp);
				session->stats[MEDIA_TYPE_AUDIO].sent_rtcp = evd->packet;
				evd->packet = NULL;

			} else if ((evt == ORTP_EVENT_ICE_SESSION_PROCESSING_FINISHED) || (evt == ORTP_EVENT_ICE_GATHERING_FINISHED)
				|| (evt == ORTP_EVENT_ICE_LOSING_PAIRS_COMPLETED) || (evt == ORTP_EVENT_ICE_RESTART_NEEDED)) {
				//TODO, should'not come here
			} else if (evt==ORTP_EVENT_TELEPHONE_EVENT){
				//We don support dtmf receiving
				//handle_dtmf_received(evd->info.telephone_event);
			} else if (evt == ORTP_EVENT_ZRTP_ENCRYPTION_CHANGED){
				handle_audiostream_encryption_changed(session, evd->info.zrtp_stream_encrypted);
			} else if (evt == ORTP_EVENT_ZRTP_SAS_READY) {
				handle_audiostream_auth_token_ready(session, evd->info.zrtp_sas.sas, evd->info.zrtp_sas.verified);
			}

			ortp_event_destroy(ev);
		}
	}
	if (session->state == ME_SESSION_AUDIO_STREAMING && one_second_elapsed && session->as->audiostream!=NULL && disconnect_timeout>0 )
		disconnected = !audio_stream_alive(session->as->audiostream, disconnect_timeout);
	if (disconnected)
		handle_media_disconnected(session);
}

void MediaEngine::handle_media_disconnected(MediaSession* session) {
	if (session) {
		//TODO
	}
}

void MediaEngine::handle_audiostream_encryption_changed(MediaSession* session, bool_t encrypted) {
	char status[255]={0};
	ms_message("Audio stream is %s ", encrypted ? "encrypted" : "not encrypted");

	session->audiostream_encrypted=encrypted;
}

void MediaEngine::handle_audiostream_auth_token_ready(MediaSession* session, const char* auth_token, bool_t verified) {
	if (session->auth_token != NULL)
		ms_free(session->auth_token);

	session->auth_token=ms_strdup(auth_token);
	session->auth_token_verified=verified;

	ms_message("Authentication token is %s (%s)", auth_token, verified?"verified":"unverified");
}

bool_t MediaEngine::generate_b64_crypto_key(int key_length, char* key_out) {
        int b64_size;
        uint8_t* tmp = (uint8_t*) malloc(key_length);
        if (ortp_crypto_get_random(tmp, key_length)!=0) {
                ms_error("Failed to generate random key");
                free(tmp);
                return FALSE;
        }

        b64_size = B64_NAMESPACE::b64_encode((const char*)tmp, key_length, NULL, 0);
        if (b64_size == 0) {
                ms_error("Failed to b64 encode key");
                free(tmp);
                return FALSE;
        }
        key_out[b64_size] = '\0';
        B64_NAMESPACE::b64_encode((const char*)tmp, key_length, key_out, 40);
        free(tmp);
        return TRUE;
}


