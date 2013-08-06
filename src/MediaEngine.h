#ifndef __MEDIA_ENGINE_H__
#define __MEDIA_ENGINE_H__

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/mscommon.h>
#include <ortp/ortp_srtp.h>


#define ME_NAME	"NGTI MediaEngine"
#define ME_MAJAR_VER 	(1)
#define ME_MINOR_VER    (0)

#define ME_MAX_NB_SESSIONS (8)

#define ME_MUTEX 			ms_mutex_t
#define ME_MUTEX_INIT  		ms_mutex_init
#define ME_MUTEX_LOCK  		ms_mutex_lock
#define ME_MUTEX_UNLOCK     ms_mutex_unlock
#define ME_MUTEX_DESTROY  	ms_mutex_destroy

#define ME_List 			MSList
#define ME_List_Append      ms_list_append
#define ME_LIST_Find		ms_list_find
#define ME_List_nth_Data    ms_list_nth_data
#define ME_List_Free        ms_list_free
#define ME_LIST_SIZE 		ms_list_size

#define ME_Codec			PayloadType
#define ME_Codec_New		payload_type_new
#define ME_Codec_Clone		payload_type_clone

#define MEDIA_TYPE_AUDIO  	(0)
#define MEDIA_TYPE_VIDEO	(1)

#define STREAM_CRYPTO_ALGO_MAX (4)

#define payload_type_set_number(pt,n)   (pt)->user_data=(void*)((long)n);
#define payload_type_get_number(pt)     ((int)(long)(pt)->user_data)


class MediaEngine
{
public:
	typedef enum _ME_state{
		ME_IDLE,
		ME_INITIALIZED,
		ME_TERMINATING
	} ME_state;

	typedef enum _ME_MediaSessionState{
		ME_SESSION_IDLE,
		ME_SESSION_AUDIO_STREAM_STARTING,
		ME_SESSION_AUDIO_STREAMING,
		ME_SESSION_TERMINATING
	} ME_MediaSessionState;

	typedef enum _ME_Echo_Supression {
		ME_ECS_NONE,
		ME_ECS_NORMAL,
		ME_ECS_HIGH
	} ME_Echo_Supression;

	/**
	 * The MediaStats objects carries various statistic informations regarding quality of audio or video streams.
	 *
	 * To receive these informations periodically and as soon as they are computed, the application is invited to place a
	 *
	 * At any time, the application can access last computed statistics using get_audio_stats() or get_video_stats().
	**/
	typedef struct _MediaStats {
		int		type; /* Can be either MEDIA_TYPE_AUDIO or MEDIA_TYPE_VIDEO */
		jitter_stats_t	jitter_stats; /* jitter buffer statistics, see oRTP documentation for details */
		mblk_t*		received_rtcp; /* last RTCP packet received, as a mblk_t structure. See oRTP documentation for details how to extract information from it*/
		mblk_t*		sent_rtcp;	/* last RTCP packet sent, as a mblk_t structure. See oRTP documentation for details how to extract information from it */
		float		round_trip_delay; /* Round trip propagation time in seconds if known, -1 if unknown. */
		float download_bandwidth; /* download bandwidth measurement of received stream, expressed in kbit/s, including IP/UDP/RTP headers */
		float upload_bandwidth; /* download bandwidth measurement of sent stream, expressed in kbit/s, including IP/UDP/RTP headers */
		float local_late_rate; /**<percentage of packet received too late over last second*/
		float local_loss_rate; /**<percentage of lost packet over last second*/
	} MediaStats;

	typedef enum {
		ME_StreamSendRecv,
		ME_StreamSendOnly,
		ME_StreamRecvOnly,
		ME_StreamInactive
	} ME_StreamDir;

	typedef struct _ME_SrtpCryptoAlgo {
		unsigned int tag;
		enum ortp_srtp_crypto_suite_t algo;
		/* 41= 40 max(key_length for all algo) + '\0' */
		char master_key[41];
	} ME_SrtpCryptoAlgo;

	typedef struct _ME_AudioStream {
		struct _AudioStream *audiostream;

		ME_StreamDir dir;
		ME_SrtpCryptoAlgo crypto[STREAM_CRYPTO_ALGO_MAX];

		unsigned int crypto_local_tag;

		bool_t encrypted;

	}ME_AudioStream;

	typedef struct _MediaSession
	{
		struct _RtpProfile *audio_profile;
		time_t media_start_time; //time at which media streams are established
		ME_MediaSessionState	state;
		void* user_pointer;
		int audio_port;

		ME_AudioStream *as;
		bool_t audiostream_encrypted;

		char *auth_token;
		bool_t auth_token_verified;

		PayloadType*	audioSendCodec;
		MSList *		audioRecvCodecs; //payload not owned


		int audio_bw;	/*upload bandwidth used by audio */

		OrtpEvQueue *audiostream_app_evq;

		MediaStats stats[2];
		bool_t audio_muted;

		bool_t all_muted; /*this flag is set during early medias*/

	} MediaSession;

	typedef struct rtp_config
	{
		int audio_rtp_min_port;
		int audio_rtp_max_port;
		int audio_jitt_comp;  //jitter compensation
		int nortp_timeout;
		bool_t rtp_no_xmit_on_audio_mute; // stop rtp xmit when audio muted
		bool_t audio_adaptive_jitt_comp_enabled;
	} rtp_config_t;

