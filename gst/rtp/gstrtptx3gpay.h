/* GStreamer
 * Copyright (C) <2015> Tamaggo Inc <jvidunas@tamaggo.com>
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

#ifndef __GST_RTP_TX3G_PAY_H__
#define __GST_RTP_TX3G_PAY_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtpbasepayload.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#define GST_TYPE_RTP_TX3G_PAY \
	(gst_rtp_tx3g_pay_get_type())
#define GST_RTP_TX3G_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_TX3G_PAY,GstRtpTX3GPay))
#define GST_RTP_TX3G_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_TX3G_PAY,GstRtpTX3GPayClass))
#define GST_IS_RTP_TX3G_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_TX3G_PAY))
#define GST_IS_RTP_TX3G_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_TX3G_PAY))

typedef struct _GstRtpTX3GPayPacketFlags GstRtpTX3GPayPacketFlags;
typedef struct _GstRtpTX3GPay GstRtpTX3GPay;
typedef struct _GstRtpTX3GPayClass GstRtpTX3GPayClass;

typedef enum {
  GST_RTP_TX3G_P_WHOLE_TEXT         = 1,
  GST_RTP_TX3G_P_FRAGMENT_TEXT      = 2,
  GST_RTP_TX3G_P_MODIFIER_BOX       = 3,
  GST_RTP_TX3G_P_MODIFIER_FRAGMENT  = 4,
  GST_RTP_TX3G_P_SAMPLE_DESCRIPTION = 5,
} GstRtpTX3GPayPacketType;

struct _GstRtpTX3GPayPacketFlags
{
  union
  {
    struct
    {
      guint8 utf_flag:1;
      guint8 reserved:4;
      guint8 packet_type:3;
    };
    guint8 byte_field;
  };
};

struct _GstRtpTX3GPay
{
  GstRTPBasePayload payload;

  GList *pending_buffers; /* GstBufferList */
  GstAdapter *adapter;
  guint8 utf_flag;
  gchar *stream_id;

  GstTagList *taglist;
};

struct _GstRtpTX3GPayClass
{
  GstRTPBasePayloadClass parent_class;
};

GType gst_rtp_tx3g_pay_get_type (void);

gboolean gst_rtp_tx3g_pay_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_RTP_GST_PAY_H__ */
