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

#include <gst/rtp/gstrtpbuffer.h>

#include "gstrtptx3gpay.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtp_pay_debug);
#define GST_CAT_DEFAULT gst_rtp_pay_debug

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |C| CV  |D|0|0|0|     ETYPE     |  MBZ                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Frag_offset                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * C: caps inlined flag
 *   When C set, first part of payload contains caps definition. Caps definition
 *   starts with variable-length length prefix and then a string of that length.
 *   the length is encoded in big endian 7 bit chunks, the top 1 bit of a byte
 *   is the continuation marker and the 7 next bits the data. A continuation
 *   marker of 1 means that the next byte contains more data.
 *
 * CV: caps version, 0 = caps from SDP, 1 - 7 inlined caps
 * D: delta unit buffer
 * ETYPE: type of event. Payload contains the event, prefixed with a
 *        variable length field.
 *   0 = NO event
 *   1 = GST_EVENT_TAG
 *   2 = GST_EVENT_CUSTOM_DOWNSTREAM
 *   3 = GST_EVENT_CUSTOM_BOTH
 *   4 = GST_EVENT_STREAM_START
 */

/*
*   0                   1                   2                   3
*   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*   |V=2|P|X| CC    |M|    PT       |        sequence number        |
*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*   |                           timestamp                           |
*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*   |           synchronization source (SSRC) identifier            |
*  /+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | |U|   R   | TYPE|             LEN               |               :
* | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               :
*U| :           (variable header fields depending on TYPE           :
*N| :                                                               :
*I< +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*T| |                                                               |
* | :                    SAMPLE CONTENTS                            :
* | |                                               +-+-+-+-+-+-+-+-+
* | |                                               |
*  \+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*
*  U: UTF Transformation Flag
*    This is used to inform receivers whether UTF-8(U=0) or UTF-16 (U=1)
*    was used to encode the text string
*  R: Reserved bits for future extensions.  Must be zero
*  TYPE: Type Field
*    This field specifies which specific header fields follow.
*    1 = whole text Sample
*    2 = text string fragment
*    3 = whole modifier box or the first fragment of a modifier box
*    4 = modifier fragment other than the first
*    5 = sample description
*    0,6,7 = reserved for future extensions
*  LEN: Length Field
*    Indicates the size (in bytes) of this header field and all the fields
*    that follow
*/

static GstStaticPadTemplate gst_rtp_tx3g_pay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw, format = { utf8 }"));

static GstStaticPadTemplate gst_rtp_tx3g_pay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "media = (string) \"text\", "
        "payload = (int) " GST_RTP_PAYLOAD_DYNAMIC_STRING ", "
        "clock-rate = (int) 90000, " "encoding-name = (string) \"X-RAW\"")
    );

static void gst_rtp_tx3g_pay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rtp_tx3g_pay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_rtp_tx3g_pay_finalize (GObject * obj);
static GstStateChangeReturn gst_rtp_tx3g_pay_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_rtp_tx3g_pay_setcaps (GstRTPBasePayload * payload,
    GstCaps * caps);
static GstFlowReturn gst_rtp_tx3g_pay_handle_buffer (GstRTPBasePayload *
    payload, GstBuffer * buffer);
static gboolean gst_rtp_tx3g_pay_sink_event (GstRTPBasePayload * payload,
    GstEvent * event);

#define gst_rtp_tx3g_pay_parent_class parent_class
G_DEFINE_TYPE (GstRtpTX3GPay, gst_rtp_tx3g_pay, GST_TYPE_RTP_BASE_PAYLOAD);

static void
gst_rtp_tx3g_pay_class_init (GstRtpTX3GPayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstRTPBasePayloadClass *gstrtpbasepayload_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstrtpbasepayload_class = (GstRTPBasePayloadClass *) klass;

  gobject_class->set_property = gst_rtp_tx3g_pay_set_property;
  gobject_class->get_property = gst_rtp_tx3g_pay_get_property;
  gobject_class->finalize = gst_rtp_tx3g_pay_finalize;

  gstelement_class->change_state = gst_rtp_tx3g_pay_change_state;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_tx3g_pay_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_tx3g_pay_sink_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "RTP 3GPP Timed Text payloader", "Codec/Payloader/Network/RTP",
      "Payload Timed Text buffers as RTP packets", "Tamaggo");

  gstrtpbasepayload_class->set_caps = gst_rtp_tx3g_pay_setcaps;
  gstrtpbasepayload_class->handle_buffer = gst_rtp_tx3g_pay_handle_buffer;
  gstrtpbasepayload_class->sink_event = gst_rtp_tx3g_pay_sink_event;

  GST_DEBUG_CATEGORY_INIT (gst_rtp_pay_debug, "rtptx3gpay", 0,
      "rtptx3gpay element");
}

