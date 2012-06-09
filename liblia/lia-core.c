/*
 * lia-core.c
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

#include "lia-core.h"

#include "lia-auth-service.h"

#define LIA_CORE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),  \
                                   LIA_TYPE_CORE, \
                                   LiaCorePrivate))

/* private data */
struct _LiaCorePrivate
{
  LiaAuthService *auth_service;

  EvdDBusDaemon *priv_bus_daemon;
  EvdDBusDaemon *prot_bus_daemon;
  EvdDBusDaemon *pub_bus_daemon;

  gchar **launch_env;
  guint launch_env_count;
};

static void     lia_core_class_init                (LiaCoreClass *class);
static void     lia_core_init                      (LiaCore *self);

static void     finalize                           (GObject *obj);
static void     dispose                            (GObject *obj);

static void     init_async                         (LiaApplication *lia_app,
                                                    GAsyncResult   *result,
                                                    gint            io_priority,
                                                    GCancellable   *cancellable);

static gboolean load_env                           (LiaApplication  *self,
                                                    gchar          **service_name,
                                                    GError         **error);

G_DEFINE_TYPE (LiaCore, lia_core, LIA_TYPE_APPLICATION);

static void
lia_core_class_init (LiaCoreClass *class)
{
  GObjectClass *obj_class;
  LiaApplicationClass *lia_app_class;

  obj_class = G_OBJECT_CLASS (class);
  obj_class->dispose = dispose;
  obj_class->finalize = finalize;

  lia_app_class = LIA_APPLICATION_CLASS (class);
  lia_app_class->init_async_finished = init_async;
  lia_app_class->load_env = load_env;

  g_type_class_add_private (obj_class, sizeof (LiaCorePrivate));
}

static void
lia_core_init (LiaCore *self)
{
  LiaCorePrivate *priv;

  priv = LIA_CORE_GET_PRIVATE (self);
  self->priv = priv;

  priv->auth_service = NULL;

  priv->priv_bus_daemon = NULL;
  priv->prot_bus_daemon = NULL;
  priv->pub_bus_daemon = NULL;

  priv->launch_env = g_new0 (char *, 16);
  priv->launch_env_count = 0;
}

static void
dispose (GObject *obj)
{
  LiaCore *self = LIA_CORE (obj);

  if (self->priv->auth_service != NULL)
    {
      g_object_unref (self->priv->auth_service);
      self->priv->auth_service = NULL;
    }

  G_OBJECT_CLASS (lia_core_parent_class)->dispose (obj);
}

static void
finalize (GObject *obj)
{
  LiaCore *self = LIA_CORE (obj);

  if (self->priv->priv_bus_daemon != NULL)
    g_object_unref (self->priv->priv_bus_daemon);

  if (self->priv->prot_bus_daemon != NULL)
    g_object_unref (self->priv->prot_bus_daemon);

  if (self->priv->pub_bus_daemon != NULL)
    g_object_unref (self->priv->pub_bus_daemon);

  g_strfreev (self->priv->launch_env);

  G_OBJECT_CLASS (lia_core_parent_class)->finalize (obj);
}

static void
abort_init_async (LiaCore            *self,
                  GSimpleAsyncResult *res,
                  GError             *error)
{
  g_simple_async_result_set_from_error (res, error);
  g_error_free (error);

  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);

  g_object_unref (self);
}

static gchar **
build_launch_env (LiaCore *self, gchar **env)
{
  gchar **launch_env = NULL;
  gint i, c;
  guint launch_env_len = 0;

  launch_env_len = self->priv->launch_env_count;

  if (env != NULL)
    launch_env_len += g_strv_length (env);

  launch_env = g_new0 (gchar *, launch_env_len + 1);

  c = 0;

  if (env != NULL)
    {
      i = 0;
      while (env[i] != NULL)
        {
          launch_env[c] = g_strdup (env[i]);
          c++;
          i++;
        }
    }

  i = 0;
  while (self->priv->launch_env[i] != NULL)
    {
      launch_env[c] = g_strdup (self->priv->launch_env[i]);
      c++;
      i++;
    }

  return launch_env;
}

static void
launch_env_add_entry (LiaCore *self, const gchar *key, const gchar *value)
{
  self->priv->launch_env[self->priv->launch_env_count] =
    g_strdup_printf ("%s=%s", key, value);

  self->priv->launch_env_count++;
}

static gboolean
write_env_file (LiaCore *self, GError **error)
{
  gchar **env;
  gchar *content;
  gboolean result = TRUE;
  gchar *filename;

  env = build_launch_env (self, NULL);

  content = g_strjoinv ("\n", env);

  filename =
    g_strdup_printf ("/tmp/%s.env",
                     lia_application_get_base_service_name (LIA_APPLICATION (self)));

  if (! g_file_set_contents (filename,
                             content,
                             -1,
                             error))
    {
      result = FALSE;
    }

  g_free (filename);
  g_free (content);
  g_strfreev (env);

  return result;
}

static void
on_auth_service_created (GObject      *obj,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);
  LiaCore *self;
  GError *error = NULL;

  self = LIA_CORE (g_async_result_get_source_object (G_ASYNC_RESULT (result)));

  self->priv->auth_service =
    LIA_AUTH_SERVICE (g_async_initable_new_finish (G_ASYNC_INITABLE (obj),
                                                   res,
                                                   &error));
  if (self->priv->auth_service == NULL)
    {
      abort_init_async (self, result, error);
    }
  else
    {
      /* launch webview service */
      if (! lia_core_launch (self,
                             SYS_PROG_DIR "/lia-webview",
                             NULL,
                             &error))
        {
          abort_init_async (self, result, error);
        }
      else
        {
          g_simple_async_result_complete (result);
          g_object_unref (result);
          g_object_unref (self);
        }
    }

  g_object_unref (self);
}

