/*
 * Copyright © 2014 Renesas Electronics Corp.
 *
 * Based on pixman-renderer by:
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Vasily Khoruzhick <anarsoul@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *	Takanari Hayama <taki@igel.co.jp>
 */

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
//#include "v4l2-renderer.h"
//#include "v4l2-renderer-device.h"

#include "media-ctl/mediactl.h"
#include "media-ctl/mediactl-priv.h"
#include "media-ctl/v4l2subdev.h"
#include "media-ctl/tools.h"

#include <linux/input.h>

/* Required for a short term workaround */
#include "v4l2-compat.h"

#include "vsp-renderer.h"


#include <direct/debug.h>

#include <directfb_util.h>


D_DEBUG_DOMAIN( VSP1_Renderer, "VSP1/Renderer", "Renesas VSP1 Renderer" );


const char *vsp_input_links[] = {
	"'%s rpf.0':1 -> '%s bru':0",
	"'%s rpf.1':1 -> '%s bru':1",
	"'%s rpf.2':1 -> '%s bru':2",
	"'%s rpf.3':1 -> '%s bru':3"
};

const char *vsp_output_links[] = {
	"'%s bru':4 -> '%s wpf.0':0",
	"'%s wpf.0':1 -> '%s wpf.0 output':0"
};

const char *vsp_inputs[] = {
	"%s rpf.0 input",
	"%s rpf.1 input",
	"%s rpf.2 input",
	"%s rpf.3 input"
};

const char *vsp_output = {
	"%s wpf.0 output"
};

const char *vsp_input_infmt[] = {
	"'%s rpf.0':0",
	"'%s rpf.1':0",
	"'%s rpf.2':0",
	"'%s rpf.3':0"
};

const char *vsp_input_outfmt[] = {
	"'%s rpf.0':1",
	"'%s rpf.1':1",
	"'%s rpf.2':1",
	"'%s rpf.3':1"
};

const char *vsp_input_composer[] = {
	"'%s bru':0",
	"'%s bru':1",
	"'%s bru':2",
	"'%s bru':3"
};

const char *vsp_input_subdev[] = {
	"%s rpf.0",
	"%s rpf.1",
	"%s rpf.2",
	"%s rpf.3"
};

const char *vsp_scaler_links[] = {
	"'%s rpf.%d':1 -> '%s uds.%d':0",
	"'%s uds.%d':1 -> '%s bru':%d"
};

const char *vsp_scaler_infmt = "'%s uds.%d':0";
const char *vsp_scaler_outfmt = "'%s uds.%d':1";


static void
video_debug_mediactl(void)
{
	FILE *p = popen("media-ctl -d /dev/media0 -p", "r");
	char buf[BUFSIZ * 16];

	weston_log("====== output of media-ctl ======\n");
	while(!feof(p)) {
		fread(buf, sizeof(buf), 1, p);
		weston_log_continue(buf);
	}
	weston_log_continue("\n================================\n");

	pclose(p);
}

static int
video_is_capture(__u32 cap)
{
	return ((cap & V4L2_CAP_VIDEO_CAPTURE) || (cap & V4L2_CAP_VIDEO_CAPTURE_MPLANE));
}

static int
video_is_mplane(__u32 cap)
{
	return ((cap & V4L2_CAP_VIDEO_CAPTURE_MPLANE) || (cap & V4L2_CAP_VIDEO_OUTPUT_MPLANE));
}

static int
video_is_streaming(__u32 cap)
{
	return (cap & V4L2_CAP_STREAMING);
}

static void
vsp_check_capabiility(int fd, const char *devname)
{
	struct v4l2_capability cap;
	int ret;

	memset(&cap, 0, sizeof cap);
	ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		weston_log("VIDIOC_QUERY_CAP on %s failed.\n", devname);
		return;
	}

	weston_log("Device `%s'(%d) is a video %s (%s mplane and %s streaming support)\n",
		   devname, fd,
		   (video_is_capture(cap.device_caps) ? "capture" : "output"),
		   (video_is_mplane(cap.device_caps) ? "w/" : "w/o"),
		   (video_is_streaming(cap.device_caps) ? "w/" : "w/o"));
}

static int
vsp_init_pipeline(struct vsp_device *vsp);