static void
gst_rtp_tx3g_pay_init (GstRtpTX3GPay * rtptx3gpay)
{
  rtptx3gpay->adapter = gst_adapter_new ();
  rtptx3gpay->pending_buffers = NULL;
  rtptx3gpay->taglist = NULL;
}

static void
gst_rtp_tx3g_pay_reset (GstRtpTX3GPay * rtptx3gpay, gboolean full)
{
  gst_adapter_clear (rtptx3gpay->adapter);
  if (rtptx3gpay->pending_buffers)
    g_list_free_full (rtptx3gpay->pending_buffers,
        (GDestroyNotify) gst_buffer_list_unref);
  rtptx3gpay->pending_buffers = NULL;
  if (full) {
    if (rtptx3gpay->taglist)
      gst_tag_list_unref (rtptx3gpay->taglist);
    rtptx3gpay->taglist = NULL;
  }
}

static void
gst_rtp_tx3g_pay_finalize (GObject * obj)
{
  GstRtpTX3GPay *rtptx3gpay;

  rtptx3gpay = GST_RTP_TX3G_PAY (obj);

  gst_rtp_tx3g_pay_reset (rtptx3gpay, TRUE);

  g_object_unref (rtptx3gpay->adapter);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_rtp_tx3g_pay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_tx3g_pay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_rtp_tx3g_pay_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpTX3GPay *rtptx3gpay;
  GstStateChangeReturn ret;

  rtptx3gpay = GST_RTP_TX3G_PAY (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_rtp_tx3g_pay_reset (rtptx3gpay, TRUE);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rtp_tx3g_pay_reset (rtptx3gpay, TRUE);
      break;
    default:
      break;
  }
  return ret;
}

#define RTP_HEADER_LEN 12
#define TX3G_MIN_HEADER_LEN 9

static gboolean                 //__attribute__((optimize("O0")))
gst_rtp_tx3g_pay_create_from_adapter (GstRtpTX3GPay * rtptx3gpay,
    GstClockTime timestamp)
{
  guint mtu;
  guint sample_len, sample_left;
  guint frag_index;
  guint frag_count;
  guint frag_offset;
  GstBufferList *list;
  guint tx3g_header_len = TX3G_MIN_HEADER_LEN;

  sample_len = gst_adapter_available (rtptx3gpay->adapter);
  if (sample_len == 0)
    return FALSE;

  sample_left = sample_len;

  mtu = GST_RTP_BASE_PAYLOAD_MTU (rtptx3gpay);

  frag_count = (sample_len / (mtu - (RTP_HEADER_LEN + tx3g_header_len))) + 1;

  if (frag_count > 1) {
    tx3g_header_len += 1;
    frag_count = (sample_len / (mtu - (RTP_HEADER_LEN + tx3g_header_len))) + 1;
  }


  list = gst_buffer_list_new_sized (frag_count);
  frag_index = 1;
  frag_offset = 0;

  while (sample_left) {
    guint8 sidx = 0;
    guint32 sdur = 0;
    guint8 *payload;
    guint payload_len;
    guint packet_len;
    GstBuffer *outbuf;
    GstRTPBuffer rtp = { NULL };
    GstBuffer *paybuf;
    GstRtpTX3GPayPacketFlags flags;


    /* this will be the total length of the packet */
    packet_len =
        gst_rtp_buffer_calc_packet_len (tx3g_header_len + sample_left, 0, 0);

    /* fill one MTU or all available bytes */
    packet_len = MIN (packet_len, mtu);

    /* this is the payload length */
    payload_len = gst_rtp_buffer_calc_payload_len (packet_len, 0, 0);

    /* create buffer to hold the header */
    outbuf = gst_rtp_buffer_new_allocate (tx3g_header_len, 0, 0);

    gst_rtp_buffer_map (outbuf, GST_MAP_WRITE, &rtp);
    payload = gst_rtp_buffer_get_payload (&rtp);

    GST_DEBUG_OBJECT (rtptx3gpay, "new packet len %u, frag %u", packet_len,
        frag_offset);

    flags.utf_flag = 0;
    flags.reserved = 0;
    if (frag_count == 1) {
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


      flags.packet_type = GST_RTP_TX3G_P_WHOLE_TEXT;
      payload[3] = sidx;
      payload[4] = sdur >> 16;
      payload[5] = sdur >> 8;
      payload[6] = sdur & 0xff;
      payload[7] = sample_len >> 8;
      payload[8] = sample_len & 0xff;
    } else {

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

      flags.packet_type = GST_RTP_TX3G_P_FRAGMENT_TEXT;
      payload[3] = frag_count << 4 | frag_index;
      payload[4] = sdur >> 16;
      payload[5] = sdur >> 8;
      payload[6] = sdur & 0xff;
      payload[7] = sidx;
      payload[8] = sample_len >> 8;
      payload[9] = sample_len & 0xff;
    }

    payload[0] = flags.byte_field;
    payload[1] = (payload_len - 1) >> 8;
    payload[2] = (payload_len - 1) & 0xff;

    payload_len -= tx3g_header_len;

    frag_offset += payload_len;
    sample_left -= payload_len;

    if (sample_left == 0)
      gst_rtp_buffer_set_marker (&rtp, TRUE);

    gst_rtp_buffer_unmap (&rtp);

    /* create a new buf to hold the payload */
    GST_DEBUG_OBJECT (rtptx3gpay, "take %u bytes from adapter", payload_len);
    paybuf = gst_adapter_take_buffer_fast (rtptx3gpay->adapter, payload_len);

    /* create a new group to hold the rtp header and the payload */
    gst_buffer_append (outbuf, paybuf);

    GST_BUFFER_TIMESTAMP (outbuf) = timestamp;

    /* and add to list */
    gst_buffer_list_insert (list, -1, outbuf);
  }

  rtptx3gpay->pending_buffers =
      g_list_append (rtptx3gpay->pending_buffers, list);

  return TRUE;
}

