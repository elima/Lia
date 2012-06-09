/*
 * lia-core.h
 *
 * This file is part of Lia <http://free-social.net/lia>
 *
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef __LIA_CORE_H__
#define __LIA_CORE_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <lia-defines.h>
#include <lia-application.h>

G_BEGIN_DECLS

typedef struct _LiaCore LiaCore;
typedef struct _LiaCoreClass LiaCoreClass;
typedef struct _LiaCorePrivate LiaCorePrivate;

struct _LiaCore
{
  LiaApplication parent;

  /* private structure */
  LiaCorePrivate *priv;
};

struct _LiaCoreClass
{
  LiaApplicationClass parent_class;

  /* Signal prototypes */
  void (* signal_example) (LiaCore *self,
			   gpointer some_data);
};


#define LIA_TYPE_CORE           (lia_core_get_type ())
#define LIA_CORE(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIA_TYPE_CORE, LiaCore))
#define LIA_CORE_CLASS(obj)     (G_TYPE_CHECK_CLASS_CAST ((obj), LIA_TYPE_CORE, LiaCoreClass))
#define LIA_IS_CORE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIA_TYPE_CORE))
#define LIA_IS_CORE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE ((obj), LIA_TYPE_CORE))
#define LIA_CORE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), LIA_TYPE_CORE, LiaCoreClass))


GType             lia_core_get_type            (void) G_GNUC_CONST;

const gchar *     lia_core_get_dbus_address    (LiaCore *self);

gboolean          lia_core_launch              (LiaCore      *self,
                                                const gchar  *command_line,
                                                gchar       **env,
                                                GError      **error);

G_END_DECLS

#endif /* __LIA_CORE_H__ */