static struct v4l2_renderer_device*
vsp_init(struct media_device *media)
{
	struct vsp_device *vsp = NULL;
	struct media_entity *entity;
	const struct media_device_info *info;
	char buf[64], *p;
	const char *device_name, *devname;
	
	/* Get device name */
	info = media_get_info(media);
	if ((p = strchr(info->bus_info, ':')))
		device_name = p + 1;
	else
		device_name = info->bus_info;

	if (strncmp(device_name, "vsp1.", 5)) {
		weston_log("The device is not VSP1.");
		goto error;
	}

	weston_log("Using the device %s\n", device_name);

	vsp = calloc(1, sizeof(struct vsp_device));
	if (!vsp)
		goto error;
	vsp->base.media = media;
	vsp->base.device_name = device_name;
	vsp->state = VSP_STATE_IDLE;
	vsp->input_max = VSP_INPUT_MAX;
	vsp->scaler_max = VSP_SCALER_MAX;
	if (!vsp->input_pads)
		goto error;

	vsp_init_pipeline( vsp );

	/* get a file descriptor for the output */
	snprintf(buf, sizeof(buf), vsp_output, device_name);
	entity = media_get_entity_by_name(media, buf, strlen(buf));
	if (!entity) {
		weston_log("error... '%s' not found.\n", buf);
		goto error;
	}

	devname = media_entity_get_devname(entity);
	weston_log("output '%s' is associated with '%s'\n", buf, devname);
	vsp->output_pad.fd = open(devname, O_RDWR);
	if (vsp->output_pad.fd < 0) {
		weston_log("error... can't open '%s'.\n", devname);
		goto error;
	}
	vsp_check_capabiility(vsp->output_pad.fd, devname);

	return (struct v4l2_renderer_device*)vsp;

error:
	if (vsp) {
		if (vsp->input_pads)
			free(vsp->input_pads);
		free(vsp);
	}
	weston_log("VSP device init failed...\n");

	return NULL;
}

static int
vsp_init_pipeline(struct vsp_device *vsp)
{
	struct media_device *media = vsp->base.media;
	struct media_link *link;
	struct media_entity *entity;
	char buf[64], *endp;
	const char *device_name = vsp->base.device_name;
	int i, j;
	
	/* Reset links */
	if (media_reset_links(media)) {
		weston_log("Reset media controller links failed.\n");
		goto error;
	}
	
	/* Initialize inputs */
	weston_log("Setting up inputs.\n");
	for (i = 0; i < vsp->input_max; i++) {
		/* setup a link - do not enable yet */
		snprintf(buf, sizeof(buf), vsp_input_links[i], device_name, device_name);
		weston_log("setting up link: '%s'\n", buf);
		link = media_parse_link(media, buf, &endp);
		if (media_setup_link(media, link->source, link->sink, 0)) {
			weston_log("link set up failed.\n");
			goto error;
		}
		vsp->input_pads[i].link = link;

		/* get a pad to configure the compositor */
		snprintf(buf, sizeof(buf), vsp_input_infmt[i], device_name);
		weston_log("get an input pad: '%s'\n", buf);
		if (!(vsp->input_pads[i].infmt_pad = media_parse_pad(media, buf, NULL))) {
			weston_log("parse pad failed.\n");
			goto error;
		}

		snprintf(buf, sizeof(buf), vsp_input_outfmt[i], device_name);
		weston_log("get an input sink: '%s'\n", buf);
		if (!(vsp->input_pads[i].outfmt_pad = media_parse_pad(media, buf, NULL))) {
			weston_log("parse pad failed.\n");
			goto error;
		}

		snprintf(buf, sizeof(buf), vsp_input_composer[i], device_name);
		weston_log("get a composer pad: '%s'\n", buf);
		if (!(vsp->input_pads[i].compose_pad = media_parse_pad(media, buf, NULL))) {
			weston_log("parse pad failed.\n");
			goto error;
		}

		snprintf(buf, sizeof(buf), vsp_input_subdev[i], device_name);
		weston_log("get a input subdev pad: '%s'\n", buf);
		if (!(vsp->input_pads[i].input_entity = media_get_entity_by_name(media, buf, strlen(buf)))) {
			weston_log("parse entity failed.\n");
			goto error;
		}

		/* get a file descriptor for the input */
		snprintf(buf, sizeof(buf), vsp_inputs[i], device_name);
		entity = media_get_entity_by_name(media, buf, strlen(buf));
		if (!entity) {
			weston_log("error... '%s' not found.\n", buf);
			goto error;
		}

		if (v4l2_subdev_open(entity)) {
			weston_log("subdev '%s' open failed\n.", buf);
			goto error;
		}

		vsp->input_pads[i].fd = entity->fd;
		vsp_check_capabiility(vsp->input_pads[i].fd, media_entity_get_devname(entity));
	}

	/* Initialize scaler */
	weston_log("Setting up scaler(s).\n");
	for (i = 0; i < vsp->scaler_max; i++) {
		/* create link templates */
		for (j = 0; j < vsp->input_max; j++) {
			snprintf(buf, sizeof(buf), vsp_scaler_links[0], device_name, j, device_name, i);
			weston_log("parsing link: '%s'\n", buf);
			vsp->scalers[i].templates[j].link0 = media_parse_link(media, buf, &endp);

			snprintf(buf, sizeof(buf), vsp_scaler_links[1], device_name, i, device_name, j);
			weston_log("parsing link: '%s'\n", buf);
			vsp->scalers[i].templates[j].link1 = media_parse_link(media, buf, &endp);
		}

		/* get pads to setup UDS */
		snprintf(buf, sizeof(buf), vsp_scaler_infmt, device_name, i);
		weston_log("get a scaler input pad: '%s'\n", buf);
		if (!(vsp->scalers[i].infmt_pad = media_parse_pad(media, buf, NULL))) {
			weston_log("parse pad failed.\n");
			goto error;
		}

		snprintf(buf, sizeof(buf), vsp_scaler_outfmt, device_name, i);
		weston_log("get a scaler output pad: '%s'\n", buf);
		if (!(vsp->scalers[i].outfmt_pad = media_parse_pad(media, buf, NULL))) {
			weston_log("parse pad failed.\n");
			goto error;
		}

		/* initialize input */
		vsp->scalers[i].input = -1;
	}

	/* Initialize output */
	weston_log("Setting up an output.\n");

	/* setup links for output - always on */
	for (i = 0; i < (int)ARRAY_SIZE(vsp_output_links); i++) {
		snprintf(buf, sizeof(buf), vsp_output_links[i], device_name, device_name);
		weston_log("setting up link: '%s'\n", buf);
		link = media_parse_link(media, buf, &endp);
		if (media_setup_link(media, link->source, link->sink, 1)) {
			weston_log("link set up failed.\n");
			goto error;
		}
	}

	return 0;

error:
	if (vsp) {
//		if (vsp->input_pads)
//			free(vsp->input_pads);
	}
	weston_log("VSP pipeline init failed...\n");

	return -1;
}

