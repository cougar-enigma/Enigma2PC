#ifndef __lib_gdi_gxlibdc_h
#define __lib_gdi_gxlibdc_h

#include <lib/base/thread.h>
#include <lib/gdi/gmaindc.h>

#include <xine.h>
#include <xine/xineutils.h>
#include <xine/xine_internal.h>
#include <xine/osd.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/*#define INPUT_MOTION (ExposureMask | ButtonPressMask | KeyPressMask | \
                      ButtonMotionMask | StructureNotifyMask |        \
                      PropertyChangeMask | PointerMotionMask)
#define INPUT_MOTION (ExposureMask | ButtonPressMask | KeyPressMask | \
                      ButtonMotionMask | StructureNotifyMask |        \
                      PropertyChangeMask)*/
#define INPUT_MOTION KeyPressMask | KeyReleaseMask

enum
{
	_NET_WM_STATE_REMOVE =0,
	_NET_WM_STATE_ADD = 1,
	_NET_WM_STATE_TOGGLE =2
};

class gXlibDC: public gMainDC, public eThread, public Object
{
private:
	static gXlibDC       *instance;

	xine_t               *xine;
	char                 configfile[2048];
	int                  screen;
	char                 *vo_driver;
	char                 *ao_driver;
	osd_object_t         *osd;
	xine_video_port_t    *vo_port;
	xine_audio_port_t    *ao_port;
	x11_visual_t         vis;
	uint32_t             *surface;
	gSurface             m_surface;
	pthread_t            xine_start_thread;
	int                  fullscreen;
	int                  windowWidth, windowHeight;

	void exec(const gOpcode *opcode);

	eFixedMessagePump<XKeyEvent> m_pump;
	void keyEvent(const XKeyEvent &event);
	void pumpEvent(const XKeyEvent &event);
	virtual void thread();

	enum event {
		EV_SET_VIDEO_MODE,
		EV_FLIP,
		EV_QUIT,
	};

	//void pushEvent(enum event code, void *data1 = 0, void *data2 = 0);
	//void evSetVideoMode(unsigned long xres, unsigned long yres);
	void evFlip();
	void fullscreen_switch();
	void updateWindowState();

public:
	xine_stream_t        *stream;
	static int       width, height;
	static double    pixel_aspect;
        xine_streamtype_data_t videoData, audioData;
int a;

	gXlibDC();
	virtual ~gXlibDC();

	static gXlibDC *getInstance() { return instance; }
	void setResolution(int xres, int yres);
	int islocked() { return 0; }

	static void frame_output_cb(void *data, int video_width, int video_height, double video_pixel_aspect,
			int *dest_x, int *dest_y, int *dest_width, int *dest_height, double *dest_pixel_aspect,
			int *win_x, int *win_y);
	static void dest_size_cb(void *data, int video_width, int video_height, double video_pixel_aspect,
			 int *dest_width, int *dest_height, double *dest_pixel_aspect);

	void videoStart(int pid, int type);
	void videoStop(void);
	void setVideoType(int pid, int type);
	void setAudioType(int pid, int type);
	void setStreamType(int video);
};

#endif
