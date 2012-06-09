/*
 * lia-application.h
 *
 * This file is part of Lia <http://free-social.net/lia/>
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

#ifndef __LIA_APPLICATION_H__
#define __LIA_APPLICATION_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <lia-defines.h>

G_BEGIN_DECLS

typedef struct _LiaApplication LiaApplication;
typedef struct _LiaApplicationClass LiaApplicationClass;
typedef struct _LiaApplicationPrivate LiaApplicationPrivate;

/**
 * LiaBusMethodCallFunc:
 * @app: The #LiaApplication
 * @bus_type:
 * @caller_id:
 * @object_path:
 * @interface_name:
 * @method_name:
 * @arguments:
 * @invocation:
 * @user_data:
 *
 **/
typedef void (* LiaBusMethodCallFunc) (LiaApplication        *app,
                                       LiaBusType             bus_type,
                                       const gchar           *caller_id,
                                       const gchar           *object_path,
                                       const gchar           *interface_name,
                                       const gchar           *method_name,
                                       GVariant              *arguments,
                                       GDBusMethodInvocation *invocation,
                                       gpointer               user_data);

struct _LiaApplication
{
  GObject parent;

  LiaApplicationPrivate *priv;
};

struct _LiaApplicationClass
{
  GObjectClass parent_class;

  /* virtual methods */
  void     (* init_async_finished) (LiaApplication *self,
                                    GAsyncResult   *result,
                                    gint            io_priority,
                                    GCancellable   *cancellable);

  void     (* register_objects)    (LiaApplication *self, LiaBusType bus_type);

  gboolean (* get_bus_addresses)   (LiaApplication  *self,
                                    gchar          **priv_bus_addr,
                                    gchar          **prot_bus_addr,
                                    gchar          **pub_bus_addr,
                                    GError         **error);

  gboolean (* load_env)            (LiaApplication  *self,
                                    gchar          **service_name,
                                    GError         **error);

  /* signals */
  void (* signal_register_objects) (LiaApplication  *self,
                                    LiaBusType       bus_type,
                                    gpointer         user_data);

  void (* signal_ready)            (LiaApplication *self,
                                    gpointer        user_data);
};

#define LIA_TYPE_APPLICATION           (lia_application_get_type ())
#define LIA_APPLICATION(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIA_TYPE_APPLICATION, LiaApplication))
#define LIA_APPLICATION_CLASS(obj)     (G_TYPE_CHECK_CLASS_CAST ((obj), LIA_TYPE_APPLICATION, LiaApplicationClass))
#define LIA_IS_APPLICATION(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIA_TYPE_APPLICATION))
#define LIA_IS_APPLICATION_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE ((obj), LIA_TYPE_APPLICATION))
#define LIA_APPLICATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), LIA_TYPE_APPLICATION, LiaApplicationClass))


GType             lia_application_get_type                       (void) G_GNUC_CONST;

gint              lia_application_run                            (LiaApplication *self);
void              lia_application_quit                           (LiaApplication *self,
                                                                  gint            exit_code);


const gchar *     lia_application_get_service_name               (LiaApplication *self);
const gchar *     lia_application_get_base_service_name          (LiaApplication *self);
const gchar *     lia_application_get_core_service_name          (LiaApplication *self);

GDBusConnection * lia_application_get_bus                        (LiaApplication *self,
                                                                  LiaBusType      bus_type);
const gchar *     lia_application_get_bus_address                (LiaApplication *self,
                                                                  LiaBusType      bus_type);

guint             lia_application_register_object                (LiaApplication        *self,
                                                                  guint                  bus_type,
                                                                  const gchar           *object_path,
                                                                  const gchar           *interface_xml,
                                                                  LiaBusMethodCallFunc   method_call_func,
                                                                  gpointer               user_data,
                                                                  GDestroyNotify         user_data_free_func,
                                                                  GError               **error);
gboolean          lia_application_unregister_object              (LiaApplication *self,
                                                                  guint           bus_type,
                                                                  guint           registration_id);

G_END_DECLS

#endif /* __LIA_APPLICATION_H__ */
