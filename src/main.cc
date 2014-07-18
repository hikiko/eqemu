/*
eqemu - electronic queue system emulator
Copyright (C) 2014  John Tsiombikas <nuclear@member.fsf.org>,
                    Eleni-Maria Stea <eleni@mutantstargoat.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <GL/glew.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "dev.h"
#include "scene.h"
#include "timer.h"
#include "fblur.h"


enum {
	REGULAR_PASS,
	GLOW_PASS
};

void post_redisplay();
static bool init();
static void cleanup();
static void display();
static void draw_scene(int pass = REGULAR_PASS);
static void post_glow(void);
static void keyb(int key, bool pressed);
static void mouse(int bn, bool pressed, int x, int y);
static void motion(int x, int y);
static Ray calc_pick_ray(int x, int y);
static int next_pow2(int x);

static Window create_window(const char *title, int xsz, int ysz);
static void process_events();
static int translate_keysym(KeySym sym);

static int proc_args(int argc, char **argv);

static Display *dpy;
static Window win;
static GLXContext ctx;
static Atom xa_wm_prot, xa_wm_del_win;

static int win_width, win_height;

static bool draw_pending;
static bool win_mapped;

static int fakefd = -1;
static char *fake_devpath;

static float cam_theta, cam_phi, cam_dist = 140;
static Scene *scn;

enum { BN_TICKET, BN_NEXT, NUM_BUTTONS };
static const char *button_names[] = { "button1", "button2" };
static Object *button_obj[NUM_BUTTONS];
static Object *disp_obj[2];
static Object *led_obj[2];
static Vector3 led_on_emissive;

static bool opt_use_glow = true;
#define GLOW_SZ_DIV		3
static unsigned int glow_tex;
static int glow_tex_xsz, glow_tex_ysz, glow_xsz, glow_ysz;
static int glow_iter = 1;
static int blur_size = 5;
unsigned char *glow_framebuf;


int main(int argc, char **argv)
{
	if(proc_args(argc, argv) == -1) {
		return 1;
	}
	if(!init()) {
		return 1;
	}
	atexit(cleanup);

	int xfd = ConnectionNumber(dpy);

	// run once through pending events before going into the select loop
	process_events();

	for(;;) {
		fd_set rd;
		FD_ZERO(&rd);

		FD_SET(xfd, &rd);
		FD_SET(fakefd, &rd);

		struct timeval noblock = {0, 0};
		int maxfd = xfd > fakefd ? xfd : fakefd;
		while(!XPending(dpy) && select(maxfd + 1, &rd, 0, 0, draw_pending ? &noblock : 0) == -1 && errno == EINTR);

		if(XPending(dpy) || FD_ISSET(xfd, &rd)) {
			process_events();
		}
		if(FD_ISSET(fakefd, &rd)) {
			proc_dev_input();
		}

		if(draw_pending) {
			draw_pending = false;
			display();
		}
	}
	return 0;
}

void post_redisplay()
{
	draw_pending = true;
}

static bool init()
{
	if(fake_devpath) {
		if((fakefd = start_dev(fake_devpath)) == -1) {
			return false;
		}
	}

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "failed to connect to the X server!\n");
		return false;
	}

	if(!(win = create_window("equeue device emulator", 512, 512))) {
		return false;
	}

	glewInit();

	scn = new Scene;
	if(!scn->load("data/device.obj")) {
		fprintf(stderr, "failed to load device 3D model\n");
		return false;
	}

	for(int i=0; i<NUM_BUTTONS; i++) {
		button_obj[i] = scn->get_object(button_names[i]);
		if(!button_obj[i]) {
			fprintf(stderr, "invalid 3D model\n");
			return false;
		}
		BSphere &bs = button_obj[i]->get_mesh()->get_bounds();
		bs.set_radius(bs.get_radius() * 1.5);
	}

	disp_obj[0] = scn->get_object("7seg0");
	disp_obj[1] = scn->get_object("7seg1");
	if(!disp_obj[0] || !disp_obj[1]) {
		fprintf(stderr, "invalid 3D model\n");
		return false;
	}
	scn->remove_object(disp_obj[0]);
	scn->remove_object(disp_obj[1]);

	led_obj[0] = scn->get_object("led1");
	led_obj[1] = scn->get_object("led2");
	if(!led_obj[0] || !led_obj[1]) {
		fprintf(stderr, "invalid 3D model\n");
		return false;
	}
	scn->remove_object(led_obj[0]);
	scn->remove_object(led_obj[1]);
	led_on_emissive = led_obj[0]->mtl.emissive;

	// create the glow texture
	glGenTextures(1, &glow_tex);
	glBindTexture(GL_TEXTURE_2D, glow_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glClearColor(0.1, 0.1, 0.1, 1);

	return true;
}

static void cleanup()
{
	delete scn;

	stop_dev();

	if(!dpy) return;

	if(win) {
		XDestroyWindow(dpy, win);
	}
	XCloseDisplay(dpy);
}

#define DIGIT_USZ	(1.0 / 11.0)
#define MIN_REDRAW_INTERVAL		(1000 / 40)		/* 40fps */

