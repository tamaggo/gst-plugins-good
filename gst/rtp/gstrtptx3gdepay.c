/* GStreamer
 * Copyright (C) <2015> Tamaggo
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "gstrtptx3gdepay.h"

GST_DEBUG_CATEGORY_STATIC (rtptx3gdepay_debug);
#define GST_CAT_DEFAULT (rtptx3gdepay_debug)

static GstStaticPadTemplate gst_rtp_tx3g_depay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw, format = { utf8 }"));

static GstStaticPadTemplate gst_rtp_tx3g_depay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "media = (string) \"text\", "
        "clock-rate = (int) 90000, " "encoding-name = (string) \"X-RAW\""));

#define gst_rtp_tx3g_depay_parent_class parent_class
G_DEFINE_TYPE (GstRtpTX3GDepay, gst_rtp_tx3g_depay,
    GST_TYPE_RTP_BASE_DEPAYLOAD);

static void gst_rtp_tx3g_depay_finalize (GObject * object);

static GstStateChangeReturn gst_rtp_tx3g_depay_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_rtp_tx3g_depay_setcaps (GstRTPBaseDepayload * depayload,
    GstCaps * caps);
static GstBuffer *gst_rtp_tx3g_depay_process (GstRTPBaseDepayload * depayload,
    GstBuffer * buf);

static void
gst_rtp_tx3g_depay_class_init (GstRtpTX3GDepayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstRTPBaseDepayloadClass *gstrtpbasedepayload_class;

  GST_DEBUG_CATEGORY_INIT (rtptx3gdepay_debug, "rtptx3gdepay", 0,
      "rtptx3gdepay element");

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstrtpbasedepayload_class = (GstRTPBaseDepayloadClass *) klass;

  gobject_class->finalize = gst_rtp_tx3g_depay_finalize;

  gstelement_class->change_state = gst_rtp_tx3g_depay_change_state;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_tx3g_depay_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_tx3g_depay_sink_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "RTP 3GPP Timed Text depayloader", "Codec/Depayloader/Network",
      "Extracts Timed Text buffers from RTP packets", "Tamaggo");

  gstrtpbasedepayload_class->set_caps = gst_rtp_tx3g_depay_setcaps;
  gstrtpbasedepayload_class->process = gst_rtp_tx3g_depay_process;
}

static void
gst_rtp_tx3g_depay_init (GstRtpTX3GDepay * rtptx3gdepay)
{
  rtptx3gdepay->adapter = gst_adapter_new ();
}

static void
gst_rtp_tx3g_depay_finalize (GObject * object)
{
  GstRtpTX3GDepay *rtptx3gdepay;

  rtptx3gdepay = GST_RTP_TX3G_DEPAY (object);

  g_object_unref (rtptx3gdepay->adapter);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_rtp_tx3g_depay_free_frag_array (GstRtpTX3GDepay * rtptx3gdepay)
{

  for (int f = 0; f < rtptx3gdepay->frag_count; f++) {
    if (rtptx3gdepay->frag_array[f]) {
      gst_buffer_unref (rtptx3gdepay->frag_array[f]);
      rtptx3gdepay->frag_array[f] = NULL;
    }
  }
  rtptx3gdepay->frag_count = 0;
}

static void
gst_rtp_tx3g_depay_reset (GstRtpTX3GDepay * rtptx3gdepay, gboolean full)
{
  gst_adapter_clear (rtptx3gdepay->adapter);
  gst_rtp_tx3g_depay_free_frag_array (rtptx3gdepay);
}



static gboolean
gst_rtp_tx3g_depay_setcaps (GstRTPBaseDepayload * depayload, GstCaps * caps)
{
  GstStructure *structure;
  gint clock_rate;
  GstCaps *srccaps;
  gboolean res;

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "clock-rate", &clock_rate))
    clock_rate = 90000;
  depayload->clock_rate = clock_rate;

  srccaps = gst_caps_new_empty_simple ("text/x-raw");
  res = gst_pad_set_caps (depayload->srcpad, srccaps);
  gst_caps_unref (srccaps);

  return res;
}





static GstBuffer *
gst_rtp_tx3g_depay_process (GstRTPBaseDepayload * depayload, GstBuffer * buf)
{
  __attribute__ ((unused)) guint8 sidx = 0;
  guint32 sdur = 0;
  guint sample_len;
  guint tx3g_header_len = 0;
  guint8 frag_index = 0;
  GstRtpTX3GDepay *rtptx3gdepay;
  GstBuffer *subbuf, *outbuf = NULL;
  gint payload_len;
  guint8 *payload;
  GstRtpTX3GDepayPacketFlags flags;
  GstRTPBuffer rtp = { NULL };
  GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buf);

  rtptx3gdepay = GST_RTP_TX3G_DEPAY (depayload);

  gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp);

  payload_len = gst_rtp_buffer_get_payload_len (&rtp);

  if (payload_len <= 9)
    goto empty_packet;

  if (GST_BUFFER_IS_DISCONT (buf)) {
    GST_WARNING_OBJECT (rtptx3gdepay, "DISCONT, clear adapter");
    gst_adapter_clear (rtptx3gdepay->adapter);
  }

  payload = gst_rtp_buffer_get_payload (&rtp);

  flags.byte_field = payload[0];

  if (flags.packet_type == GST_RTP_TX3G_DP_WHOLE_TEXT) {
    /*
     * Type 1 Header
     * 0                   1                   2                   3
     * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |U|   R   |TYPE |       LEN  (always >=8)       |    SIDX       |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                      SDUR                     |     TLEN      |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |      TLEN     |
     * +-+-+-+-+-+-+-+-+
     *
     * SIDX: (8 bits) "Text Sample Entry Index"
     *   This is an index used to identify the sample descriptions.
     * SDUR: (24 bits) "Text Sample Duration"
     *   Indicates the sample duration in RTP timestamp units of the text sample.
     * TLEN: (16 bits) "Text String Length"
     *   Indicates the byte count of the text string
     */
    sidx = payload[3];
    sdur = payload[4];
    sdur <<= 8;
    sdur |= payload[5];
    sdur <<= 8;
    sdur |= payload[6];
    sample_len = payload[7];
    sample_len <<= 8;
    sample_len |= payload[8];
    tx3g_header_len = 9;
    frag_index = 1;

    gst_rtp_tx3g_depay_free_frag_array (rtptx3gdepay);
    rtptx3gdepay->frag_count = 1;


  } else if (flags.packet_type == GST_RTP_TX3G_DP_FRAGMENT_TEXT) {
    /*
     * Type 2 Header
     * 0                   1                   2                   3
     * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |U|   R   |TYPE |          LEN( always >9)      | TOTAL | THIS  |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                    SDUR                       |    SIDX       |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |               SLEN            |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *
     * SIDX: (8 bits) "Text Sample Entry Index"
     *   This is an index used to identify the sample descriptions.
     * SDUR: (24 bits) "Text Sample Duration"
     *   Indicates the sample duration in RTP timestamp units of the text sample.
     * TOTAL: (4 bits) "Total Fragments in Sample"
     * THIS: (4 bites) "Position of this fragment"
     * SLEN: (16 bits) "Text String Length"
     *   Indicates the size (in bytes) of the original whole text sample to
     *   which this fragment belongs
     */
    frag_index = payload[3] & 0x0F;

    if (frag_index == 1) {
      gst_rtp_tx3g_depay_free_frag_array (rtptx3gdepay);
      rtptx3gdepay->frag_count = payload[3] >> 4;
    }

    sidx = payload[7];
    sdur = payload[4];
    sdur <<= 8;
    sdur |= payload[5];
    sdur <<= 8;
    sdur |= payload[6];
    sample_len = payload[8];
    sample_len <<= 8;
    sample_len |= payload[9];
    tx3g_header_len = 10;
  }

  /* subbuffer skipping the header bytes */
  subbuf = gst_rtp_buffer_get_payload_subbuffer (&rtp, tx3g_header_len, -1);

  if (frag_index > 0) {
    rtptx3gdepay->frag_array[frag_index - 1] = subbuf;
    gst_buffer_ref (subbuf);
  }

  if (gst_rtp_buffer_get_marker (&rtp)) {
    guint avail;

    for (int f = 0; f < rtptx3gdepay->frag_count; f++) {
      if (rtptx3gdepay->frag_array[f] == NULL) {
        goto wrong_frag;
      }
      gst_adapter_push (rtptx3gdepay->adapter, rtptx3gdepay->frag_array[f]);
    }

    gst_rtp_tx3g_depay_free_frag_array (rtptx3gdepay);

    /* take the buffer */
    avail = gst_adapter_available (rtptx3gdepay->adapter);
    outbuf = gst_adapter_take_buffer (rtptx3gdepay->adapter, avail);

    if (avail) {
      GstBuffer *temp;

      GST_DEBUG_OBJECT (rtptx3gdepay, "sub buffer: size %u", avail);

      temp = gst_buffer_copy_region (outbuf, GST_BUFFER_COPY_ALL, 0, avail);

      gst_buffer_unref (outbuf);
      outbuf = temp;

      GST_BUFFER_TIMESTAMP (outbuf) = timestamp;
      GST_BUFFER_DURATION (outbuf) =
          gst_util_uint64_scale (sdur, GST_SECOND, 90000);

    } else {
      gst_buffer_unref (outbuf);
      outbuf = NULL;
    }
  }
  gst_rtp_buffer_unmap (&rtp);

  return outbuf;

  /* ERRORS */