static struct v4l2_surface_state*
vsp_create_surface(struct v4l2_renderer_device *dev)
{
	return (struct v4l2_surface_state*)calloc(1, sizeof(struct vsp_surface_state));
}

static int
vsp_attach_buffer(struct v4l2_surface_state *surface_state)
{
	struct vsp_surface_state *vs = (struct vsp_surface_state*)surface_state;
	enum v4l2_mbus_pixelcode code;
	int i;

	if (vs->base.width > 8190 || vs->base.height > 8190)
		return -1;

	switch(vs->base.pixel_format) {
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_RGB565:
		code = V4L2_MBUS_FMT_ARGB8888_1X32;
		break;

	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		code = V4L2_MBUS_FMT_AYUV8_1X32;
		break;

	default:
		return -1;
	}

	// create v4l2_fmt to use later
	vs->mbus_code = code;
	vs->fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	vs->fmt.fmt.pix_mp.width = vs->base.width;
	vs->fmt.fmt.pix_mp.height = vs->base.height;
	vs->fmt.fmt.pix_mp.pixelformat = vs->base.pixel_format;
	vs->fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	vs->fmt.fmt.pix_mp.num_planes = vs->base.num_planes;

	for (i = 0; i < vs->base.num_planes; i++)
		vs->fmt.fmt.pix_mp.plane_fmt[i].bytesperline = vs->base.planes[i].stride;

	return 0;
}

static int
vsp_set_format(int fd, struct v4l2_format *fmt)
{
	struct v4l2_format current_fmt;
	int ret;

	D_DEBUG_AT( VSP1_Renderer, "%s( fd %d )\n", __FUNCTION__, fd );

	memset(&current_fmt, 0, sizeof(struct v4l2_format));
	current_fmt.type = fmt->type;


	if (ioctl(fd, VIDIOC_G_FMT, &current_fmt) == -1) {
		weston_log("VIDIOC_G_FMT failed to %d (%s).\n", fd, strerror(errno));
	}

	DBG("Current video format: %d, %08x(%c%c%c%c) %ux%u (stride %u) field %08u buffer size %u\n",
	    current_fmt.type,
	    current_fmt.fmt.pix_mp.pixelformat,
	    (current_fmt.fmt.pix_mp.pixelformat >> 24) & 0xff,
	    (current_fmt.fmt.pix_mp.pixelformat >> 16) & 0xff,
	    (current_fmt.fmt.pix_mp.pixelformat >>  8) & 0xff,
	    current_fmt.fmt.pix_mp.pixelformat & 0xff,
	    current_fmt.fmt.pix_mp.width, current_fmt.fmt.pix_mp.height, current_fmt.fmt.pix_mp.plane_fmt[0].bytesperline,
	    current_fmt.fmt.pix_mp.field,
	    current_fmt.fmt.pix_mp.plane_fmt[0].sizeimage);

	ret = ioctl(fd, VIDIOC_S_FMT, fmt);

	DBG("New video format: %d, %08x(%c%c%c%c) %ux%u (stride %u) field %08u buffer size %u\n",
	    fmt->type,
	    fmt->fmt.pix_mp.pixelformat,
	    (fmt->fmt.pix_mp.pixelformat >> 24) & 0xff,
	    (fmt->fmt.pix_mp.pixelformat >> 16) & 0xff,
	    (fmt->fmt.pix_mp.pixelformat >>  8) & 0xff,
	    fmt->fmt.pix_mp.pixelformat & 0xff,
	    fmt->fmt.pix_mp.width, fmt->fmt.pix_mp.height, fmt->fmt.pix_mp.plane_fmt[0].bytesperline,
	    fmt->fmt.pix_mp.field,
	    fmt->fmt.pix_mp.plane_fmt[0].sizeimage);

	if (ret == -1) {
		weston_log("VIDIOC_S_FMT failed to %d (%s).\n", fd, strerror(errno));
		return -1;
	}

	return 0;
}