static void display()
{
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -cam_dist);
	glRotatef(cam_phi, 1, 0, 0);
	glRotatef(cam_theta, 0, 1, 0);

	float lpos[] = {-7, 5, 10, 0};
	glLightfv(GL_LIGHT0, GL_POSITION, lpos);

	if(opt_use_glow) {
		glViewport(0, 0, glow_xsz, glow_ysz);

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		draw_scene(GLOW_PASS);

		glReadPixels(0, 0, glow_xsz, glow_ysz, GL_RGBA, GL_UNSIGNED_BYTE, glow_framebuf);
		glViewport(0, 0, win_width, win_height);
	}

	glClearColor(0.05, 0.05, 0.05, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	draw_scene();

	if(opt_use_glow) {
		for(int i=0; i<glow_iter; i++) {
			fast_blur(BLUR_BOTH, blur_size, (uint32_t*)glow_framebuf, glow_xsz, glow_ysz);
			glBindTexture(GL_TEXTURE_2D, glow_tex);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glow_xsz, glow_ysz, GL_RGBA, GL_UNSIGNED_BYTE, glow_framebuf);

			post_glow();
		}
	}

	if(get_led_state(0)) {
		// continuously redraw until the left LED times out
		draw_pending = true;
	}

	glXSwapBuffers(dpy, win);
	assert(glGetError() == GL_NO_ERROR);

	static long prev_msec;
	long msec = get_msec();
	long dt = msec - prev_msec;

	if(dt < MIN_REDRAW_INTERVAL) {
		wait_for(MIN_REDRAW_INTERVAL - dt);
	}
	prev_msec = get_msec();
}

static void draw_scene(int pass)
{
	if(pass != GLOW_PASS) {
		scn->render();
	}

	// shift the textures and modify the materials to make the display match our state
	for(int i=0; i<2; i++) {
		// 7seg
		int digit = get_display_number();
		for(int j=0; j<i; j++) {
			digit /= 10;
		}
		digit %= 10;

		float uoffs = DIGIT_USZ + DIGIT_USZ * digit;

		disp_obj[i]->mtl.tex_offset[TEX_DIFFUSE] = Vector2(uoffs, 0);
		disp_obj[i]->render();

		// LEDs
		if(get_led_state(i)) {
			led_obj[i]->mtl.emissive = led_on_emissive;
		} else {
			led_obj[i]->mtl.emissive = Vector3(0, 0, 0);
		}
		led_obj[i]->render();
	}
}

static void post_glow(void)
{
	float max_s = (float)glow_xsz / (float)glow_tex_xsz;
	float max_t = (float)glow_ysz / (float)glow_tex_ysz;

	glPushAttrib(GL_ENABLE_BIT);

	glBlendFunc(GL_ONE, GL_ONE);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, glow_tex);

	glBegin(GL_QUADS);
	glColor4f(1, 1, 1, 1);
	glTexCoord2f(0, 0);
	glVertex2f(-1, -1);
	glTexCoord2f(max_s, 0);
	glVertex2f(1, -1);
	glTexCoord2f(max_s, max_t);
	glVertex2f(1, 1);
	glTexCoord2f(0, max_t);
	glVertex2f(-1, 1);
	glEnd();

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glPopAttrib();
}


