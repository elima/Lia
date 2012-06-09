/*
 * lia-webview.h
 *
 * This file is part of Lia <http://free-social.net/lia/>
 *
 * Copyright (C) 2010 Igalia S.L.
 *
 * Authors:
 *   Eduardo Lima Mitev <elima@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License at http://www.gnu.org/licenses/gpl-3.0.txt
 * for more details.
 */

#ifndef __LIA_WEBVIEW_H__
#define __LIA_WEBVIEW_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <lia-defines.h>
#include <lia-application.h>

G_BEGIN_DECLS

typedef struct _LiaWebview LiaWebview;
typedef struct _LiaWebviewClass LiaWebviewClass;
typedef struct _LiaWebviewPrivate LiaWebviewPrivate;

struct _LiaWebview
{
  LiaApplication parent;

  LiaWebviewPrivate *priv;
};

struct _LiaWebviewClass
{
  LiaApplicationClass parent_class;
};


#define LIA_TYPE_WEBVIEW           (lia_webview_get_type ())
#define LIA_WEBVIEW(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIA_TYPE_WEBVIEW, LiaWebview))
#define LIA_WEBVIEW_CLASS(obj)     (G_TYPE_CHECK_CLASS_CAST ((obj), LIA_TYPE_WEBVIEW, LiaWebviewClass))
#define LIA_IS_WEBVIEW(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIA_TYPE_WEBVIEW))
#define LIA_IS_WEBVIEW_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE ((obj), LIA_TYPE_WEBVIEW))
#define LIA_WEBVIEW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), LIA_TYPE_WEBVIEW, LiaWebviewClass))


GType             lia_webview_get_type            (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __LIA_WEBVIEW_H__ */