empty_packet:
  {
    GST_ELEMENT_WARNING (rtptx3gdepay, STREAM, DECODE,
        ("Empty Payload."), (NULL));
    gst_rtp_buffer_unmap (&rtp);
    return NULL;
  }

wrong_frag:
  {
    gst_adapter_clear (rtptx3gdepay->adapter);
    gst_rtp_tx3g_depay_free_frag_array (rtptx3gdepay);
    gst_rtp_buffer_unmap (&rtp);
    GST_LOG_OBJECT (rtptx3gdepay, "wrong fragment, skipping");
    return NULL;
  }

}

static GstStateChangeReturn
gst_rtp_tx3g_depay_change_state (GstElement * element,
    GstStateChange transition)
{
  GstRtpTX3GDepay *rtptx3gdepay;
  GstStateChangeReturn ret;

  rtptx3gdepay = GST_RTP_TX3G_DEPAY (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_rtp_tx3g_depay_reset (rtptx3gdepay, TRUE);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rtp_tx3g_depay_reset (rtptx3gdepay, TRUE);
      break;
    default:
      break;
  }
  return ret;
}


gboolean
gst_rtp_tx3g_depay_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rtptx3gdepay",
      GST_RANK_MARGINAL, GST_TYPE_RTP_TX3G_DEPAY);
}