static GstFlowReturn
gst_rtp_tx3g_pay_flush (GstRtpTX3GPay * rtptx3gpay, GstClockTime timestamp)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GList *iter;

  gst_rtp_tx3g_pay_create_from_adapter (rtptx3gpay, timestamp);

  iter = rtptx3gpay->pending_buffers;
  while (iter) {
    GstBufferList *list = iter->data;

    rtptx3gpay->pending_buffers = iter =
        g_list_delete_link (rtptx3gpay->pending_buffers, iter);

    /* push the whole buffer list at once */
    ret = gst_rtp_base_payload_push_list (GST_RTP_BASE_PAYLOAD (rtptx3gpay),
        list);
    if (ret != GST_FLOW_OK)
      break;
  }

  return ret;
}

static gboolean
gst_rtp_tx3g_pay_setcaps (GstRTPBasePayload * payload, GstCaps * caps)
{
  gboolean res;
  gint rate;
  GstStructure *structure;

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "rate", &rate))
    rate = 90000;               /* default */

  gst_rtp_base_payload_set_options (payload, "text", TRUE, "X-RAW", 90000);
  res = gst_rtp_base_payload_set_outcaps (payload, NULL);

  return res;
}

static gboolean
gst_rtp_tx3g_pay_sink_event (GstRTPBasePayload * payload, GstEvent * event)
{
  gboolean ret;
  GstRtpTX3GPay *rtptx3gpay;

  rtptx3gpay = GST_RTP_TX3G_PAY (payload);

  ret =
      GST_RTP_BASE_PAYLOAD_CLASS (parent_class)->sink_event (payload,
      gst_event_ref (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      gst_rtp_tx3g_pay_reset (rtptx3gpay, FALSE);
      break;
    case GST_EVENT_TAG:{
      GstTagList *tags;

      gst_event_parse_tag (event, &tags);

      if (gst_tag_list_get_scope (tags) == GST_TAG_SCOPE_STREAM) {
        GstTagList *old;

        GST_DEBUG_OBJECT (rtptx3gpay, "storing stream tags %" GST_PTR_FORMAT,
            tags);
        if ((old = rtptx3gpay->taglist))
          gst_tag_list_unref (old);
        rtptx3gpay->taglist = gst_tag_list_ref (tags);
      }
      break;
    }
    case GST_EVENT_STREAM_START:{
      const gchar *stream_id = NULL;

      if (rtptx3gpay->taglist)
        gst_tag_list_unref (rtptx3gpay->taglist);
      rtptx3gpay->taglist = NULL;

      gst_event_parse_stream_start (event, &stream_id);
      if (stream_id) {
        if (rtptx3gpay->stream_id)
          g_free (rtptx3gpay->stream_id);
        rtptx3gpay->stream_id = g_strdup (stream_id);
      }
      break;
    }
    default:
      GST_LOG_OBJECT (rtptx3gpay, "no event for %s",
          GST_EVENT_TYPE_NAME (event));
      break;
  }

  gst_event_unref (event);

  return ret;
}


static GstFlowReturn
gst_rtp_tx3g_pay_handle_buffer (GstRTPBasePayload * basepayload,
    GstBuffer * buffer)
{
  GstFlowReturn ret;
  GstRtpTX3GPay *rtptx3gpay;
  GstClockTime timestamp;

  rtptx3gpay = GST_RTP_TX3G_PAY (basepayload);

  timestamp = GST_BUFFER_TIMESTAMP (buffer);

  gst_adapter_push (rtptx3gpay->adapter, buffer);
  ret = gst_rtp_tx3g_pay_flush (rtptx3gpay, timestamp);

  return ret;
}

gboolean
gst_rtp_tx3g_pay_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rtptx3gpay",
      GST_RANK_NONE, GST_TYPE_RTP_TX3G_PAY);
}
