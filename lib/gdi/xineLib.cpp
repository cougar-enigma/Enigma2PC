#include <lib/gdi/xineLib.h>
#include <lib/base/eenv.h>

cXineLib   *cXineLib::instance;

DEFINE_REF(cXineLib);

cXineLib::cXineLib(x11_visual_t *vis) : m_pump(eApp, 1) {
	char        configfile[300];
	char        vo_driver[] = "vdpau";
	char        ao_driver[] = "alsa";

	instance = this;
	osd = NULL;
	stream = NULL;
	end_of_stream = false;
	videoPlayed = false;

	printf("XINE-LIB version: %s\n", xine_get_version_string() );

	xine = xine_new();
	sprintf(configfile, "%s%s", xine_get_homedir(), "/.xine/config");
	printf("configfile  %s\n", configfile);
	xine_config_load(xine, configfile);
	xine_init(xine);

	if((vo_port = xine_open_video_driver(xine, vo_driver, XINE_VISUAL_TYPE_X11, (void *) vis)) == NULL)
	{
		printf("I'm unable to initialize '%s' video driver. Giving up.\n", vo_driver);
		return;
	}

	ao_port     = xine_open_audio_driver(xine , ao_driver, NULL);
	stream      = xine_stream_new(xine, ao_port, vo_port);

	if ( (!xine_open(stream, eEnv::resolve("${sysconfdir}/tuxbox/logo.mvi").c_str()))
			|| (!xine_play(stream, 0, 0)) ) {
		return;
	}

	//vo_port->set_property(vo_port, VO_PROP_INTERLACED, 1);
	vo_port->set_property(vo_port, VO_PROP_INTERLACED, 4);

	//setPrebuffer(150000);
	//xine_engine_set_param(xine, XINE_ENGINE_PARAM_VERBOSITY, XINE_VERBOSITY_DEBUG);

	xine_queue = xine_event_new_queue (stream);
	xine_event_create_listener_thread(xine_queue, xine_event_handler, this);

	CONNECT(m_pump.recv_msg, cXineLib::pumpEvent);

	m_width     = 0;
	m_height    = 0;
	m_framerate = 0;
	m_aspect    = -1;
}

cXineLib::~cXineLib() {
	instance = 0;

	if (stream)
	{
		xine_stop(stream);
		xine_close(stream);

		if (xine_queue)
		{
			xine_event_dispose_queue(xine_queue);
			xine_queue = 0;
		}

		_x_demux_flush_engine(stream);

		xine_dispose(stream);
		stream = NULL;
	}

	if (ao_port)
		xine_close_audio_driver(xine, ao_port);
	if (vo_port)
		xine_close_video_driver(xine, vo_port);
}

void cXineLib::setVolume(int value) {
	xine_set_param (stream, XINE_PARAM_AUDIO_VOLUME, value);
}

void cXineLib::setVolumeMute(int value) {
	xine_set_param (stream, XINE_PARAM_AUDIO_MUTE, value==0?0:1);
}

void cXineLib::showOsd() {
	xine_osd_show_scaled(osd, 0);
	//stream->osd_renderer->draw_bitmap(osd, (uint8_t*)m_surface.data, 0, 0, 720, 576, temp_bitmap_mapping);
}

void cXineLib::newOsd(int width, int height, uint32_t *argb_buffer) {
	osdWidth  = width;
	osdHeight = height;

	if (osd)
		xine_osd_free(osd);

	osd = xine_osd_new(stream, 0, 0, osdWidth, osdHeight);
	xine_osd_set_extent(osd, osdWidth, osdHeight);
	xine_osd_set_argb_buffer(osd, argb_buffer, 0, 0, osdWidth, osdHeight);
}