static void reshape(int x, int y)
{
	glViewport(0, 0, x, y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50.0, (float)x / (float)y, 1.0, 1000.0);

	win_width = x;
	win_height = y;

	if(opt_use_glow) {
		glow_xsz = x / GLOW_SZ_DIV;
		glow_ysz = y / GLOW_SZ_DIV;
		printf("glow image size: %dx%d\n", glow_xsz, glow_ysz);

		delete [] glow_framebuf;
		glow_framebuf = new unsigned char[glow_xsz * glow_ysz * 4];

		glow_tex_xsz = next_pow2(glow_xsz);
		glow_tex_ysz = next_pow2(glow_ysz);
		glBindTexture(GL_TEXTURE_2D, glow_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glow_tex_xsz, glow_tex_ysz, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	}
}

static void keyb(int key, bool pressed)
{
	if(pressed) {
		switch(key) {
		case 27:
			exit(0);
		}
	}
}

static bool bnstate[32];
static int prev_x, prev_y;

static void mouse(int bn, bool pressed, int x, int y)
{
	bnstate[bn] = pressed;
	prev_x = x;
	prev_y = y;

	if(bn == 0 && pressed) {
		// do picking
		Ray ray = calc_pick_ray(x, win_height - y);

		HitPoint minhit;
		minhit.t = FLT_MAX;
		int hit_found = -1;

		for(int i=0; i<NUM_BUTTONS; i++) {
			HitPoint hit;
			if(button_obj[i]->get_mesh()->get_bounds().intersect(ray, &hit) && hit.t < minhit.t) {
				minhit = hit;
				hit_found = i;
			}
		}

		if(hit_found != -1) {
			switch(hit_found) {
			case BN_TICKET:
				issue_ticket();
				break;

			case BN_NEXT:
				next_customer();
				break;
			}
			draw_pending = true;
		}
	}
}

static void motion(int x, int y)
{
	int dx = x - prev_x;
	int dy = y - prev_y;
	prev_x = x;
	prev_y = y;

	if(bnstate[0]) {
		cam_theta += dx * 0.5;
		cam_phi += dy * 0.5;
		if(cam_phi < -90) cam_phi = -90;
		if(cam_phi > 90) cam_phi = 90;

	} else if(bnstate[2]) {
		cam_dist += dy * 0.5;
		if(cam_dist < 0.0) cam_dist = 0.0;

	} else {
		float xoffs = 2.0 * x / win_width - 1.0;
		float yoffs = 2.0 * y / win_height - 1.0;
		cam_theta = -xoffs * 15.0 * (win_width / win_height);
		cam_phi = -yoffs * 15.0;
	}
	draw_pending = true;
}

static Ray calc_pick_ray(int x, int y)
{
	double mv[16], proj[16];
	int vp[4];
	double resx, resy, resz;
	Ray ray;

	glGetDoublev(GL_MODELVIEW_MATRIX, mv);
	glGetDoublev(GL_PROJECTION_MATRIX, proj);
	glGetIntegerv(GL_VIEWPORT, vp);

	gluUnProject(x, y, 0, mv, proj, vp, &resx, &resy, &resz);
	ray.origin = Vector3(resx, resy, resz);

	gluUnProject(x, y, 1, mv, proj, vp, &resx, &resy, &resz);
	ray.dir = normalize(Vector3(resx, resy, resz) - ray.origin);

	return ray;
}

static int next_pow2(int x)
{
	x--;
	x = (x >> 1) | x;
	x = (x >> 2) | x;
	x = (x >> 4) | x;
	x = (x >> 8) | x;
	x = (x >> 16) | x;
	return x + 1;
}

static Window create_window(const char *title, int xsz, int ysz)
{
	int scr = DefaultScreen(dpy);
	Window root = RootWindow(dpy, scr);

	int glxattr[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_DOUBLEBUFFER,
#if defined(GLX_VERSION_1_4) || defined(GLX_ARB_multisample)
		GLX_SAMPLE_BUFFERS_ARB, 1,
		GLX_SAMPLES_ARB, 1,
#endif
		None
	};

	XVisualInfo *vis = glXChooseVisual(dpy, scr, glxattr);
	if(!vis) {
		fprintf(stderr, "failed to find a suitable visual\n");
		return 0;
	}

	if(!(ctx = glXCreateContext(dpy, vis, 0, True))) {
		fprintf(stderr, "failed to create OpenGL context\n");
		XFree(vis);
		return -1;
	}

	XSetWindowAttributes xattr;
	xattr.background_pixel = xattr.border_pixel = BlackPixel(dpy, scr);
	xattr.colormap = XCreateColormap(dpy, root, vis->visual, AllocNone);
	unsigned int xattr_mask = CWColormap | CWBackPixel | CWBorderPixel;

	Window win = XCreateWindow(dpy, root, 0, 0, xsz, ysz, 0, vis->depth, InputOutput,
			vis->visual, xattr_mask, &xattr);
	if(!win) {
		fprintf(stderr, "failed to create window\n");
		glXDestroyContext(dpy, ctx);
		XFree(vis);
		return -1;
	}
	XFree(vis);

	unsigned int evmask = StructureNotifyMask | VisibilityChangeMask | ExposureMask |
		KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
		PointerMotionMask | LeaveWindowMask;
	XSelectInput(dpy, win, evmask);

	xa_wm_prot = XInternAtom(dpy, "WM_PROTOCOLS", False);
	xa_wm_del_win = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &xa_wm_del_win, 1);

	XClassHint hint;
	hint.res_name = hint.res_class = (char*)"equeue_win";
	XSetClassHint(dpy, win, &hint);

	XTextProperty wm_name;
	XStringListToTextProperty((char**)&title, 1, &wm_name);
	XSetWMName(dpy, win, &wm_name);
	XSetWMIconName(dpy, win, &wm_name);
	XFree(wm_name.value);

	XMapWindow(dpy, win);
	glXMakeCurrent(dpy, win, ctx);

	return win;
}

