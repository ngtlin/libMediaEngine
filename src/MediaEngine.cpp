#include "MediaEngine.h"
#include "mediastreamer2/msvolume.h"
#include "mediastreamer2/msequalizer.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msjpegwriter.h"
#include "mediastreamer2/mseventqueue.h"
#include "mediastreamer2/mssndcard.h"


#include <math.h>


#include <ortp/rtp.h>

#include <android/log.h>


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
#define AUDIO_RTP_JITTER_TIME		(60)  	// 60 milliseconds
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
#define AUDIO_RTP_JITTER_TIME 	 			(60) 	// Nominal audio jitter buffer size in milliseconds
#define NO_RTP_TIMEOUT						(30) 	// RTP timeout in seconds: when no RTP or RTCP


#define payload_type_set_number(pt,n)   (pt)->user_data=(void*)((long)n);
#define payload_type_get_number(pt)     ((int)(long)(pt)->user_data)


extern "C" void libmsilbc_init();

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

// Public functions
void MediaEngine::Initialize()
{
	ms_message("Initializing MediaEngine %d.%d", ME_MAJAR_VER, ME_MINOR_VER);
	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "MediaEngine::Initialize, iLBC");
	memset(mData, 0, sizeof (ME_PrivData));

	//initialize iLBC plugin
	libmsilbc_init();

	//Initialize oRTP stack
	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Init oRTP ...");
	ortp_init();
	mData->dyn_pt=DYNAMIC_PAYLOAD_TYPE_MIN;
	mData->default_profile=rtp_profile_new("default profile");

	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Assign payload ...");
	assign_payload_type(&payload_type_pcmu8000, 0, NULL);
	assign_payload_type(&payload_type_gsm,3,NULL);
	assign_payload_type(&payload_type_pcma8000,8,NULL);
	assign_payload_type(&payload_type_speex_nb,110,"vbr=on");
	assign_payload_type(&payload_type_speex_wb,111,"vbr=on");
	assign_payload_type(&payload_type_speex_uwb,112,"vbr=on");
	assign_payload_type(&payload_type_telephone_event,101,"0-11");
	assign_payload_type(&payload_type_g722,9,NULL);

	//add all payload type for which we don't care about the number
	assign_payload_type(&payload_type_ilbc,-1,"mode=30");
	assign_payload_type(&payload_type_amr,-1,"octet-align=1");
	assign_payload_type(&payload_type_amrwb,-1,"octet-align=1");
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
	assign_payload_type(&payload_type_g729,18,"annexb=no");

	//Assign static payloads
	handle_static_payloads();

	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Init MS ...");
	ms_init();

	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Create EVQ ...");
	// Create a mediastreamer2 event queue and set it as global
	// This allows event's callback
	mData->msevq=ms_event_queue_new();
	ms_set_global_event_queue(mData->msevq);

	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Set rtp conf...");

    mData->rtp_conf.audio_rtp_min_port = 1024;  // Not used yet, take IANA the lowest unofficial port
	mData->rtp_conf.audio_rtp_max_port = 65535; //
	mData->rtp_conf.audio_jitt_comp = AUDIO_RTP_JITTER_TIME;
	mData->rtp_conf.nortp_timeout = NO_RTP_TIMEOUT;
	if (ENABLE_AUDIO_NO_XMIT_ON_MUTE == 0) {
		mData->rtp_conf.rtp_no_xmit_on_audio_mute = FALSE;
	} else {
		mData->rtp_conf.rtp_no_xmit_on_audio_mute = TRUE; // stop rtp xmit when audio muted
	}
	if (ENABLE_AUDIO_ADAPTIVE_JITT_COMP == 0) {
		mData->rtp_conf.audio_adaptive_jitt_comp_enabled = FALSE;
	} else {
		mData->rtp_conf.audio_adaptive_jitt_comp_enabled = TRUE;
	}

	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "Init sound ...");
	//init sound device properties
	init_sound();

	mData->state=ME_INITIALIZED;

	// Done, inform the observer
	//if (iObserver)
	//	iObserver->MediaEngineInitCompleted(KErrNone);
	__android_log_write(ANDROID_LOG_DEBUG, "*ME_N*", "ME_INITIALIZED!!!");
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
		terminate_session(theSession);

		usleep(50000);
	}

	ms_event_queue_destroy(mData->msevq);
	mData->msevq=NULL;

	uninit_sound();

	free_payload_types();
	ortp_exit();

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