	typedef struct sound_config
	{
		struct _MSSndCard * play_sndcard;	// the playback sndcard currently used
		struct _MSSndCard * capt_sndcard; 	// the capture sndcard currently used
		const char **cards;
		int latency;			// latency in samples of the current used sound device
		float soft_play_lev; 	//playback gain in db.
		float soft_mic_lev; 	//mic gain in db.
		char rec_lev;
		char play_lev;
		//char ring_lev;
		char source;
		bool_t ec;		//echo cancellation
		bool_t ea;		//echo limiter (AES)
		bool_t ng; 		//noise gate
		bool_t agc;		//automatic gain control
	} sound_config_t;

	typedef struct _ME_PrivData
	{
		ME_state state;
		RtpProfile *default_profile;
		rtp_config_t rtp_conf;
		sound_config_t sound_conf;
		MSList *payload_types; // all available codecs
		int dyn_pt;

		MediaSession *curSession;   // the current media session
		MSList *sessions;		 // all the active media session

		struct _MSEventQueue *msevq;

		MSList *bl_reqs;
		void *data;

		time_t prevtime;
		int audio_bw;

		int max_calls;
		char* device_id;

		ME_Echo_Supression ecs;

		char *echo_canceller_state_str;

		ms_mutex_t mutex; //ME lock

	} ME_PrivData;


public:	// Constructors and destructors

	MediaEngine();

	~MediaEngine();

public:	// Public functions

	void Enable_Trace(FILE *file);

	void Initialize();

	bool_t isInitialized();

	virtual void Uninitialize();

	virtual const char* EngineName() const;

	virtual void GetEngineVersion(int& aMajor, int& aMinor) const;

	virtual ME_List* GetAvailableAudioCodecs() const;

	virtual ME_Codec* GetCodec(int pt_number, int clk_rate, const char * name) const;

	virtual ME_List* GetAvaliableVideoCodecs() const;

	virtual const char* GetAudioStreamSessionKey(MediaSession* session); //now default only the AES_128_SHA1_80 algo supported

	virtual MediaSession* CreateSession();

	virtual int DeleteSession(MediaSession* session);

	virtual void InitStreams(MediaSession* session, int local_audio_port, int local_video_port = -1);

	virtual void StartStreams(MediaSession* session, PayloadType* sendAudioCodec, ME_List* recAudioCodecs, const char *cname, const char *remIp, const int remAudioPort, const int remVideoPort =-1, const bool_t sendAudio = TRUE, const char* audio_rcv_key=NULL);

	virtual void StopStreams(MediaSession* session);

	virtual void PauseStreams(MediaSession* session);

	virtual void ResumeStreams(MediaSession* session);

	virtual void UpdateStatistics(MediaSession* session);

	virtual void MuteMicphone(bool_t val);

	virtual void SendDTMF(MediaSession* session, char dtmf);

	virtual bool_t IsMediaStreamStarted(MediaSession* session, int mediaType);

	virtual void SetPlaybackGain(float gain);

private: // Private functions

	void init_sound();

	void uninit_sound();

	void build_sound_devices_table();

	void assign_payload_type(PayloadType *const_pt, int number, const char *recv_fmtp);

	void handle_static_payloads();

	void free_payload_types();

	void stop_media_streams(MediaSession* session);

	void enable_echo_limiter(const MediaSession* session, bool_t val, bool_t isFull);
	bool_t is_echo_limiter_enabled();

	void enable_echo_cancellation(const MediaSession* session, bool_t enable);
	bool_t is_echo_cancellation_enabled();

	bool_t is_agc_enabled();
	bool_t is_ng_enabled();

	bool_t is_audio_adaptive_jittcomp_enabled();
	bool_t is_rtp_no_xmit_on_audio_mute_enabled();

	//Audio stream
	void preempt_sound_resources();
	void init_audio_stream(MediaSession* session, int local_port);
	void start_audio_stream(MediaSession* session, const char *cname, const char *remIp, const int remport, bool_t muted, bool_t use_arc, bool_t sendAudio, const char* rcv_key);
	void stop_audio_stream(MediaSession* session);
	void pause_audio_stream(MediaSession* session);
	void resume_audio_stream(MediaSession* session);

	// DTMF tone
	void send_dtmf(const MediaSession* session, char dtmf);

	int add_session(MediaSession* session);
	int delete_session(MediaSession* session);

	//Voice play back (DTMF and ring tone optionally)
	void set_play_level(int level);
	void set_rec_level(int level);
	void set_playback_gain_db (MediaSession* session, float gaindb);

	//util function to make a Rtp profile out of selected codecs
	RtpProfile* make_profile(MSList* payloads);

	void post_configure_audio_stream(const MediaSession* session);

	void set_mic_gain_db(float gaindb);
	void set_playback_gain_db(float gaindb);

	// RTP events handling
	void background_tasks(MediaSession* session, bool_t one_second_elapsed);

	void handle_media_disconnected(MediaSession* session);
	void handle_audiostream_encryption_changed(MediaSession* session, bool_t encrypted);
	void handle_audiostream_auth_token_ready(MediaSession* session, const char* auth_token, bool_t verified);

	static bool_t generate_b64_crypto_key(int key_length, char* key_out);

private: // Data

	ME_PrivData* mData;
};
#endif // MediaEngine