static void process_events()
{
	XEvent ev;

	while(XPending(dpy)) {
		XNextEvent(dpy, &ev);
		switch(ev.type) {
		case MapNotify:
			win_mapped = true;
			break;

		case UnmapNotify:
			win_mapped = false;
			break;

		case Expose:
			if(win_mapped && ev.xexpose.count == 0) {
				draw_pending = true;
			}
			break;

		case MotionNotify:
			motion(ev.xmotion.x, ev.xmotion.y);
			break;

		case ButtonPress:
			mouse(ev.xbutton.button - 1, true, ev.xbutton.x, ev.xbutton.y);
			break;

		case ButtonRelease:
			mouse(ev.xbutton.button - 1, false, ev.xbutton.x, ev.xbutton.y);
			break;

		case KeyPress:
			{
				KeySym sym = XLookupKeysym(&ev.xkey, 0);
				keyb(translate_keysym(sym), true);
			}
			break;

		case KeyRelease:
			{
				KeySym sym = XLookupKeysym(&ev.xkey, 0);
				keyb(translate_keysym(sym), false);
			}
			break;

		case ConfigureNotify:
			{
				int xsz = ev.xconfigure.width;
				int ysz = ev.xconfigure.height;

				if(xsz != win_width || ysz != win_height) {
					win_width = xsz;
					win_height = ysz;
					reshape(xsz, ysz);
				}
			}
			break;

		case ClientMessage:
			if(ev.xclient.message_type == xa_wm_prot) {
				if((Atom)ev.xclient.data.l[0] == xa_wm_del_win) {
					exit(0);
				}
			}
			break;

		case LeaveNotify:
			if(ev.xcrossing.mode == NotifyNormal) {
				cam_theta = cam_phi = 0;
				draw_pending = true;
			}
			break;

		default:
			break;
		}

	}
}

static int translate_keysym(KeySym sym)
{
	switch(sym) {
	case XK_BackSpace:
		return '\b';
	case XK_Tab:
		return '\t';
	case XK_Linefeed:
		return '\r';
	case XK_Return:
		return '\n';
	case XK_Escape:
		return 27;
	default:
		break;
	}
	return (int)sym;
}

static int proc_args(int argc, char **argv)
{
	for(int i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			fprintf(stderr, "unexpected option: %s\n", argv[i]);
			return -1;

		} else {
			if(fake_devpath) {
				fprintf(stderr, "unexpected argument: %s\n", argv[i]);
				return -1;
			}
			fake_devpath = argv[i];
		}
	}
	if(!fake_devpath) {
		fprintf(stderr, "no device path specified, running standalone\n");
	}
	return 0;
}
