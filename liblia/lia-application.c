/*
 * lia-application.c
 *
 * This file is part of Lia <http://free-social.net/lia/>
 *
 * Copyright (C) 2011-2012 Igalia S.L.
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

#include <string.h>
#include <evd.h>

#include "lia-application.h"
#include "lia-marshal.h"

#define LIA_APPLICATION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                          LIA_TYPE_APPLICATION, \
                                          LiaApplicationPrivate))

/* private data */
struct _LiaApplicationPrivate
{
  GSimpleAsyncResult *async_result;
  gboolean initialized;

  GDBusInterfaceVTable reg_obj_vtable;

  EvdDaemon *daemon;

  gchar *base_service_name;
  gchar *service_name;

  guint8 create_bus_conn_ops;
  guint8 acquire_bus_name_ops;

  gchar *bus_addr[3];
  GDBusConnection *bus_conn[3];
  guint service_name_owner_id[3];

  gchar *webview_html_root;
};

typedef struct
{
  GSimpleAsyncResult *result;
  LiaBusType bus_type;
  GCancellable *cancellable;
} BusConnData;

typedef struct
{
  LiaApplication *self;
  gpointer user_data;
  GDestroyNotify user_data_free_func;
  LiaBusMethodCallFunc method_call_func;
  LiaBusType bus_type;
} RegObjData;

/* signals */
enum
{
  SIGNAL_READY,
  SIGNAL_EXPORT_OBJECTS,
  SIGNAL_LAST
};

static guint lia_application_signals [SIGNAL_LAST] = { 0 };

/* properties */
enum
{
  PROP_0,
  PROP_BASE_SERVICE_NAME,
  PROP_SERVICE_NAME,
  PROP_WEBVIEW_HTML_ROOT
};


static void     lia_application_class_init                (LiaApplicationClass *class);
static void     lia_application_init                      (LiaApplication *self);

static void     finalize                                  (GObject *obj);
static void     dispose                                   (GObject *obj);
static void     set_property                              (GObject      *obj,
                                                           guint         prop_id,
                                                           const GValue *value,
                                                           GParamSpec   *pspec);
static void     get_property                              (GObject    *obj,
                                                           guint       prop_id,
                                                           GValue     *value,
                                                           GParamSpec *pspec);

static void     async_initable_iface_init                 (GAsyncInitableIface *iface);

static void     init_async                                (GAsyncInitable      *initable,
                                                           int                  io_priority,
                                                           GCancellable        *cancellable,
                                                           GAsyncReadyCallback  callback,
                                                           gpointer             user_data);
static gboolean init_finish                               (GAsyncInitable      *initable,
                                                           GAsyncResult        *res,
                                                           GError             **error);

static gboolean load_env                                  (LiaApplication  *self,
                                                           gchar          **service_name,
                                                           GError         **error);

static void     register_objects                          (LiaApplication  *self,
                                                           LiaBusType       bus_type);

static void     on_bus_method_call                        (GDBusConnection       *connection,
                                                           const gchar           *sender,
                                                           const gchar           *object_path,
                                                           const gchar           *interface_name,
                                                           const gchar           *method_name,
                                                           GVariant              *parameters,
                                                           GDBusMethodInvocation *invocation,
                                                           gpointer               user_data);

G_DEFINE_TYPE_WITH_CODE (LiaApplication, lia_application, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                                async_initable_iface_init));

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = init_async;
  iface->init_finish = init_finish;
}

