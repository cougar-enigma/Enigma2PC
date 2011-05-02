#include <lib/gdi/gxlibdc.h>
#include <lib/actions/action.h>
#include <lib/base/init.h>
#include <lib/base/init_num.h>
#include <lib/driver/input_fake.h>
#include <lib/driver/rcxlib.h>

gXlibDC   *gXlibDC::instance;
Display   *display;
Window    window;
int       gXlibDC::width, gXlibDC::height;
double    gXlibDC::pixel_aspect;

gXlibDC::gXlibDC() : m_pump(eApp, 1)
{
	double      res_h, res_v;
	char        mrl[] = "/usr/local/etc/tuxbox/logo.avi";
	
	CONNECT(m_pump.recv_msg, gXlibDC::pumpEvent);

	vo_driver    = "vdpau";
	ao_driver    = "alsa";

	osd = NULL;
	surface = NULL;
	fullscreen = 0;
	width        = 720;
	height       = 576;
	windowWidth  = 720;
	windowHeight = 576;

	ASSERT(instance == 0);
	instance = this;
	
	if(!XInitThreads())
	{
		eFatal("XInitThreads() failed\n");
		return;
	}

	xine = xine_new();
	sprintf(configfile, "%s%s", xine_get_homedir(), "/.xine/config");
	printf("configfile  %s\n", configfile);
	xine_config_load(xine, configfile);
	xine_init(xine);

	if((display = XOpenDisplay(getenv("DISPLAY"))) == NULL) {
		eFatal("XOpenDisplay() failed.\n");
		return;
	}

	screen       = XDefaultScreen(display);

	if (fullscreen)	{
		windowWidth  = width  = DisplayWidth( display, screen );
		windowHeight = height = DisplayHeight( display, screen );
	} else {
		windowWidth  = width  = 720;
		windowHeight = height = 576;
	}

	XLockDisplay(display);
 	window = XCreateSimpleWindow(display, XDefaultRootWindow(display), 0, 0, width, height, 1, 0, 0);
	XSelectInput (display, window, INPUT_MOTION);
	XMapRaised(display, window);
	res_h = (DisplayWidth(display, screen) * 1000 / DisplayWidthMM(display, screen));
	res_v = (DisplayHeight(display, screen) * 1000 / DisplayHeightMM(display, screen));
	XSync(display, False);
	XUnlockDisplay(display);

	printf("Display resolution %d %d\n", DisplayWidth(display, screen), DisplayHeight(display, screen));

	vis.display           = display;
	vis.screen            = screen;
	vis.d                 = window;
	vis.dest_size_cb      = dest_size_cb;
	vis.frame_output_cb   = frame_output_cb;
	vis.user_data         = NULL;
	pixel_aspect          = res_v / res_h;

	if(fabs(pixel_aspect - 1.0) < 0.01)
		pixel_aspect = 1.0;

	if((vo_port = xine_open_video_driver(xine, vo_driver, XINE_VISUAL_TYPE_X11, (void *) &vis)) == NULL)
	{
		printf("I'm unable to initialize '%s' video driver. Giving up.\n", vo_driver);
		return;
	}

	ao_port     = xine_open_audio_driver(xine , ao_driver, NULL);
	stream      = xine_stream_new(xine, ao_port, vo_port);

	setResolution(width, height); // default res

	if((!xine_open(stream, mrl)) || (!xine_play(stream, 0, 0))) {
		printf("Unable to open MRL !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		return;
	}

	vo_port->set_property(vo_port, 0, 1);

	run();

	/*m_surface.type = 0;
	m_surface.clut.colors = 256;
	m_surface.clut.data = new gRGB[m_surface.clut.colors];

	m_pixmap = new gPixmap(&m_surface);

	memset(m_surface.clut.data, 0, sizeof(*m_surface.clut.data)*m_surface.clut.colors);*/
}

gXlibDC::~gXlibDC()
{
	instance = 0;

	//pushEvent(EV_QUIT);
	//kill();
	//SDL_Quit();
	xine_dispose(stream);

	if (ao_port)
		xine_close_audio_driver(xine, ao_port);
	if (vo_port)
		xine_close_video_driver(xine, vo_port);  
	xine_exit(xine);
  
	XLockDisplay(display);
	XUnmapWindow(display, window);
	XDestroyWindow(display, window);
	XUnlockDisplay(display);

	XCloseDisplay (display);
}

void gXlibDC::keyEvent(const XKeyEvent &event)
{
	eXlibInputDriver *driver = eXlibInputDriver::getInstance();

	eDebug("SDL Key %s: key=%d", (event.type == KeyPress) ? "Down" : "Up", event.keycode);

	if (driver)
		driver->keyPressed(event);
}

void gXlibDC::pumpEvent(const XKeyEvent &event)
{
	switch (event.type) {
	case KeyPress:
	case KeyRelease:
		if (event.keycode!=95)
			keyEvent(event);
		else if (event.type==KeyPress)
			fullscreen_switch();
		break;
	//case SDL_QUIT:
	//	eDebug("SDL Quit");
	//	extern void quitMainloop(int exit_code);
	//	quitMainloop(0);
	//	break;
	}
}

/*void gSDLDC::pushEvent(enum event code, void *data1, void *data2)
{
	SDL_Event event;

	event.type = SDL_USEREVENT;
	event.user.code = code;
	event.user.data1 = data1;
	event.user.data2 = data2;

	SDL_PushEvent(&event);
}*/

void gXlibDC::exec(const gOpcode *o)
{
	switch (o->opcode)
	{
	case gOpcode::flush:
		eDebug("FLUSH");
		//stream->osd_renderer->draw_bitmap(osd, (uint8_t*)m_surface.data, 0, 0, 720, 576, temp_bitmap_mapping);
		stream->osd_renderer->show_scaled(osd, 0);
		break;
	default:
		gDC::exec(o);
		break;
	}
}

void gXlibDC::setResolution(int xres, int yres)
{
	printf("setResolution %d %d\n", xres, yres);
	windowWidth  = width  = xres;
	windowHeight = height = yres;

	if (osd)
		stream->osd_renderer->free_object(osd);
	osd = stream->osd_renderer->new_object (stream->osd_renderer, windowWidth, windowHeight);
	stream->osd_renderer->set_extent(osd, windowWidth, windowHeight);
	//stream->osd_renderer->set_video_window(osd, 100, 100, 200, 200);

	if (surface)
		delete [] surface;
	surface = new uint32_t[xres*yres];

	stream->osd_renderer->set_argb_buffer(osd, (uint32_t*)surface, 0, 0, windowWidth, windowHeight);

	m_surface.type = 0;
	m_surface.x = xres;
	m_surface.y = yres;
	m_surface.bpp = 32;
	m_surface.bypp = 4;
	m_surface.stride = xres*4;
	m_surface.data = surface;
	m_surface.offset = 0;

	m_pixmap = new gPixmap(&m_surface);

	if (!fullscreen)
		updateWindowState();
}

void gXlibDC::updateWindowState() {
	if (fullscreen)	{
		width  = DisplayWidth( display, screen );
		height = DisplayHeight( display, screen );
	} else {
		width  = windowWidth;
		height = windowHeight;
	}

	XEvent xev;
	Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
	Atom fullscreenAtom = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = window;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = fullscreen ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = fullscreenAtom;
	xev.xclient.data.l[2] = 0;
	XSendEvent(display, XDefaultRootWindow(display), False, SubstructureNotifyMask, &xev);

	if (!fullscreen)
		XResizeWindow(display, window, width, height);

	XFlush(display);
	stream->osd_renderer->show_scaled(osd, 0);
}

void gXlibDC::fullscreen_switch() {
	printf("FULLSCREEN EVENT\n");
	fullscreen ^= 1;
	updateWindowState();
}

void gXlibDC::evFlip()
{
	//SDL_Flip(m_screen);
}

void gXlibDC::thread()
{
	hasStarted();

	int x11_fd = ConnectionNumber(display);
	bool stop = false;
	fd_set in_fds;
	struct timeval tv;
	XEvent event;

	while (!stop) {
		FD_ZERO(&in_fds);
		FD_SET(x11_fd, &in_fds);

		tv.tv_usec = 0;
		tv.tv_sec = 1;

		if (select(x11_fd+1, &in_fds, 0, 0, &tv))
			printf("Event Received!\n");
		//else
		//	printf("Timer Fired!\n");

		while(XPending(display))
		{
			XNextEvent(display, &event);
			printf("XNextEvent %d\n", event.type);
			switch(event.type)
			{
			case KeyPress:
			case KeyRelease:
				XKeyEvent& xKeyEvent = (XKeyEvent&)event;
				m_pump.send(xKeyEvent);
				break;
			}
		}
	}
}

void gXlibDC::frame_output_cb(void *data, int video_width, int video_height, double video_pixel_aspect,
		int *dest_x, int *dest_y, int *dest_width, int *dest_height, double *dest_pixel_aspect,
		int *win_x, int *win_y)
{
	*dest_x            = 0;
	*dest_y            = 0;
	*win_x             = 0;
	*win_y             = 0;
	*dest_width        = gXlibDC::width;
	*dest_height       = gXlibDC::height;
	*dest_pixel_aspect = gXlibDC::pixel_aspect;
}

void gXlibDC::dest_size_cb(void *data, int video_width, int video_height, double video_pixel_aspect,
			 int *dest_width, int *dest_height, double *dest_pixel_aspect)
{
	*dest_width        = gXlibDC::width;
	*dest_height       = gXlibDC::height;
	*dest_pixel_aspect = gXlibDC::pixel_aspect;
}

void *xine_start_thread_function( void *arg ) {
	gXlibDC *xlibDC = gXlibDC::getInstance();

	xine_close(xlibDC->stream);
	if ( !xine_open(xlibDC->stream, "fifo://tmp/ENIGMA_FIFO") ) {
	//if ( !xine_open(stream, "fifo://dev/dvb/adapter0/dvr0") ) {
		printf("Unable to open stream !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}

	xlibDC->setStreamType(1);
	xlibDC->setStreamType(0);

	if( !xine_play(xlibDC->stream, 0, 0) ) {
		printf("Unable to play stream !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}
	printf("XINE STARTED !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

}

void gXlibDC::videoStart(int pid, int type) {
	if (xine_start_thread != NULL) {
		pthread_cancel(xine_start_thread);
		xine_start_thread = NULL;
	}
	pthread_create( &xine_start_thread, NULL, xine_start_thread_function, NULL);
}

void gXlibDC::videoStop(void) {
	if (xine_start_thread != NULL) {
		pthread_cancel(xine_start_thread);
		xine_start_thread = NULL;
	}
}

void gXlibDC::setVideoType(int pid, int type) {
	videoData.pid = pid;
	videoData.streamtype = type;
}

void gXlibDC::setAudioType(int pid, int type) {
	audioData.pid = pid;
	audioData.streamtype = type;
}

void gXlibDC::setStreamType(int video) {
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

eAutoInitPtr<gXlibDC> init_gXlibDC(eAutoInitNumbers::graphic-1, "gXlibDC");
