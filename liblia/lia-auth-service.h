/*
 * lia-auth-service.h
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

#ifndef __LIA_AUTH_SERVICE_H__
#define __LIA_AUTH_SERVICE_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _LiaAuthService LiaAuthService;
typedef struct _LiaAuthServiceClass LiaAuthServiceClass;
typedef struct _LiaAuthServicePrivate LiaAuthServicePrivate;

struct _LiaAuthService
{
  GObject parent;

  LiaAuthServicePrivate *priv;
};

struct _LiaAuthServiceClass
{
  GObjectClass parent_class;
};


#define LIA_TYPE_AUTH_SERVICE           (lia_auth_service_get_type ())
#define LIA_AUTH_SERVICE(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIA_TYPE_AUTH_SERVICE, LiaAuthService))
#define LIA_AUTH_SERVICE_CLASS(obj)     (G_TYPE_CHECK_CLASS_CAST ((obj), LIA_TYPE_AUTH_SERVICE, LiaAuthServiceClass))
#define LIA_IS_AUTH_SERVICE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIA_TYPE_AUTH_SERVICE))
#define LIA_IS_AUTH_SERVICE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE ((obj), LIA_TYPE_AUTH_SERVICE))
#define LIA_AUTH_SERVICE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), LIA_TYPE_AUTH_SERVICE, LiaAuthServiceClass))


GType             lia_auth_service_get_type            (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __LIA_AUTH_SERVICE_H__ */