ME_Codec* MediaEngine::GetCodec(int pt_number) const {
	MSList *elem;
	for (elem=mData->payload_types;elem!=NULL;elem=elem->next) {
		PayloadType *it=(PayloadType*)elem->data;
		if (payload_type_get_number(it)==pt_number) {
			return it;
		}
	}
	return NULL;
}

MediaEngine::MediaSession* MediaEngine::CreateSession()
{
	if (ms_list_size(mData->sessions) > ME_MAX_NB_SESSIONS) {
		ms_message("Too many opens media sessions!!!");
		return NULL;
	}

	MediaSession* session = ms_new0(MediaSession, 1);
	if (session == NULL) {
		ms_message("Memory error ...");
		return NULL;
	}

	preempt_sound_resources();

	session->state = ME_SESSION_IDLE;
	session->media_start_time = 0;
	session->stats[MEDIA_TYPE_AUDIO].type = MEDIA_TYPE_AUDIO;
	session->stats[MEDIA_TYPE_AUDIO].received_rtcp = NULL;
	session->stats[MEDIA_TYPE_AUDIO].sent_rtcp = NULL;

	if (add_session(session)!= 0)
	{
		ms_warning("Not possible at failing of add_session ... weird!");
		ms_free(session);
		return NULL;
	}

	/* this session becomes now the current one */
	mData->curSession = session;

	return session;
}

int MediaEngine::DeleteSession(MediaSession* session)
{
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
		ms_warning("could not find the call into the list\n");
		return -1;
	}
	mData->sessions = theSessions;

    ms_free(session);

	return 0;
}

void MediaEngine::InitStreams(MediaSession* session, int local_audio_port, int local_video_port) {

	//save the port
	session->audio_port = local_audio_port;

    init_audio_stream(session, local_audio_port);

    audio_stream_prepare_sound(session->audiostream, mData->sound_conf.play_sndcard, mData->sound_conf.capt_sndcard);
}

void MediaEngine::StartStreams(MediaSession* session, PayloadType* sendAudioCodec, ME_List* recAudioCodecs, const char *cname, const char *remIp, const int remAudioPort, const int remVideoPort) {
	if (session == NULL) {
		ms_message("Session pointer NULL!!!");
		return;
	}

	if (session->audiostream == NULL) {
		ms_message("Audio stream not yet initialized!!!");
		return;
	}

	if (session->state == ME_SESSION_AUDIO_STREAMING) {
		ms_message("Audio stream already started!!!");
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

	start_audio_stream(session, cname, remIp, remAudioPort, session->all_muted, ENABLE_ARC);
}

void MediaEngine::StopStreams(MediaSession* session) {
	stop_media_streams(session);
}

/**
 * Mutes or unmute the local microphone.
**/
void MediaEngine::MuteMicphone(bool_t val) {

    MediaSession* session = mData->curSession;
    AudioStream *st=NULL;

    if (session==NULL){
    	ms_warning("ME::MuteMicphone(): No current call !");
        return;
    } else {
    	st=session->audiostream;
        session->audio_muted=val;
    }

    if (st!=NULL){
    	audio_stream_set_mic_gain(st, (val==TRUE) ? 0 : pow(10, mData->sound_conf.soft_mic_lev/10));
    	if (is_rtp_no_xmit_on_audio_mute_enabled()) {
    		audio_stream_mute_rtp(st,val);
    	}
    }
}

void MediaEngine::SendDTMF(MediaSession* session, char dtmf) {
	send_dtmf(session, dtmf);
}

bool_t MediaEngine::IsMediaStreamStarted(MediaSession* session, int mediaType) {
	if (mediaType == MEDIA_TYPE_AUDIO && session->audiostream) {
    	return audio_stream_started(session->audiostream);
    }
    return FALSE;
}

// Private functions
void MediaEngine::init_audio_stream(MediaSession *session, int local_port) {
	AudioStream *audiostream;
	int dscp; //what shall we do with DSCP?

	if (session->audiostream != NULL) return;

	//save the port
	session->audio_port = local_port;

	session->audiostream=audiostream=audio_stream_new(session->audio_port,session->audio_port+1, FALSE); //no IPv6 support
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
		audio_stream_enable_noise_gate(audiostream, TRUE);
	}

	//TODO check whether we need this
	rtp_session_set_pktinfo(audiostream->session, TRUE);

	session->audiostream_app_evq = ortp_ev_queue_new();
	rtp_session_register_event_queue(audiostream->session, session->audiostream_app_evq);
}