void cXineLib::playVideo(void) {
	//xine_stop(stream);
	end_of_stream = false;
	videoPlayed = false;

	printf("XINE try START !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	if ( !xine_open(stream, "enigma:/") ) {
		printf("Unable to open stream !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}

	setStreamType(1);
	setStreamType(0);

        //_x_demux_control_start(stream);
        //_x_demux_seek(stream, 0, 0, 0);

	if( !xine_play(stream, 0, 0) ) {
		printf("Unable to play stream !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}
	printf("XINE STARTED !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

	videoPlayed = true;
}

void cXineLib::stopVideo(void) {
	xine_stop(stream);
	end_of_stream = false;
	videoPlayed = false;
}

void cXineLib::setStreamType(int video) {
	xine_event_t event;

	if (video) {
		event.type = XINE_EVENT_SET_VIDEO_STREAMTYPE;
		event.data = &videoData;
	} else {
		event.type = XINE_EVENT_SET_AUDIO_STREAMTYPE;
		event.data = &audioData;
	}

	event.data_length = sizeof (xine_streamtype_data_t);

	xine_event_send (stream, &event);
}

void cXineLib::setVideoType(int pid, int type) {
	videoData.pid = pid;
	videoData.streamtype = type;
}

void cXineLib::setAudioType(int pid, int type) {
	audioData.pid = pid;
	audioData.streamtype = type;
}

void cXineLib::setPrebuffer(int prebuffer) {
	xine_set_param(stream, XINE_PARAM_METRONOM_PREBUFFER, prebuffer);
}

void cXineLib::xine_event_handler(void *user_data, const xine_event_t *event)
{
	cXineLib *xineLib = (cXineLib*)user_data;
	//if (event->type!=15)
	//	printf("I have event %d\n", event->type);

	switch (event->type)
	{
	case XINE_EVENT_UI_PLAYBACK_FINISHED:
		printf("XINE_EVENT_UI_PLAYBACK_FINISHED\n");
		break;
	case XINE_EVENT_NBC_STATS:
		return;
	case XINE_EVENT_FRAME_FORMAT_CHANGE:
		printf("XINE_EVENT_FRAME_FORMAT_CHANGE\n");
		{
			xine_format_change_data_t* data = (xine_format_change_data_t*)event->data;
			printf("width %d  height %d  aspect %d\n", data->width, data->height, data->aspect);

			struct iTSMPEGDecoder::videoEvent evt;
			evt.type = iTSMPEGDecoder::videoEvent::eventSizeChanged;
			xineLib->m_aspect = evt.aspect = data->aspect;
			xineLib->m_height = evt.height = data->height;
			xineLib->m_width  = evt.width  = data->width;
			xineLib->m_pump.send(evt);
		}
		return;
	case XINE_EVENT_FRAMERATE_CHANGE:
		printf("XINE_EVENT_FRAMERATE_CHANGE\n");
		{
			xine_framerate_data_t* data = (xine_framerate_data_t*)event->data;
			printf("framerate %d  \n", data->framerate);

			struct iTSMPEGDecoder::videoEvent evt;
			evt.type = iTSMPEGDecoder::videoEvent::eventFrameRateChanged;
			xineLib->m_framerate = evt.framerate = data->framerate;
			xineLib->m_pump.send(evt);
		}
		return;
	case XINE_EVENT_PROGRESS:
		{
			xine_progress_data_t* data = (xine_progress_data_t*) event->data;
			printf("XINE_EVENT_PROGRESS  %s  %d\n", data->description, data->percent);
			if (xineLib->videoPlayed && data->percent==0)
				xineLib->end_of_stream = true;
		}
		break;

	default:
		printf("xine_event_handler(): event->type: %d\n", event->type);
		return;
	}
}

void cXineLib::pumpEvent(const iTSMPEGDecoder::videoEvent &event)
{
	m_event(event);
}

int cXineLib::getVideoWidth()
{
	return m_width;
}

int cXineLib::getVideoHeight()
{
	return m_height;
}

int cXineLib::getVideoFrameRate()
{
	return m_framerate;
}

int cXineLib::getVideoAspect()
{
	return m_aspect;
}

RESULT cXineLib::getPTS(pts_t &pts)
{
	pts_t* last_pts_l = (pts_t*)vo_port->get_property(vo_port, VO_PROP_LAST_PTS);

	pts = *last_pts_l;

	if (pts != 0)
		return 0;
	
	return -1;
}

void cXineLib::setVideoWindow(int window_x, int window_y, int window_width, int window_height)
{
	int left = window_x * windowWidth / osdWidth;
	int top = window_y * windowHeight / osdHeight;
	int width = window_width * windowWidth / osdWidth;
	int height = window_height * windowHeight / osdHeight;

	xine_osd_set_video_window(osd, left, top, width, height);
	showOsd();
}

void cXineLib::updateWindowSize(int width, int height)
{
	windowWidth  = width;
	windowHeight = height;
}