static void
lia_application_class_init (LiaApplicationClass *class)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS (class);

  obj_class->dispose = dispose;
  obj_class->finalize = finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  class->register_objects = register_objects;
  class->load_env = load_env;

  /* signals */
   lia_application_signals[SIGNAL_READY] =
     g_signal_new ("ready",
                   G_TYPE_FROM_CLASS (obj_class),
                   G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                   G_STRUCT_OFFSET (LiaApplicationClass, signal_ready),
                   NULL, NULL,
                   g_cclosure_marshal_VOID__BOXED,
                   G_TYPE_NONE, 0);

   lia_application_signals[SIGNAL_EXPORT_OBJECTS] =
     g_signal_new ("register-objects",
                   G_TYPE_FROM_CLASS (obj_class),
                   G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                   G_STRUCT_OFFSET (LiaApplicationClass, signal_register_objects),
                   NULL, NULL,
                   lia_marshal_VOID__OBJECT_UINT,
                   G_TYPE_NONE, 1,
                   G_TYPE_UINT);

  /* properties */
  g_object_class_install_property (obj_class,
                                   PROP_BASE_SERVICE_NAME,
                                   g_param_spec_string ("base-service-name",
                                                        "Base service name",
                                                        "Name of the base service this application will be running for",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class,
                                   PROP_SERVICE_NAME,
                                   g_param_spec_string ("service-name",
                                                        "Service name",
                                                        "Name of the D-Bus service owned by the application",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class,
                                   PROP_WEBVIEW_HTML_ROOT,
                                   g_param_spec_string ("webview-html-root",
                                                        "Webview HTML root",
                                                        "Path to directory on disk containing application's HTML files",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (obj_class, sizeof (LiaApplicationPrivate));
}

static void
lia_application_init (LiaApplication *self)
{
  LiaApplicationPrivate *priv;

  priv = LIA_APPLICATION_GET_PRIVATE (self);
  self->priv = priv;

  priv->base_service_name = NULL;

  priv->async_result = NULL;
  priv->initialized = FALSE;

  priv->reg_obj_vtable.method_call = on_bus_method_call;

  priv->daemon = evd_daemon_get_default (NULL, NULL);

  memset (priv->bus_addr, 0, 3);
  memset (priv->bus_conn, 0, 3);

  priv->webview_html_root = NULL;
}

static void
dispose (GObject *obj)
{
  LiaApplication *self = LIA_APPLICATION (obj);
  gint i;

  for (i=0; i<3; i++)
    if (self->priv->bus_conn[i] != NULL)
      {
        g_object_unref (self->priv->bus_conn[i]);
        self->priv->bus_conn[i] = NULL;
      }

  G_OBJECT_CLASS (lia_application_parent_class)->dispose (obj);
}

static void
finalize (GObject *obj)
{
  LiaApplication *self = LIA_APPLICATION (obj);
  gint i;

  g_object_unref (self->priv->daemon);

  for (i=0; i<3; i++)
    if (self->priv->bus_addr[i] != NULL)
      g_free (self->priv->bus_addr[i]);

  g_free (self->priv->base_service_name);
  g_free (self->priv->service_name);
  g_free (self->priv->webview_html_root);

  G_OBJECT_CLASS (lia_application_parent_class)->finalize (obj);

  g_print ("%s finalized\n", G_OBJECT_CLASS_NAME (LIA_APPLICATION_GET_CLASS (self)));
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  LiaApplication *self;

  self = LIA_APPLICATION (obj);

  switch (prop_id)
    {
    case PROP_BASE_SERVICE_NAME:
      self->priv->base_service_name = g_value_dup_string (value);
      break;

    case PROP_SERVICE_NAME:
      self->priv->service_name = g_value_dup_string (value);
      break;

    case PROP_WEBVIEW_HTML_ROOT:
      self->priv->webview_html_root = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
get_property (GObject    *obj,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
  LiaApplication *self;

  self = LIA_APPLICATION (obj);

  switch (prop_id)
    {
    case PROP_BASE_SERVICE_NAME:
      g_value_set_string (value, lia_application_get_base_service_name (self));
      break;

    case PROP_SERVICE_NAME:
      g_value_set_string (value, lia_application_get_service_name (self));
      break;

    case PROP_WEBVIEW_HTML_ROOT:
      g_value_set_string (value, self->priv->webview_html_root);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
abort_init_async (LiaApplication     *self,
                  GSimpleAsyncResult *res,
                  GError             *error)
{
  g_simple_async_result_set_from_error (res, error);
  g_error_free (error);

  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);
}

static void
finish_init_async (LiaApplication     *self,
                   GSimpleAsyncResult *res)
{
  LiaApplicationClass *class;

  class = LIA_APPLICATION_GET_CLASS (self);

  if (class->init_async_finished != NULL)
    {
      class->init_async_finished (self,
                                  G_ASYNC_RESULT (res),
                                  G_PRIORITY_DEFAULT,
                                  NULL);
    }
  else
    {
      g_simple_async_result_complete (res);
      g_object_unref (res);
    }
}

static void
on_webview_html_root_registered (GObject      *obj,
                                 GAsyncResult *res,
                                 gpointer      user_data)
{
  LiaApplication *self = LIA_APPLICATION (user_data);
  GError *error = NULL;
  GVariant *ret;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (obj),
                                       res,
                                       &error);
  if (ret == NULL)
    {
      g_print ("Error registering webview HTML root: %s\n", error->message);
      g_error_free (error);
    }
  else
    {
      g_variant_unref (ret);
    }

  g_object_unref (self);
}

static void
register_webview_html_root (LiaApplication *self, const gchar *bus_name)
{
  g_object_ref (self);
  g_dbus_connection_call (self->priv->bus_conn[LIA_BUS_PRIVATE],
                          bus_name,
                          LIA_WEBVIEW_OBJ_PATH,
                          LIA_WEBVIEW_IFACE_NAME,
                          "RegisterWebDir",
                          g_variant_new ("(ss)",
                                         self->priv->service_name,
                                         self->priv->webview_html_root),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          on_webview_html_root_registered,
                          self);
}

static void
webview_bus_name_appeared (GDBusConnection *connection,
                           const gchar     *name,
                           const gchar     *name_owner,
                           gpointer         user_data)
{
  LiaApplication *self = LIA_APPLICATION (user_data);

  /* @TODO: Webview service appeared, log it properly */
  g_debug ("Webview service '%s' appeared, register HTML root", name);
  register_webview_html_root (self, name);
}

static void
webview_bus_name_vanished (GDBusConnection *connection,
                           const gchar     *name,
                           gpointer         user_data)
{
  /* @TODO: Webview service vanished, log it properly */
  g_warning ("Webview service '%s' vanished!", name);
}

static void
on_service_name_acquired (GDBusConnection *connection,
                          const gchar     *name,
                          gpointer         user_data)
{
  LiaApplication *self = LIA_APPLICATION (user_data);

  if (self->priv->async_result != NULL)
    {
      self->priv->acquire_bus_name_ops--;

      if (self->priv->acquire_bus_name_ops == 0 &&
          self->priv->create_bus_conn_ops == 0)
        {
          /* watch Webview bus name to register app's HTML root */
          if (self->priv->webview_html_root != NULL &&
              self->priv->bus_conn[LIA_BUS_PRIVATE] != NULL)
            {
              gchar *bus_name;

              bus_name =
                g_strdup_printf ("%s." LIA_WEBVIEW_SERVICE_NAME_SUFFIX,
                                 self->priv->base_service_name);

              g_bus_watch_name_on_connection (self->priv->bus_conn[LIA_BUS_PRIVATE],
                                              bus_name,
                                              G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                              webview_bus_name_appeared,
                                              webview_bus_name_vanished,
                                              self,
                                              NULL);
              g_free (bus_name);
            }

          finish_init_async (self, self->priv->async_result);
          self->priv->async_result = NULL;
        }
    }
}

static void
on_service_name_lost (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
  LiaApplication *self = LIA_APPLICATION (user_data);

  if (self->priv->async_result != NULL)
    {
      self->priv->acquire_bus_name_ops--;

      g_simple_async_result_set_error (self->priv->async_result,
                                       G_IO_ERROR,
                                       G_IO_ERROR_DBUS_ERROR,
                                       "Failed to own service name");

      g_simple_async_result_complete (self->priv->async_result);

      g_object_unref (self->priv->async_result);
      self->priv->async_result = NULL;
    }
  else
    {
      /* @TODO: log properly */
      g_critical ("Application's service name '%s' lost",
                  self->priv->service_name);
      evd_daemon_quit (self->priv->daemon, -1);
    }
}

static void
register_objects (LiaApplication  *self, LiaBusType bus_type)
{
  g_signal_emit (self,
                 lia_application_signals[SIGNAL_EXPORT_OBJECTS],
                 0,
                 bus_type,
                 NULL);
}

static void
dbus_connection_closed (GDBusConnection *conn,
                        gboolean         remote_peer_vanished,
                        GError          *error,
                        gpointer         user_data)
{
  LiaApplication *self = LIA_APPLICATION (user_data);

  if (remote_peer_vanished)
    g_print ("Connection closed: %s!\n", error->message);

  /* @TODO: try to recover from a closed D-Bus connection! */

  /* by now just terminated the application */
  evd_daemon_quit (self->priv->daemon, -1);
}

static void
on_dbus_connection (GObject      *obj,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  BusConnData *data = user_data;
  GSimpleAsyncResult *result;
  LiaApplication *self;
  GError *error = NULL;

  result = data->result;
  self = LIA_APPLICATION (g_async_result_get_source_object (G_ASYNC_RESULT (result)));

  self->priv->create_bus_conn_ops--;

  self->priv->bus_conn[data->bus_type] =
    g_dbus_connection_new_for_address_finish (res, &error);
  if (self->priv->bus_conn[data->bus_type] == NULL)
    {
      g_print ("Error: %s\n", error->message);
      g_cancellable_cancel (data->cancellable);

      if (self->priv->create_bus_conn_ops == 0)
        {
          abort_init_async (self, result, error);
        }
      else
        {
          g_simple_async_result_set_from_error (result, error);
          g_error_free (error);
        }
    }
  else
    {
      LiaApplicationClass *class;

      g_signal_connect (self->priv->bus_conn[data->bus_type],
                        "closed",
                        G_CALLBACK (dbus_connection_closed),
                        self);

      /* register objects over this bus */
      class = LIA_APPLICATION_GET_CLASS (self);
      if (class->register_objects != NULL)
        class->register_objects (self, data->bus_type);

      if (self->priv->service_name != NULL)
        {
          self->priv->async_result = result;

          self->priv->service_name_owner_id[data->bus_type] =
            g_bus_own_name_on_connection (self->priv->bus_conn[data->bus_type],
                                          self->priv->service_name,
                                          G_BUS_NAME_OWNER_FLAGS_NONE,
                                          on_service_name_acquired,
                                          on_service_name_lost,
                                          self,
                                          g_object_unref);

          self->priv->acquire_bus_name_ops++;
        }

      if (self->priv->create_bus_conn_ops == 0 &&
          self->priv->acquire_bus_name_ops == 0)
        {
          /* or finish initialization */
          finish_init_async (self, result);
        }
    }

  g_slice_free (BusConnData, data);

  g_object_unref (result);
  g_object_unref (self);
}

static gboolean
resolve_base_service_name (LiaApplication *self, GError **error)
{
  if (self->priv->base_service_name == NULL)
    {
      const gchar *base_service_name;

      base_service_name = g_getenv (LIA_ENV_KEY_BASE_SERVICE_NAME);

      if (base_service_name == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Failed to resolve base service name");
          return FALSE;
        }
      else
        {
          self->priv->base_service_name = g_strdup (base_service_name);
        }
    }

  return TRUE;
}

static gboolean
setup_env_from_file (LiaApplication *self, GError **error)
{
  gchar *contents;
  gchar *filename;
  gboolean result = TRUE;
  gchar **entries = NULL;
  gint i;
  gchar **pair = NULL;

  filename = g_strdup_printf ("/tmp/%s.env", self->priv->base_service_name);

  if (! g_file_get_contents (filename,
                             &contents,
                             NULL,
                             error))
    {
      result = FALSE;
      goto out;
    }

  entries = g_strsplit (contents, "\n", 16);
  if (entries == NULL)
    goto out;

  i = 0;
  while (entries[i] != NULL)
    {
      if (entries[i][0] != '\0')
        {
          pair = g_strsplit (entries[i], "=", 2);

          if (pair == NULL || g_strv_length (pair) != 2)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Error in service environment file '%s', line %d",
                           filename, i + 1);
              result = FALSE;
              g_strfreev (pair);
              goto out;
            }
          else
            {
              g_setenv (pair[0], pair[1], TRUE);
            }

          g_strfreev (pair);
        }

      i++;
    }

 out:
  g_free (filename);
  g_strfreev (entries);
  g_free (contents);

  return result;
}

static gboolean
load_env (LiaApplication *self, gchar **service_name, GError **error)
{
  gboolean result = TRUE;
  const gchar *core_service_name;

  /* resolve core service name */
  core_service_name = g_getenv (LIA_ENV_KEY_CORE_SERVICE_NAME);
  if (core_service_name == NULL)
    {
      /* try loading env from file */
      if (! setup_env_from_file (self, error))
        return FALSE;
    }

  /* By default, an application read the bus addresses from
     environment variables */
  self->priv->bus_addr[LIA_BUS_PRIVATE] =
    g_strdup (g_getenv (LIA_ENV_KEY_PRIVATE_BUS_ADDR));

  self->priv->bus_addr[LIA_BUS_PROTECTED] =
    g_strdup (g_getenv (LIA_ENV_KEY_PROTECTED_BUS_ADDR));

  self->priv->bus_addr[LIA_BUS_PUBLIC] =
    g_strdup (g_getenv (LIA_ENV_KEY_PUBLIC_BUS_ADDR));

  return result;
}

static void
init_async (GAsyncInitable      *initable,
            int                  io_priority,
            GCancellable        *cancellable,
            GAsyncReadyCallback  callback,
            gpointer             user_data)
{
  LiaApplication *self = LIA_APPLICATION (initable);
  GError *error = NULL;
  GSimpleAsyncResult *res;
  gint i;

  res = g_simple_async_result_new (G_OBJECT (self),
                                   callback,
                                   user_data,
                                   init_async);

  /* resolve base service name */
  if (! resolve_base_service_name (self, &error))
    {
      abort_init_async (self, res, error);
      return;
    }

  /* load service environment */
  if (! LIA_APPLICATION_GET_CLASS (self)->load_env (self,
                                                    &self->priv->service_name,
                                                    &error))
    {
      abort_init_async (self, res, error);
      return;
    }

  self->priv->create_bus_conn_ops = 0;
  self->priv->acquire_bus_name_ops = 0;

  for (i=0; i<3; i++)
    if (self->priv->bus_addr[i] != NULL)
      {
        BusConnData *data;

        data = g_slice_new (BusConnData);
        data->result = res;
        g_object_ref (res);
        data->cancellable = cancellable;
        data->bus_type = i;
        g_dbus_connection_new_for_address (self->priv->bus_addr[i],
                                           G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                           G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                           NULL,
                                           cancellable,
                                           on_dbus_connection,
                                           data);
        self->priv->create_bus_conn_ops++;
      }
}

static gboolean
init_finish (GAsyncInitable  *initable,
             GAsyncResult    *res,
             GError         **error)
{
  g_return_val_if_fail (g_simple_async_result_is_valid (res,
                                                        G_OBJECT (initable),
                                                        init_async),
                        FALSE);

  return (! g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res),
                                                   error));
}

static void
on_initialized (GObject      *obj,
                GAsyncResult *res,
                gpointer      user_data)
{
  LiaApplication *self = LIA_APPLICATION (user_data);
  GError *error = NULL;

  if (! g_async_initable_init_finish (G_ASYNC_INITABLE (obj),
                                      res,
                                      &error))
    {
      /* @TODO: log error */
      g_warning ("ERROR: failed to initalize application: %s", error->message);
      g_error_free (error);

      //      evd_daemon_quit (self->priv->daemon, -1);
    }
  else
    {
      /* app initialized successfully */

      g_print ("'%s' application initialized successfully\n",
               self->priv->service_name);

      /* fire "ready" signal */
      g_signal_emit (self, lia_application_signals[SIGNAL_READY], 0, NULL);
    }
}

static void
free_registered_object_data (gpointer _data)
{
  RegObjData *data = _data;

  g_object_unref (data->self);

  if (data->user_data != NULL && data->user_data_free_func != NULL)
    data->user_data_free_func (data->user_data);

  g_slice_free (RegObjData, data);
}

static void
on_bus_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  RegObjData *data = user_data;

  /* @TODO: resolve caller_id from 'sender'. By now use 'sender' itself */

  g_debug ("method called %p", data->method_call_func);

  data->method_call_func (data->self,
                          data->bus_type,
                          sender,
                          object_path,
                          interface_name,
                          method_name,
                          parameters,
                          invocation,
                          data->user_data);
}