static gboolean
load_env (LiaApplication *app, gchar **service_name, GError **error)
{
  LiaCore *self = LIA_CORE (app);
  const gchar *base_service_name;
  gchar *webview_service_name;
  gchar *priv_bus_addr;
  gchar *prot_bus_addr;
  gchar *pub_bus_addr;

  base_service_name = lia_application_get_base_service_name (app);
  launch_env_add_entry (self, LIA_ENV_KEY_BASE_SERVICE_NAME, base_service_name);

  if (*service_name != NULL)
    g_free (*service_name);

  *service_name = g_strdup_printf ("%s.%s",
                                   base_service_name,
                                   LIA_CORE_SERVICE_NAME_SUFFIX);
  g_setenv (LIA_ENV_KEY_CORE_SERVICE_NAME, *service_name, TRUE);
  launch_env_add_entry (self, LIA_ENV_KEY_CORE_SERVICE_NAME, *service_name);

  webview_service_name = g_strdup_printf ("%s.%s",
                                          base_service_name,
                                          LIA_WEBVIEW_SERVICE_NAME_SUFFIX);
  g_setenv (LIA_ENV_KEY_WEBVIEW_SERVICE_NAME, webview_service_name, TRUE);
  launch_env_add_entry (self,
                        LIA_ENV_KEY_WEBVIEW_SERVICE_NAME,
                        webview_service_name);
  g_free (webview_service_name);

  /* private bus */
  /* @TODO: by now lets just use user's session bus */
  priv_bus_addr =
    g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION,
                                     NULL,
                                     error);
  if (priv_bus_addr == NULL)
    return FALSE;

  g_setenv (LIA_ENV_KEY_PRIVATE_BUS_ADDR, priv_bus_addr, TRUE);
  launch_env_add_entry (self, LIA_ENV_KEY_PRIVATE_BUS_ADDR, priv_bus_addr);

  g_print ("Private bus address: %s\n", priv_bus_addr);
  g_free (priv_bus_addr);

  /* protected bus */
  self->priv->prot_bus_daemon =
    evd_dbus_daemon_new (SYS_CONF_DIR "/dbus-daemon-prot.conf", error);
  if (self->priv->prot_bus_daemon == NULL)
    return FALSE;
  else
    g_object_get (self->priv->prot_bus_daemon,
                  "address", &prot_bus_addr,
                  NULL);

  g_setenv (LIA_ENV_KEY_PROTECTED_BUS_ADDR, prot_bus_addr, TRUE);
  launch_env_add_entry (self, LIA_ENV_KEY_PROTECTED_BUS_ADDR, prot_bus_addr);

  g_print ("Protected bus address: %s\n", prot_bus_addr);
  g_free (prot_bus_addr);

  /* public bus */
  self->priv->pub_bus_daemon =
    evd_dbus_daemon_new (SYS_CONF_DIR "/dbus-daemon-pub.conf", error);
  if (self->priv->pub_bus_daemon == NULL)
    return FALSE;
  else
    g_object_get (self->priv->pub_bus_daemon,
                  "address", &pub_bus_addr,
                  NULL);

  g_setenv (LIA_ENV_KEY_PUBLIC_BUS_ADDR, pub_bus_addr, TRUE);
  launch_env_add_entry (self, LIA_ENV_KEY_PUBLIC_BUS_ADDR, pub_bus_addr);

  g_print ("Public bus address: %s\n", pub_bus_addr);
  g_free (pub_bus_addr);

  if (! write_env_file (self, error))
    return FALSE;

  return LIA_APPLICATION_CLASS (lia_core_parent_class)->load_env (app,
                                                                  service_name,
                                                                  error);
}

static void
init_async (LiaApplication *lia_app,
            GAsyncResult   *result,
            gint            io_priority,
            GCancellable   *cancellable)
{
  GDBusConnection *priv_bus_conn;

  priv_bus_conn = lia_application_get_bus (lia_app, LIA_BUS_PRIVATE);

  /* create auth service object */
  g_object_ref (lia_app);
  g_async_initable_new_async (LIA_TYPE_AUTH_SERVICE,
                              G_PRIORITY_DEFAULT,
                              NULL,
                              on_auth_service_created,
                              result,
                              "dbus-connection", priv_bus_conn,
                              NULL);
}

/* public methods */

gboolean
lia_core_launch (LiaCore      *self,
                 const gchar  *command_line,
                 gchar       **env,
                 GError      **error)
{
  gboolean result = TRUE;
  gint argc;
  gchar **argv;
  GPid pid;
  gchar **launch_env = NULL;

  g_return_val_if_fail (LIA_IS_CORE (self), FALSE);

  if (! g_shell_parse_argv (command_line, &argc, &argv, error))
    return FALSE;

  launch_env = build_launch_env (self, env);

  if (! g_spawn_async (NULL,
                       argv,
                       launch_env,
                       G_SPAWN_SEARCH_PATH,
                       NULL,
                       NULL,
                       &pid,
                       error))
    {
      g_print ("Failed to launch '%s': %s\n", command_line, (*error)->message);
      result = FALSE;
    }
  else
    {
      g_print ("Launched: '%s' with PID: %d\n", command_line, pid);
    }

  g_strfreev (argv);
  g_strfreev (launch_env);

  return result;
}