void MediaEngine::start_audio_stream(MediaSession* session, const char *cname, const char *remIp, const int remport, bool_t muted, bool_t use_arc) {

	int used_pt=-1;
	char rtcp_tool[128]={0};

	MSSndCard *playcard=mData->sound_conf.play_sndcard;
	MSSndCard *captcard=mData->sound_conf.capt_sndcard;

	bool_t use_ec;

	if (session->audio_profile && session->audioSendCodec) {
		if (playcard==NULL) {
			ms_warning("No card defined for playback !");
		}
		if (captcard==NULL) {
			ms_warning("No card defined for capture !");
		}

		if (session != mData->curSession) {
			ms_message("Sound resources are used by another call, not using soundcard.");
			captcard=playcard=NULL;
		}
		use_ec=captcard==NULL ? FALSE : is_echo_cancellation_enabled();

		audio_stream_enable_adaptive_bitrate_control(session->audiostream, use_arc);
		audio_stream_enable_adaptive_jittcomp(session->audiostream, is_audio_adaptive_jittcomp_enabled());
		int result = audio_stream_start_now(
				session->audiostream,
				session->audio_profile, remIp, remport, remport+1,
				used_pt,
				mData->rtp_conf.audio_jitt_comp,
				playcard,
				captcard,
				use_ec);

		post_configure_audio_stream(session);

		if (muted){
			audio_stream_set_mic_gain(session->audiostream, 0);
		}

		audio_stream_set_rtcp_information(session->audiostream, cname, rtcp_tool);

		session->state = ME_SESSION_AUDIO_STREAMING;
	}
}

void MediaEngine::stop_audio_stream(MediaSession *session) {
	if (session->audiostream != NULL) {
		rtp_session_unregister_event_queue(session->audiostream->session, session->audiostream_app_evq);
		ortp_ev_queue_flush(session->audiostream_app_evq);
		ortp_ev_queue_destroy(session->audiostream_app_evq);
		session->audiostream_app_evq=NULL;

		if (session->audiostream->ec) {
			const char *state_str=NULL;
			ms_filter_call_method(session->audiostream->ec, MS_ECHO_CANCELLER_GET_STATE_STRING, &state_str);
			if (state_str){
				ms_message("Writing echo canceler state, %i bytes",(int)strlen(state_str));
				if (mData->echo_canceller_state_str)
					free(mData->echo_canceller_state_str);
				mData->echo_canceller_state_str =ortp_strdup(state_str);
			}
		}

		audio_stream_stop(session->audiostream);
		session->audiostream=NULL;
	}
}

void MediaEngine::stop_media_streams(MediaSession *session)
{
	stop_audio_stream(session);

	ms_event_queue_skip(mData->msevq);

	if (session->audio_profile) {
		rtp_profile_clear_all(session->audio_profile);
		rtp_profile_destroy(session->audio_profile);
		session->audio_profile=NULL;
	}

	session->state = ME_SESSION_IDLE;
}