/* public methods */

gint
lia_application_run (LiaApplication *self)
{
  g_return_val_if_fail (LIA_IS_APPLICATION (self), -1);

  if (! self->priv->initialized)
    {
      self->priv->initialized = TRUE;

      g_async_initable_init_async (G_ASYNC_INITABLE (self),
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   on_initialized,
                                   self);
    }

  return evd_daemon_run (self->priv->daemon, NULL);
}

void
lia_application_quit (LiaApplication *self, gint exit_code)
{
  g_return_if_fail (LIA_IS_APPLICATION (self));

  evd_daemon_quit (self->priv->daemon, exit_code);
}

/**
 * lia_application_get_service_name:
 *
 * Returns: (transfer none):
 **/
const gchar *
lia_application_get_service_name (LiaApplication *self)
{
  g_return_val_if_fail (LIA_IS_APPLICATION (self), NULL);

  return self->priv->service_name;
}

/**
 * lia_application_get_core_service_name:
 *
 * Returns: (transfer none):
 **/
const gchar *
lia_application_get_core_service_name (LiaApplication *self)
{
  g_return_val_if_fail (LIA_IS_APPLICATION (self), NULL);

  return g_getenv (LIA_ENV_KEY_CORE_SERVICE_NAME);
}

/**
 * lia_application_get_base_service_name:
 *
 * Returns: (transfer none):
 **/