static int
vsp_set_output(struct vsp_device *vsp, struct vsp_renderer_output *out)
{
	char buf[64];
	int i;
	struct media_pad *pad;
	struct v4l2_mbus_framefmt format, format2;
	static char *pads[] = {
		"'%s bru':4",
		"'%s wpf.0':0",
		"'%s wpf.0':1",
	};

	DBG("Setting output size to %dx%d\n", out->base.width, out->base.height);

	/* set WPF output size  */
	format.width  = out->base.width;
	format.height = out->base.height;
	format.code   = V4L2_MBUS_FMT_ARGB8888_1X32;	// TODO: does this have to be flexible?

	for (i = 0; i < (int)ARRAY_SIZE(pads); i++) {
		snprintf(buf, sizeof(buf), pads[i], vsp->base.device_name);
		DBG("Setting output format: '%s'\n", buf);
		pad = media_parse_pad(vsp->base.media, buf, NULL);
		if (v4l2_subdev_set_format(pad->entity, &format, pad->index, V4L2_SUBDEV_FORMAT_ACTIVE)) {
			weston_log("set sbudev format for %s failed.\n", buf);
			return -1;
		}

		if (v4l2_subdev_get_format(pad->entity, &format2, pad->index, V4L2_SUBDEV_FORMAT_ACTIVE)) {
			weston_log("get sbudev format for %s failed.\n", buf);
			return -1;
		}

		if ((format.width != format2.width) || (format.height != format2.height) || (format.code != format2.code)) {
			weston_log("set format doesn't match: '%s'!", buf);
		}
	}

	return 0;
}

static struct v4l2_renderer_output*
vsp_create_output(struct v4l2_renderer_device *dev, int width, int height)
{
	//struct vsp_device *vsp = (struct vsp_device*)dev;
	struct vsp_renderer_output *outdev;
	struct v4l2_format *fmt;

	outdev = calloc(1, sizeof(struct vsp_renderer_output));
	if (!outdev)
		return NULL;

	/* set output surface state */
	outdev->base.width = width;
	outdev->base.height = height;
	outdev->surface_state.mbus_code = V4L2_MBUS_FMT_ARGB8888_1X32;
	outdev->surface_state.base.width = width;
	outdev->surface_state.base.height = height;
	outdev->surface_state.base.num_planes = 1;
	outdev->surface_state.base.src_rect.width = width;
	outdev->surface_state.base.src_rect.height = height;
	outdev->surface_state.base.dst_rect.width = width;
	outdev->surface_state.base.dst_rect.height = height;

	/* we use this later to let output to be input for composition */
	fmt = &outdev->surface_state.fmt;
	fmt->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt->fmt.pix_mp.width = width;
	fmt->fmt.pix_mp.height = height;
	fmt->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_ABGR32;
	fmt->fmt.pix_mp.num_planes = 1;

	return (struct v4l2_renderer_output*)outdev;
}

static int
vsp_dequeue_buffer(int fd, int capture)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];

	D_DEBUG_AT( VSP1_Renderer, "%s( fd %d, capture %d )...\n", __FUNCTION__, fd, capture );

	memset(&buf, 0, sizeof buf);
	buf.type = (capture) ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.memory = V4L2_MEMORY_DMABUF;
	buf.index = 0;
	buf.m.planes = planes;
	buf.length = 1;
	memset(planes, 0, sizeof(planes));

	if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
		weston_log("VIDIOC_DQBUF failed on %d (%s).\n", fd, strerror(errno));
		return -1;
	}

	D_DEBUG_AT( VSP1_Renderer, "  -> done\n" );

	return 0;
}

