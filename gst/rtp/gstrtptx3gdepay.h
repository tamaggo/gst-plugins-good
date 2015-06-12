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

#ifndef __GST_RTP_TX3G_DEPAY_H__
#define __GST_RTP_TX3G_DEPAY_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/rtp/gstrtpbasedepayload.h>

G_BEGIN_DECLS

#define GST_TYPE_RTP_TX3G_DEPAY \
  (gst_rtp_tx3g_depay_get_type())
#define GST_RTP_TX3G_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_TX3G_DEPAY,GstRtpTX3GDepay))
#define GST_RTP_TX3G_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_TX3G_DEPAY,GstRtpTX3GDepayClass))
#define GST_IS_RTP_TX3G_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_TX3G_DEPAY))
#define GST_IS_RTP_TX3G_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_TX3G_DEPAY))

typedef struct _GstRtpTX3GDepayPacketFlags GstRtpTX3GDepayPacketFlags;
typedef struct _GstRtpTX3GDepay GstRtpTX3GDepay;
typedef struct _GstRtpTX3GDepayClass GstRtpTX3GDepayClass;

typedef enum {
  GST_RTP_TX3G_DP_WHOLE_TEXT         = 1,
  GST_RTP_TX3G_DP_FRAGMENT_TEXT      = 2,
  GST_RTP_TX3G_DP_MODIFIER_BOX       = 3,
  GST_RTP_TX3G_DP_MODIFIER_FRAGMENT  = 4,
  GST_RTP_TX3G_DP_SAMPLE_DESCRIPTION = 5,
} GstRtpTX3GDepayPacketType;


struct _GstRtpTX3GDepayPacketFlags
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


struct _GstRtpTX3GDepay
{
  GstRTPBaseDepayload depayload;

  GstAdapter *adapter;

  GstTagList *tags;
  gchar *stream_id;
  GstBuffer* frag_array[256];
  guint8 frag_count;

};

struct _GstRtpTX3GDepayClass
{
  GstRTPBaseDepayloadClass parent_class;
};

GType gst_rtp_tx3g_depay_get_type (void);

gboolean gst_rtp_tx3g_depay_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_RTP_GST_DEPAY_H__ */