const gchar *
lia_application_get_base_service_name (LiaApplication *self)
{
  g_return_val_if_fail (LIA_IS_APPLICATION (self), NULL);

  return self->priv->base_service_name;
}

/**
 * lia_application_get_bus_address:
 *
 * Returns: (transfer none):
 **/
const gchar *
lia_application_get_bus_address (LiaApplication *self, LiaBusType bus_type)
{
  g_return_val_if_fail (LIA_IS_APPLICATION (self), NULL);
  g_return_val_if_fail (bus_type >= LIA_BUS_PRIVATE &&
                        bus_type <= LIA_BUS_PUBLIC, NULL);

  return self->priv->bus_addr[bus_type];
}

/**
 * lia_application_get_bus:
 *
 * Returns: (transfer none):
 **/
GDBusConnection *
lia_application_get_bus (LiaApplication *self, LiaBusType bus_type)
{
  g_return_val_if_fail (LIA_IS_APPLICATION (self), NULL);
  g_return_val_if_fail (bus_type >= LIA_BUS_PRIVATE &&
                        bus_type <= LIA_BUS_PUBLIC, NULL);

  return self->priv->bus_conn[bus_type];
}

/**
 * lia_application_register_object:
 * @method_call_func: (scope async) (closure user_data):
 * @user_data: (allow-none) (closure):
 * @user_data_free_func: (allow-none) (type any):
 *
 * Returns:
 **/