static int
vsp_queue_buffer(int fd, int capture, struct vsp_surface_state *vs)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	int i;

	D_DEBUG_AT( VSP1_Renderer, "%s( fd %d, capture %d, state %p )\n", __FUNCTION__, fd, capture, vs );

	memset(&buf, 0, sizeof buf);
	buf.type = (capture) ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.memory = V4L2_MEMORY_DMABUF;
	buf.index = 0;
	buf.m.planes = planes;
	buf.length = vs->base.num_planes;
	memset(planes, 0, sizeof(planes));
	for (i = 0; i < vs->base.num_planes; i++)
		buf.m.planes[i].m.fd = vs->base.planes[i].dmafd;

	if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
		weston_log("VIDIOC_QBUF failed for dmafd=%d(%d planes) on %d (%s).\n",
			   vs->base.planes[i].dmafd, vs->base.num_planes, fd, strerror(errno));
		return -1;
	}

	return 0;
}

static int
vsp_request_buffer(int fd, int capture, int count)
{
	struct v4l2_requestbuffers reqbuf;

	D_DEBUG_AT( VSP1_Renderer, "%s( fd %d, capture %d, count %d )\n", __FUNCTION__, fd, capture, count );

	memset(&reqbuf, 0, sizeof(reqbuf));
	reqbuf.type = (capture) ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	reqbuf.memory = V4L2_MEMORY_DMABUF;
	reqbuf.count = count;
	if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
		weston_log("clearing VIDIOC_REQBUFS failed (%s).\n", strerror(errno));
		return -1;
	}

	return 0;
}

static void
vsp_comp_begin(struct v4l2_renderer_device *dev, struct v4l2_renderer_output *out)
{
	struct vsp_device *vsp = (struct vsp_device*)dev;
	struct vsp_renderer_output *output = (struct vsp_renderer_output*)out;
	struct v4l2_format *fmt = &output->surface_state.fmt;

	DBG("start vsp composition.\n");

	vsp->state = VSP_STATE_START;

	vsp_set_output(vsp, output);

	// just in case
	vsp_request_buffer(vsp->output_pad.fd, 1, 0);

	fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	vsp_set_format(vsp->output_pad.fd, fmt);
	fmt->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	vsp->output_surface_state = &output->surface_state;

	vsp_request_buffer(vsp->output_pad.fd, 1, 1);

	DBG("output set to dmabuf=%d\n", vsp->output_surface_state->base.planes[0].dmafd);
}

static int
vsp_set_alpha(struct media_entity *entity, float alpha)
{
	struct v4l2_control ctrl;

	ctrl.id = V4L2_CID_ALPHA_COMPONENT;
	ctrl.value = (__s32)(alpha * 0xff);

	if (ioctl(entity->fd, VIDIOC_S_CTRL, &ctrl) == -1) {
		weston_log("failed to set alpha value (%d)\n", ctrl.value);
		return -1;
	}

	return 0;
}

static int
vsp_comp_setup_inputs(struct vsp_device *vsp, struct vsp_media_pad *mpad, struct vsp_scaler *scaler,
		      struct vsp_surface_state *vs, int enable)
{
	struct v4l2_mbus_framefmt format;

	D_DEBUG_AT( VSP1_Renderer, "%s( state %p, enable %d )\n", __FUNCTION__, vs, enable );

	// enable link associated with this pad
	if (!scaler) {
		if (media_setup_link(vsp->base.media, mpad->link->source, mpad->link->sink, enable)) {
			weston_log("enabling media link setup failed.\n");
			return -1;
		}
	} else {
		struct vsp_scaler_template *temp = &scaler->templates[scaler->input];

		if (enable)
			media_setup_link(vsp->base.media, mpad->link->source, mpad->link->sink, 0);

		if (media_setup_link(vsp->base.media, temp->link0->source, temp->link0->sink, enable)) {
			weston_log("enabling scaler link0 setup failed.\n");
			return -1;
		}

		if (media_setup_link(vsp->base.media, temp->link1->source, temp->link1->sink, enable)) {
			weston_log("enabling scaler link1 setup failed.\n");
			return -1;
		}
	}

	if (!enable)
		return 0;

	// set pixel format and size
	format.width = vs->base.width;
	format.height = vs->base.height;
	format.code = vs->mbus_code;	// this is input format
	if (v4l2_subdev_set_format(mpad->infmt_pad->entity, &format, mpad->infmt_pad->index,
				   V4L2_SUBDEV_FORMAT_ACTIVE)) {
		weston_log("set input format via subdev failed.\n");
		return -1;
	}

	// set an alpha
	if (vsp_set_alpha(mpad->input_entity, vs->base.alpha)) {
		weston_log("setting alpha (=%f) failed.", vs->base.alpha);
		return -1;
	}
#if 1
	// set a crop paramters
	if (v4l2_subdev_set_selection(mpad->infmt_pad->entity, &vs->base.src_rect, mpad->infmt_pad->index,
				      V4L2_SEL_TGT_CROP, V4L2_SUBDEV_FORMAT_ACTIVE)) {
		weston_log("set crop parameter failed: %dx%d@(%d,%d).\n",
			   vs->base.src_rect.width, vs->base.src_rect.height,
			   vs->base.src_rect.left, vs->base.src_rect.top);
		return -1;
	}
#endif
	format.width = vs->base.src_rect.width;
	format.height = vs->base.src_rect.height;