void MediaEngine::enable_echo_cancellation(const MediaSession* session, bool_t enable)
{
	if (session != NULL && session->audiostream!=NULL && session->audiostream->ec) {
		bool_t bypass_mode = !enable;
		ms_filter_call_method(session->audiostream->ec, MS_ECHO_CANCELLER_SET_BYPASS_MODE, &bypass_mode);
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
	if (session!=NULL && session->audiostream!=NULL ) {
		if (val) {
			if (isFull)
				audio_stream_enable_echo_limiter(session->audiostream, ELControlFull);
			else
				audio_stream_enable_echo_limiter(session->audiostream, ELControlMic);
		} else {
			audio_stream_enable_echo_limiter(session->audiostream, ELInactive);
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
	if (session->audiostream != NULL) {
		audio_stream_send_dtmf(session->audiostream, dtmf);
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

/**
 * Terminates a media session
 *
 * @param the_session the MedaiSession object representing the media session(call) to be terminated.
**/
int MediaEngine::terminate_session(MediaSession* theSession)
{
	//TODO lock engine
	MediaSession *session = NULL;
	if (theSession == NULL) {
		session = mData->curSession;
		if (ms_list_size(mData->sessions)==1){
			session=(MediaSession*)mData->sessions->data;
		} else {
			ms_warning("No unique call to terminate !");
			return -1;
		}
	}
	else {
		session = theSession;
	}

	//stop the streams
	stop_media_streams(session);
	session->state = ME_SESSION_IDLE;

	return 0;
}

void MediaEngine::init_sound()
{
	int tmp;
	const char *tmpbuf;
	const char *devid;

	// retrieve all sound devices
	build_sound_devices_table();

    //set ring sound card
	//mData->sound_conf.ring_sndcard = ms_snd_card_manager_get_default_playback_card(ms_snd_card_manager_get());

	if (ENABLE_ECHO_CANCELLATION == 0) {
		mData->sound_conf.ec = FALSE;
	} else {
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
	mData->sound_conf.soft_play_lev = DEFAULT_MIC_GAIN; //

	/*just parse requested stream feature once at start to print out eventual errors*/
	//linphone_core_get_audio_features(lc);
}

void MediaEngine::uninit_sound()
{
	ms_free(mData->sound_conf.cards);

	//if (config->local_ring)
	//	ms_free(config->local_ring);
	//if (config->remote_ring)
	//	ms_free(config->remote_ring);
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
		AudioStream *st = session->audiostream;

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
		ms_message("Stop automatically the current media session ...");
		if (current_session->state == ME_SESSION_AUDIO_STREAMING) {
			stop_media_streams(current_session);
			current_session->state = ME_SESSION_IDLE;
		}

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
			} else {
				rtp_profile_set_payload(prof, number, pt);
			}
		}
	}
	return prof;
}

void MediaEngine::post_configure_audio_stream(const MediaSession* session) {

	if (session && session->audiostream) {
		float mic_gain = mData->sound_conf.soft_mic_lev;
		float thres = 0;
		float recv_gain;
		float ng_thres = NG_THRESHOLD;
		float ng_floorgain = NG_FLOOR_GAIN;
		AudioStream* st = session->audiostream;

		int dc_removal = ENABLE_DC_REMOVAL;

		if (!session->audio_muted)
			set_mic_gain_db(mic_gain);
		else
			audio_stream_set_mic_gain(session->audiostream, 0);

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

	if (session == NULL || (st=session->audiostream) == NULL) {
		ms_message("ME::set_mic_gain_db(): no active call.");
		return;
	}
	if (st->volrecv){
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

    if (session==NULL || (st=session->audiostream)==NULL){
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
		if (session->audiostream!=NULL){
			as=session->audiostream->session;
			if (session->audiostream->ticker)
				audio_load=ms_ticker_get_average_load(session->audiostream->ticker);
		}
		//report_bandwidth(session, as);
		ms_message("Thread processing load: audio=%f",audio_load);
	}

	if (session->audiostream != NULL) {
		OrtpEvent *ev;

		// Beware that the application queue should not depend on treatments from the
		// media streamer queue.
		audio_stream_iterate(session->audiostream);

		while (session->audiostream_app_evq && (NULL != (ev=ortp_ev_queue_get(session->audiostream_app_evq)))){
			OrtpEventType evt=ortp_event_get_type(ev);
			OrtpEventData *evd=ortp_event_get_data(ev);
			if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				session->stats[MEDIA_TYPE_AUDIO].round_trip_delay = rtp_session_get_round_trip_propagation(session->audiostream->session);
				if(session->stats[MEDIA_TYPE_AUDIO].received_rtcp != NULL)
					freemsg(session->stats[MEDIA_TYPE_AUDIO].received_rtcp);
				session->stats[MEDIA_TYPE_AUDIO].received_rtcp = evd->packet;
				evd->packet = NULL;

			} else if (evt == ORTP_EVENT_RTCP_PACKET_EMITTED) {
				memcpy(&session->stats[MEDIA_TYPE_AUDIO].jitter_stats,
						rtp_session_get_jitter_stats(session->audiostream->session),
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
				//hanlde_audiostream_encryption_changed(session, evd->info.zrtp_stream_encrypted);

			} else if (evt == ORTP_EVENT_ZRTP_SAS_READY) {
				//handle_audiostream_auth_token_ready(session, evd->info.zrtp_sas.sas, evd->info.zrtp_sas.verified);
			}

			ortp_event_destroy(ev);
		}
	}
	if (session->state == ME_SESSION_AUDIO_STREAMING && one_second_elapsed && session->audiostream!=NULL && disconnect_timeout>0 )
		disconnected = !audio_stream_alive(session->audiostream, disconnect_timeout);
	if (disconnected)
		handle_media_disconnected(session);
}

void MediaEngine::handle_media_disconnected(MediaSession* session) {
	if (session) {
		//TODO
	}
}



// End of File