guint
lia_application_register_object (LiaApplication        *self,
                                 guint                  bus_type,
                                 const gchar           *object_path,
                                 const gchar           *interface_xml,
                                 LiaBusMethodCallFunc   method_call_func,
                                 gpointer               user_data,
                                 GDestroyNotify         user_data_free_func,
                                 GError               **error)
{
  static GDBusNodeInfo *introspection_data = NULL;
  gchar *introspection_xml;
  guint reg_id = 0;
  RegObjData *data = NULL;
  GDBusConnection *conn = NULL;

  g_return_val_if_fail (LIA_IS_APPLICATION (self), 0);
  g_return_val_if_fail (bus_type >= LIA_BUS_PRIVATE &&
                        bus_type <= LIA_BUS_PUBLIC, 0);
  g_return_val_if_fail (object_path != NULL, 0);
  g_return_val_if_fail (interface_xml != NULL, 0);
  g_return_val_if_fail (method_call_func != NULL, 0);

  introspection_xml = g_strdup_printf ("<node>%s</node>", interface_xml);
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, error);
  g_free (introspection_xml);

  if (introspection_data == NULL)
    return 0;

  conn = self->priv->bus_conn[bus_type];
  if (conn == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Bus type %d is not available for application",
                   bus_type);
      return 0;
    }

  data = g_slice_new0 (RegObjData);
  data->self = self;
  g_object_ref (self);
  data->user_data = user_data;
  data->user_data_free_func = user_data_free_func;
  data->method_call_func = method_call_func;
  data->bus_type = bus_type;

  reg_id = g_dbus_connection_register_object (conn,
                                              object_path,
                                              introspection_data->interfaces[0],
                                              &self->priv->reg_obj_vtable,
                                              data,
                                              free_registered_object_data,
                                              error);

  return reg_id;
}

gboolean
lia_application_unregister_object (LiaApplication *self,
                                   guint           bus_type,
                                   guint           registration_id)
{
  g_return_val_if_fail (LIA_IS_APPLICATION (self), FALSE);
  g_return_val_if_fail (registration_id > 0, FALSE);

  if (self->priv->bus_conn[bus_type] != NULL)
    {
      return g_dbus_connection_unregister_object (self->priv->bus_conn[bus_type],
                                                  registration_id);
    }
  else
    {
      return FALSE;
    }
}