	// this is an output towards BRU. this shall be consistent among all inputs.
	format.code = V4L2_MBUS_FMT_ARGB8888_1X32;
	if (v4l2_subdev_set_format(mpad->outfmt_pad->entity, &format, mpad->outfmt_pad->index,
				   V4L2_SUBDEV_FORMAT_ACTIVE)) {
		weston_log("set output format via subdev failed.\n");
		return -1;
	}

	// if we enabled the scaler, we should set resize parameters.
	if (scaler) {
		// a sink of UDS should be the same as a source of RPF.
		if (v4l2_subdev_set_format(scaler->infmt_pad->entity, &format, scaler->infmt_pad->index,
					   V4L2_SUBDEV_FORMAT_ACTIVE)) {
			weston_log("set input format of UDS via subdev failed.\n");
			return -1;
		}

		// a source of UDS should be the same as a sink of BRU.
		format.width  = vs->base.dst_rect.width;
		format.height = vs->base.dst_rect.height;
		if (v4l2_subdev_set_format(scaler->outfmt_pad->entity, &format, scaler->outfmt_pad->index,
					   V4L2_SUBDEV_FORMAT_ACTIVE)) {
			weston_log("set output format of UDS via subdev failed.\n");
			return -1;
		}
	}

	// so does the BRU input
	if (v4l2_subdev_set_format(mpad->compose_pad->entity, &format, mpad->compose_pad->index,
				   V4L2_SUBDEV_FORMAT_ACTIVE)) {
		weston_log("set composition format via subdev failed.\n");
		return -1;
	}

	// set a composition paramters
	struct v4l2_rect x = { 0, 0, vs->base.dst_rect.width, vs->base.dst_rect.height };
	if (v4l2_subdev_set_selection(mpad->compose_pad->entity, &x, mpad->compose_pad->index,
				      V4L2_SEL_TGT_COMPOSE, V4L2_SUBDEV_FORMAT_ACTIVE)) {
		weston_log("set compose parameter failed: %dx%d@(%d,%d).\n",
			   x.width, x.height,
			   x.left, x.top);
		return -1;
	}

	// just in case
	if (vsp_request_buffer(mpad->fd, 0, 0) < 0)
		return -1;

	// set input format
	if (vsp_set_format(mpad->fd, &vs->fmt))
		return -1;

	DBG("requesting a buffer...\n");
	// request a buffer
	if (vsp_request_buffer(mpad->fd, 0, 1) < 0)
		return -1;

	DBG("queueing an input buffer...\n");
	// queue buffer
	if (vsp_queue_buffer(mpad->fd, 0, vs) < 0)
		return -1;

	DBG("queued buffer\n");
	return 0;
}

static int
vsp_comp_flush_views(struct vsp_device *vsp, const struct v4l2_rect *clip)
{
	int i, fd;
	int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	D_DEBUG_AT( VSP1_Renderer, "%s()\n", __FUNCTION__ );

	DBG("flush vsp composition.\n");

	D_ASSERT( vsp->flushed == 0 );

	// enable links and queue buffer
	for (i = 0; i < vsp->input_count; i++)
		vsp_comp_setup_inputs(vsp, &vsp->input_pads[i], vsp->use_scaler[i],
				      vsp->input_surface_states[i], 1);

	// disable unused inputs
	for (i = vsp->input_count; i < vsp->input_max; i++)
		vsp_comp_setup_inputs(vsp, &vsp->input_pads[i], NULL, NULL, 0);

	// get an output pad
	fd = vsp->output_pad.fd;

	// queue buffer
	if (vsp_queue_buffer(fd, 1, vsp->output_surface_state) < 0)
		goto error;

//	video_debug_mediactl();

	// stream on
	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	for (i = 0; i < vsp->input_count; i++) {
		if (ioctl(vsp->input_pads[i].fd, VIDIOC_STREAMON, &type) == -1) {
			weston_log("VIDIOC_STREAMON failed for input %d. (%s)\n", i, strerror(errno));
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		weston_log("VIDIOC_STREAMON failed for output (%s).\n", strerror(errno));
		goto error;
	}

	vsp->flushed = 1;
	return 0;

error:
	video_debug_mediactl();
	vsp->input_count = 0;
	return -1;
}

static void
vsp_comp_flush(struct v4l2_renderer_device *dev, const struct v4l2_rect *clip)
{
	char buf[64];
	struct vsp_device *vsp = (struct vsp_device*)dev;
	struct media_pad *pad;
	struct v4l2_mbus_framefmt format;

	D_DEBUG_AT( VSP1_Renderer, "%s( " DFB_RECT_FORMAT " )\n", __FUNCTION__,
		    clip->left, clip->top, clip->width, clip->height );

	if (!vsp->flushed && vsp->input_count > 0) {
		format.width  = clip->width;
		format.height = clip->height;
		format.code   = V4L2_MBUS_FMT_ARGB8888_1X32;


		/* Set wpf.0:0 size */
		snprintf(buf, sizeof(buf), "'%s wpf.0':0", vsp->base.device_name);
		pad = media_parse_pad(vsp->base.media, buf, NULL);

		if (v4l2_subdev_set_format(pad->entity, &format, pad->index, V4L2_SUBDEV_FORMAT_ACTIVE)) {
			weston_log("wpf.0:0 set format failed: %dx%d.\n",
					 clip->width, clip->height);
		}


		/* Set bru:4 size */
		snprintf(buf, sizeof(buf), "'%s bru':4", vsp->base.device_name);
		pad = media_parse_pad(vsp->base.media, buf, NULL);

		if (v4l2_subdev_set_format(pad->entity, &format, pad->index, V4L2_SUBDEV_FORMAT_ACTIVE)) {
			weston_log("bru:4 set format failed: %dx%d.\n",
					 clip->width, clip->height);
		}


		/* Set wpf.0:0 crop */
		snprintf(buf, sizeof(buf), "'%s wpf.0':0", vsp->base.device_name);
		pad = media_parse_pad(vsp->base.media, buf, NULL);

		struct v4l2_rect x = { 0, 0, clip->width, clip->height };
		if (v4l2_subdev_set_selection(pad->entity, (struct v4l2_rect*) &x, pad->index,
								V4L2_SEL_TGT_CROP, V4L2_SUBDEV_FORMAT_ACTIVE)) {
			weston_log("wpf.0:0 set crop parameter failed: %dx%d@(%d,%d).\n",
					 x.width, x.height,
					 x.left, x.top);
		}


		/* Set wpf.0:1 compose (for offset) */
		snprintf(buf, sizeof(buf), "'%s wpf.0':1", vsp->base.device_name);
		pad = media_parse_pad(vsp->base.media, buf, NULL);

		if (v4l2_subdev_set_selection(pad->entity, (struct v4l2_rect*) clip, pad->index,
								V4L2_SEL_TGT_COMPOSE, V4L2_SUBDEV_FORMAT_ACTIVE)) {
			weston_log("wpf.0:1 set compose parameter failed: %dx%d@(%d,%d).\n",
					 clip->width, clip->height,
					 clip->left, clip->top);
		}


		/* program rpf.0... */
		vsp_comp_flush_views(vsp, clip);
	}
}

static int
vsp_comp_dequeue(struct vsp_device *vsp)
{
	int i, fd;
	int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	D_DEBUG_AT( VSP1_Renderer, "%s()\n", __FUNCTION__ );

	DBG("dequeue vsp composition.\n");

	D_ASSERT( vsp->flushed == 1 );

	// get an output pad
	fd = vsp->output_pad.fd;

	// dequeue buffer
	if (vsp_dequeue_buffer(fd, 1) < 0)
		goto error;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
		weston_log("%s: VIDIOC_STREAMOFF failed on %d (%s).\n", __func__, fd, strerror(errno));
		goto error;
	}

	// stream off
	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	for (i = 0; i < vsp->input_count; i++) {
		if (ioctl(vsp->input_pads[i].fd, VIDIOC_STREAMOFF, &type) == -1) {
			weston_log("VIDIOC_STREAMOFF failed for input %d.\n", i);
		}
	}

	// disable UDS if used
	if (vsp->scaler_count) {
		for (i = 0; i < vsp->input_count; i++) {
			if (vsp->use_scaler[i]) {
				vsp_comp_setup_inputs(vsp, &vsp->input_pads[i], vsp->use_scaler[i], NULL, 0);
				vsp->use_scaler[i]->input = -1;
				vsp->use_scaler[i] = NULL;
			}
		}
		vsp->scaler_count = 0;
	}

	vsp->input_count = 0;
	return 0;

error:
	video_debug_mediactl();
	vsp->input_count = 0;
	return -1;
}

static void
vsp_comp_finish(struct v4l2_renderer_device *dev)
{
	struct vsp_device *vsp = (struct vsp_device*)dev;

	D_DEBUG_AT( VSP1_Renderer, "%s()\n", __FUNCTION__ );

//	vsp_comp_flush(dev);

	if (vsp->flushed) {
		vsp_comp_dequeue(vsp);

		vsp->flushed = 0;
	}

	vsp->state = VSP_STATE_IDLE;
	DBG("complete vsp composition.\n");
	vsp->output_surface_state = NULL;
}

static int
vsp_comp_set_view(struct v4l2_renderer_device *dev, struct v4l2_surface_state *surface_state)
{
	struct vsp_device *vsp = (struct vsp_device*)dev;
	struct vsp_surface_state *vs = (struct vsp_surface_state*)surface_state;

	D_DEBUG_AT( VSP1_Renderer, "%s( state %p )\n", __FUNCTION__, surface_state );

	if (vs->base.src_rect.width > 8190 || vs->base.src_rect.height > 8190) {
		weston_log("ignoring the size exceeding the limit (8190x8190) < (%dx%d)\n", vs->base.src_rect.width, vs->base.src_rect.height);
		return -1;
	}

	if (vs->base.src_rect.width < 1 || vs->base.src_rect.height < 1) {
		weston_log("ignoring the size of zeros < (%dx%d)\n", vs->base.src_rect.width, vs->base.src_rect.height);
		return -1;
	}

	if (vs->base.src_rect.left < 0) {
		vs->base.src_rect.width += vs->base.src_rect.left;
		vs->base.src_rect.left = 0;
	}

	if (vs->base.src_rect.top < 0) {
		vs->base.src_rect.height += vs->base.src_rect.top;
		vs->base.src_rect.top = 0;
	}

	DBG("set input %d (dmafd=%d): %dx%d@(%d,%d)->%dx%d@(%d,%d). alpha=%f\n",
	    vsp->input_count,
	    vs->base.planes[0].dmafd,
	    vs->base.src_rect.width, vs->base.src_rect.height,
	    vs->base.src_rect.left, vs->base.src_rect.top,
	    vs->base.dst_rect.width, vs->base.dst_rect.height,
	    vs->base.dst_rect.left, vs->base.dst_rect.top,
	    vs->base.alpha);

	switch(vsp->state) {
	case VSP_STATE_START:
		DBG("VSP_STATE_START -> COMPSOING\n");
		vsp->state = VSP_STATE_COMPOSING;
		break;

	case VSP_STATE_COMPOSING:
		if (vsp->input_count == 0) {
			DBG("VSP_STATE_COMPOSING -> START (compose with output)\n");
			vsp->state = VSP_STATE_START;
			if (vsp_comp_set_view(dev, (struct v4l2_surface_state*)vsp->output_surface_state) < 0)
				return -1;
		}
		break;

	default:
		weston_log("unknown state... %d\n", vsp->state);
		return -1;
	}

	/* check if we need to use a scaler */
	if (vs->base.dst_rect.width != vs->base.src_rect.width ||
	    vs->base.dst_rect.height != vs->base.src_rect.height) {
		DBG("We need scaler! scaler! scaler! (%dx%d)->(%dx%d)\n",
		    vs->base.src_rect.width, vs->base.src_rect.height,
		    vs->base.dst_rect.width, vs->base.dst_rect.height);

		// if all scalers are oocupied, flush and then retry.
		if (vsp->scaler_count == vsp->scaler_max) {
		     D_BREAK("foo");
			vsp_comp_flush_views(vsp, NULL);
			return vsp_comp_set_view(dev, surface_state);
		}

		vsp->scalers[vsp->scaler_count].input = vsp->input_count;
		vsp->use_scaler[vsp->input_count] = &vsp->scalers[vsp->scaler_count];
		vsp->scaler_count++;
	}

	// get an available input pad
	vsp->input_surface_states[vsp->input_count] = vs;

	// check if we should flush now
	vsp->input_count++;
//	if (vsp->input_count == vsp->input_max)
//		vsp_comp_flush_views(vsp);

	return 0;
}

static void
vsp_set_output_buffer(struct v4l2_renderer_output *out, struct v4l2_bo_state *bo)
{
	struct vsp_renderer_output *output = (struct vsp_renderer_output*)out;
	DBG("set output dmafd to %d\n", bo->dmafd);
	output->surface_state.base.planes[0].dmafd = bo->dmafd;
	output->surface_state.fmt.fmt.pix_mp.plane_fmt[0].bytesperline = bo->stride;
}

static uint32_t
vsp_get_capabilities(void)
{
	return 0;
}

WL_EXPORT struct v4l2_device_interface v4l2_device_interface = {
	.init = vsp_init,

	.create_output = vsp_create_output,
	.set_output_buffer = vsp_set_output_buffer,

	.create_surface = vsp_create_surface,
	.attach_buffer = vsp_attach_buffer,

	.begin_compose = vsp_comp_begin,
	.finish_compose = vsp_comp_finish,
	.draw_view = vsp_comp_set_view,
	.flush = vsp_comp_flush,

	.get_capabilities = vsp_get_capabilities,
};
