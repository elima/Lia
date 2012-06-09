/*
 * lia-webview.c
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

#include <string.h>
#include <evd.h>
#include <libsoup/soup.h>

#include "lia-webview.h"

#include "lia-defines.h"

G_DEFINE_TYPE (LiaWebview, lia_webview, LIA_TYPE_APPLICATION);

#define LIA_WEBVIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                      LIA_TYPE_WEBVIEW, \
                                      LiaWebviewPrivate))

#define DEFAULT_LISTEN_ADDR "0.0.0.0:8080"

#define DEFAULT_JQUERY_PATH "/usr/share/javascript/jquery/"

#define REST_ACTION_SIGNIN  "signin"
#define REST_ACTION_SIGNOUT "signout"
#define REST_ACTION_CONFIG  "webview-config"

#define SESSION_ID_COOKIE_NAME_SUFFIX LIA_WEBVIEW_SERVICE_NAME_SUFFIX ".SID"

#define TRANSPORT_BASE_PATH_SUFFIX "transport"

static const gchar *DEFAULT_WEB_DIRS[3] = {"private", "protected", "public"};

static const gchar introspection_xml[] =
  "  <interface name='" LIA_WEBVIEW_IFACE_NAME "'>"
  "    <method name='RegisterWebDir'>"
  "      <arg type='s' name='path' direction='in'/>"
  "      <arg type='s' name='dir' direction='in'/>"
  "    </method>"
  "  </interface>";

/* private data */
struct _LiaWebviewPrivate
{
  gchar *base_path;
  gchar *bus_addr_alias;
  gchar *transport_base_path;
  gchar *jquery_path;

  EvdDBusBridge *dbus_bridge;
  EvdWebService *web_service;

  EvdWebSelector *selectors[3];
  EvdWebDir *web_dirs[3];
  EvdWebTransportServer *transports[3];

  gchar *listen_addr;

  GHashTable *auth_sessions;
  gchar *sid_cookie_name;

  gchar *own_path_regexp;
  gchar *signin_path;
  gchar *signout_path;

  guint obj_reg_id;

  GList *app_web_dirs;
};

/* AuthData */
typedef struct
{
  const gchar *session_id;
  gchar *user_id;
  LiaBusType bus_type;
  gchar *auth_token;
  GDateTime *timestamp;
  guint expire_src_id;
} AuthData;

/* AppWebDir */
typedef struct
{
  gchar *path;
  EvdWebDir *web_dir;
  LiaBusType bus_type;
  gchar *owner_id;
} AppWebDir;

typedef struct
{
  LiaWebview *self;
  guint watcher_id;
} NameWatchData;

/* signals */
enum
{
  SIGNAL_EXAMPLE,
  LAST_SIGNAL
};

//static guint lia_webview_signals [LAST_SIGNAL] = { 0 };

/* properties */
enum
{
  PROP_0,
  PROP_EXAMPLE
};


static void     lia_webview_class_init                    (LiaWebviewClass *class);
static void     lia_webview_init                          (LiaWebview *self);

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

static void     init_async                                (LiaApplication *self,
                                                           GAsyncResult   *result,
                                                           gint            io_priority,
                                                           GCancellable   *cancellable);

static gboolean load_env                                  (LiaApplication  *self,
                                                           gchar          **service_name,
                                                           GError         **error);

static guint    transport_on_validate_peer                (EvdTransport *transport,
                                                           EvdPeer      *peer,
                                                           gpointer      user_data);

static void     register_objects                          (LiaApplication *app,
                                                           LiaBusType      bus_type);

static void     web_service_on_request                    (EvdWebService     *web_service,
                                                           EvdHttpConnection *conn,
                                                           EvdHttpRequest    *request,
                                                           gpointer           user_data);

static void     free_auth_session_data                    (gpointer _data);
static void     free_app_web_dir                          (gpointer _data);

static void
lia_webview_class_init (LiaWebviewClass *class)
{
  GObjectClass *obj_class;
  LiaApplicationClass *lia_app_class;

  obj_class = G_OBJECT_CLASS (class);
  obj_class->dispose = dispose;
  obj_class->finalize = finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  lia_app_class = LIA_APPLICATION_CLASS (class);
  lia_app_class->init_async_finished = init_async;
  lia_app_class->register_objects = register_objects;
  lia_app_class->load_env = load_env;

  /* signals */
  /*
  lia_webview_signals[SIGNAL_EXAMPLE] =
    g_signal_new ("signal-example",
          G_TYPE_FROM_CLASS (obj_class),
          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
          G_STRUCT_OFFSET (LiaWebviewClass, signal_example),
          NULL, NULL,
          g_cclosure_marshal_VOID__BOXED,
          G_TYPE_NONE, 1,
          G_TYPE_POINTER);
  */

  /* properties */
  g_object_class_install_property (obj_class,
                                   PROP_EXAMPLE,
                                   g_param_spec_int ("example",
                                   "An example property",
                                   "An example property to gobject boilerplate",
                                   0,
                                   256,
                                   0,
                                   G_PARAM_READWRITE));

  g_type_class_add_private (obj_class, sizeof (LiaWebviewPrivate));
}

static void
lia_webview_init (LiaWebview *self)
{
  LiaWebviewPrivate *priv;
  gint i;

  priv = LIA_WEBVIEW_GET_PRIVATE (self);
  self->priv = priv;

  priv->base_path = g_strdup ("/elima/");

  priv->jquery_path = g_strdup (DEFAULT_JQUERY_PATH);

  priv->signin_path = g_strdup_printf ("%s" REST_ACTION_SIGNIN,
                                       priv->base_path);
  priv->signout_path = g_strdup_printf ("%s" REST_ACTION_SIGNOUT,
                                        priv->base_path);

  priv->transport_base_path = g_strdup_printf ("%s" TRANSPORT_BASE_PATH_SUFFIX,
                                               priv->base_path);

  /* D-Bus bridge */
  priv->dbus_bridge = evd_dbus_bridge_new ();

  for (i=0; i<3; i++)
    {
      gchar *root_dir;

      /* web transport */
      priv->transports[i] =
        evd_web_transport_server_new (priv->transport_base_path);

      /*
        evd_web_transport_server_set_enable_websocket (priv->transports[i],
        FALSE);
      */
      g_signal_connect (priv->transports[i],
                        "validate-peer",
                        G_CALLBACK (transport_on_validate_peer),
                        self);

      evd_dbus_bridge_add_transport (priv->dbus_bridge,
                                     EVD_TRANSPORT (priv->transports[i]));

      /* web dir */
      priv->web_dirs[i] = evd_web_dir_new ();
      root_dir = g_strdup_printf (HTML_DATA_DIR "/%s", DEFAULT_WEB_DIRS[i]);
      evd_web_dir_set_root (priv->web_dirs[i], root_dir);
      g_free (root_dir);

      evd_web_dir_set_alias (priv->web_dirs[i], priv->base_path);

      /* web selector */
      priv->selectors[i] = evd_web_selector_new ();

      evd_web_selector_set_default_service (priv->selectors[i],
                                            EVD_SERVICE (priv->web_dirs[i]));

      evd_web_transport_server_set_selector (priv->transports[i],
                                             priv->selectors[i]);
    }

  /* web service */
  priv->web_service = evd_web_service_new ();
  g_signal_connect (priv->web_service,
                    "request-headers",
                    G_CALLBACK (web_service_on_request),
                    self);

  priv->listen_addr = g_strdup (DEFAULT_LISTEN_ADDR);

  /* auth sessions */
  priv->auth_sessions = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               free_auth_session_data);

  priv->sid_cookie_name = NULL;

  priv->obj_reg_id = 0;

  priv->app_web_dirs = NULL;
}

static void
dispose (GObject *obj)
{
  LiaWebview *self = LIA_WEBVIEW (obj);

  /* web service */
  if (self->priv->web_service != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->priv->web_service,
                                            G_CALLBACK (web_service_on_request),
                                            self);

      g_object_unref (self->priv->web_service);
      self->priv->web_service = NULL;
    }

  /* D-Bus bridge */
  if (self->priv->dbus_bridge != NULL)
    {
      g_object_unref (self->priv->dbus_bridge);
      self->priv->dbus_bridge = NULL;
    }

  if (self->priv->app_web_dirs != NULL)
    {
      g_list_free_full (self->priv->app_web_dirs, free_app_web_dir);
      self->priv->app_web_dirs = NULL;
    }

  G_OBJECT_CLASS (lia_webview_parent_class)->dispose (obj);
}

static void
finalize (GObject *obj)
{
  LiaWebview *self = LIA_WEBVIEW (obj);
  gint i;

  for (i=0; i<3; i++)
    {
      /* web transport */
      g_object_unref (self->priv->transports[i]);

      /* web dir */
      g_object_unref (self->priv->web_dirs[i]);

      /* web selector */
      g_object_unref (self->priv->selectors[i]);
    }

  g_free (self->priv->listen_addr);

  g_hash_table_unref (self->priv->auth_sessions);
  g_free (self->priv->sid_cookie_name);

  g_free (self->priv->own_path_regexp);
  g_free (self->priv->signin_path);
  g_free (self->priv->signout_path);

  g_free (self->priv->base_path);
  g_free (self->priv->bus_addr_alias);
  g_free (self->priv->transport_base_path);
  g_free (self->priv->jquery_path);

  G_OBJECT_CLASS (lia_webview_parent_class)->finalize (obj);
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  //  LiaWebview *self;

  //  self = LIA_WEBVIEW (obj);

  switch (prop_id)
    {
    case PROP_EXAMPLE:
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
  //  LiaWebview *self;

  //  self = LIA_WEBVIEW (obj);

  switch (prop_id)
    {
    case PROP_EXAMPLE:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
free_app_web_dir (gpointer _data)
{
  AppWebDir *data = _data;

  g_object_unref (data->web_dir);
  g_free (data->path);
  g_free (data->owner_id);

  g_slice_free (AppWebDir, data);
}

static gboolean
register_web_dir (LiaWebview   *self,
                  const gchar  *path,
                  const gchar  *dir,
                  const gchar  *owner_id,
                  GError      **error)
{
  gint i;
  gchar *root, *url_path;
  EvdWebDir *web_dir;

  /* @TODO: check for existence first */

  for (i=0; i<4; i++)
    {
      AppWebDir *app_web_dir;

      url_path = g_strdup_printf ("%s%s/%s",
                                  self->priv->base_path,
                                  path,
                                  i < 3 ? DEFAULT_WEB_DIRS[i] : "");

      root = g_strdup_printf (dir[strlen (dir)-1] == '/'? "%s%s/":"%s/%s/",
                              dir,
                              i < 3 ? DEFAULT_WEB_DIRS[i] : DEFAULT_WEB_DIRS[LIA_BUS_PUBLIC]);

      web_dir = evd_web_dir_new ();
      evd_web_dir_set_root (web_dir, root);
      evd_web_dir_set_alias (web_dir, url_path);
      g_print ("%s -> %s\n", url_path, root);
      g_free (root);

      app_web_dir = g_slice_new0 (AppWebDir);
      app_web_dir->path = g_strdup (url_path);
      app_web_dir->web_dir = web_dir;
      app_web_dir->bus_type = i;
      app_web_dir->owner_id = g_strdup (owner_id);

      self->priv->app_web_dirs = g_list_append (self->priv->app_web_dirs,
                                                app_web_dir);

      g_free (url_path);
    }

  return TRUE;
}

static void
unregister_web_dir_by_owner_id (LiaWebview *self, const gchar *owner_id)
{
  GList *node;

  node = self->priv->app_web_dirs;
  while (node != NULL)
    {
      AppWebDir *app_web_dir;

      app_web_dir = node->data;

      if (g_strcmp0 (app_web_dir->owner_id, owner_id) == 0)
        {
          GList *tmp;

          g_print ("Remove app web dir: %s\n", app_web_dir->path);

          tmp = node;
          node = node->next;

          if (tmp->prev == NULL)
            self->priv->app_web_dirs = tmp->next;
          else
            tmp->prev->next = tmp->next;

          if (tmp->next != NULL)
            tmp->next->prev = tmp->prev;

          free_app_web_dir (app_web_dir);
          g_list_free_1 (tmp);
        }
      else
        {
          node = node->next;
        }
    }
}

static void
free_name_watch_data (gpointer _data)
{
  NameWatchData *data = _data;

  g_object_unref (data->self);
  g_slice_free (NameWatchData, data);
}

static void
bus_name_vanished (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
  NameWatchData *data = user_data;

  /* remove all application web dirs associated with 'name' */
  unregister_web_dir_by_owner_id (data->self, name);

  g_bus_unwatch_name (data->watcher_id);
}

static void
on_bus_method_called (LiaApplication        *app,
                      LiaBusType             bus_type,
                      const gchar           *caller_id,
                      const gchar           *object_path,
                      const gchar           *interface_name,
                      const gchar           *method_name,
                      GVariant              *arguments,
                      GDBusMethodInvocation *invocation,
                      gpointer               user_data)
{
  LiaWebview *self = LIA_WEBVIEW (user_data);
  GError *error = NULL;

  /* RegisterWebDir */
  if (g_strcmp0 (method_name, "RegisterWebDir") == 0)
    {
      gchar *path, *dir;

      g_variant_get (arguments, "(ss)", &path, &dir);

      if (! register_web_dir (self, path, dir, caller_id, &error))
        {
          g_dbus_method_invocation_take_error (invocation, error);
        }
      else
        {
          GDBusConnection *bus_conn;
          NameWatchData *data;

          g_dbus_method_invocation_return_value (invocation,
                                                 g_variant_new ("()"));

          /* watch this name to cleanup registration when vanishes */
          bus_conn = lia_application_get_bus (app, bus_type);

          data = g_slice_new0 (NameWatchData);
          data->self = self;
          g_object_ref (self);

          data->watcher_id =
            g_bus_watch_name_on_connection (bus_conn,
                                            caller_id,
                                            G_BUS_NAME_WATCHER_FLAGS_NONE,
                                            NULL,
                                            bus_name_vanished,
                                            data,
                                            free_name_watch_data);
        }

      g_free (dir);
      g_free (path);
    }
}

static void
register_objects (LiaApplication *app, LiaBusType bus_type)
{
  if (bus_type == LIA_BUS_PRIVATE)
    {
      LiaWebview *self = LIA_WEBVIEW (app);
      GError *error = NULL;
      guint reg_id = 0;

      reg_id = lia_application_register_object (app,
                                                bus_type,
                                                LIA_WEBVIEW_OBJ_PATH,
                                                introspection_xml,
                                                on_bus_method_called,
                                                app,
                                                g_object_unref,
                                                &error);
      if (reg_id <= 0)
        {
          g_print ("Fatal error registering Webview object: %s\n", error->message);
          g_error_free (error);

          lia_application_quit (app, -1);
        }
      else
        {
          self->priv->obj_reg_id = reg_id;
        }
    }

  LIA_APPLICATION_CLASS (lia_webview_parent_class)->
    register_objects (app, bus_type);
}

static AuthData *
get_auth_data_from_request (LiaWebview *self, EvdHttpRequest *request)
{
  gchar *session_id;
  AuthData *auth_data = NULL;

  session_id = evd_http_request_get_cookie_value (request, self->priv->sid_cookie_name);
  if (session_id != NULL)
    {
      auth_data = g_hash_table_lookup (self->priv->auth_sessions, session_id);

      g_free (session_id);
    }

  return auth_data;
}

static guint
transport_on_validate_peer (EvdTransport *transport,
                            EvdPeer      *peer,
                            gpointer      user_data)
{
  LiaWebview *self = LIA_WEBVIEW (user_data);
  const gchar *bus_addr;
  EvdHttpConnection *conn;
  EvdHttpRequest *request;
  LiaBusType bus_type = LIA_BUS_PUBLIC;
  AuthData *auth_data;

  evd_web_transport_server_get_validate_peer_arguments (
                                           EVD_WEB_TRANSPORT_SERVER (transport),
                                           peer,
                                           &conn,
                                           &request);

  /* resolve what bus this peer can connect to */
  auth_data = get_auth_data_from_request (self, request);
  if (auth_data != NULL)
    {
      bus_type = auth_data->bus_type;
    }
  else
    {
      /* @TODO: use AUTH_TOKEN cookie to verify credentials and re-auth */
    }

  bus_addr = lia_application_get_bus_address (LIA_APPLICATION (self), bus_type);

  evd_dbus_agent_create_address_alias (G_OBJECT (peer),
                                       bus_addr,
                                       self->priv->bus_addr_alias);

  return EVD_VALIDATE_ACCEPT;
}

static void
web_service_on_listen (GObject      *obj,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error = NULL;

  if (! evd_service_listen_finish (EVD_SERVICE (obj), res, &error))
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
    }

  g_simple_async_result_complete (G_SIMPLE_ASYNC_RESULT (result));
  g_object_unref (result);
}

static void
on_tls_credentials_ready (GObject      *obj,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GAsyncResult *result = G_ASYNC_RESULT (user_data);
  LiaWebview *self;
  GError *error = NULL;
  EvdTlsCredentials *creds;

  self = LIA_WEBVIEW (g_async_result_get_source_object (result));
  creds = EVD_TLS_CREDENTIALS (obj);

  if (! evd_tls_credentials_add_certificate_from_file_finish (creds,
                                                              res,
                                                              &error))
    {
      g_simple_async_result_set_from_error (G_SIMPLE_ASYNC_RESULT (result),
                                            error);
      g_error_free (error);

      g_simple_async_result_complete (G_SIMPLE_ASYNC_RESULT (result));
      g_object_unref (result);
    }
  else
    {
      evd_service_listen (EVD_SERVICE (self->priv->web_service),
                          self->priv->listen_addr,
                          NULL,
                          web_service_on_listen,
                          result);
    }

  g_object_unref (self);
}

static gboolean
load_env (LiaApplication *app, gchar **service_name, GError **error)
{
  const gchar *base_service_name;
  const gchar *webview_service_name;

  if (! LIA_APPLICATION_CLASS (lia_webview_parent_class)->load_env (app,
                                                                   service_name,
                                                                   error))
    {
      return FALSE;
    }

  if (*service_name != NULL)
    g_free (*service_name);

  webview_service_name = g_getenv (LIA_ENV_KEY_WEBVIEW_SERVICE_NAME);
  if (webview_service_name == NULL)
    {
      base_service_name = lia_application_get_base_service_name (app);

      *service_name = g_strdup_printf ("%s.%s",
                                       base_service_name,
                                       LIA_WEBVIEW_SERVICE_NAME_SUFFIX);
    }
  else
    {
      *service_name = g_strdup (webview_service_name);
    }

  return TRUE;
}

static void
setup_jquery_web_dir (LiaWebview *self)
{
  gchar *jquery_web_alias;
  EvdWebDir *jquery_web_dir;
  gint i;

  jquery_web_dir = evd_web_dir_new ();
  evd_web_dir_set_root (jquery_web_dir, self->priv->jquery_path);

  jquery_web_alias = g_strdup_printf ("%sjquery/", self->priv->base_path);
  evd_web_dir_set_alias (jquery_web_dir, jquery_web_alias);

  for (i=0; i<3; i++)
    {
      evd_web_selector_add_service (self->priv->selectors[i],
                                    NULL,
                                    jquery_web_alias,
                                    EVD_SERVICE (jquery_web_dir),
                                    NULL);
    }

  g_free (jquery_web_alias);
  g_object_unref (jquery_web_dir);
}

static void
init_async (LiaApplication *lia_app,
            GAsyncResult   *result,
            gint            io_priority,
            GCancellable   *cancellable)
{
  LiaWebview *self = LIA_WEBVIEW (lia_app);
  EvdTlsCredentials *creds;

  self->priv->sid_cookie_name =
    g_strdup_printf ("%s.%s",
                     lia_application_get_base_service_name (lia_app),
                     SESSION_ID_COOKIE_NAME_SUFFIX);

  creds = evd_service_get_tls_credentials (EVD_SERVICE (self->priv->web_service));
  evd_tls_credentials_add_certificate_from_file (creds,
                                                 "/home/elima/projects/lia/ssl-cert-snakeoil.pem",
                                                 "/home/elima/projects/lia/ssl-cert-snakeoil.key",
                                                 cancellable,
                                                 on_tls_credentials_ready,
                                                 result);
  evd_service_set_tls_autostart (EVD_SERVICE (self->priv->web_service), TRUE);

  self->priv->own_path_regexp = g_strdup_printf ("%s(.+/|)%s/",
                                                 self->priv->base_path,
                                                 LIA_BASE_IFACE_NAME);

  self->priv->bus_addr_alias =
    g_strdup_printf ("unix:abstract=/%s/lia/bus",
                     lia_application_get_base_service_name (lia_app));

  setup_jquery_web_dir (self);

  /* register Webview's own web dirs */
  register_web_dir (self, LIA_BASE_IFACE_NAME, HTML_DATA_DIR, NULL, NULL);
}

#include "lia-webview-login.c"

static AppWebDir *
lookup_app_web_dir (LiaWebview *self, const gchar *path)
{
  GList *node;

  node = self->priv->app_web_dirs;
  while (node != NULL)
    {
      AppWebDir *app_web_dir;

      app_web_dir = node->data;

      if (g_strstr_len (path,
                        strlen (app_web_dir->path),
                        app_web_dir->path) == path)
        {
          return app_web_dir;
        }

      node = node->next;
    }

  return NULL;
}

static void
handle_webview_config_response (LiaWebview        *self,
                                EvdHttpConnection *conn,
                                EvdHttpRequest    *request,
                                LiaBusType         bus_type)
{
  gchar *config_json;

  config_json =
    g_strdup_printf ("{\"base-path\": \"%s\","
                "\"bus-address\": \"%s\","
                "\"transport-base-path\": \"%s\","
                "\"base-service-name\": \"%s\","
                "\"bus-type\": %d"
                "}",
                self->priv->base_path,
                self->priv->bus_addr_alias,
                self->priv->transport_base_path,
                lia_application_get_base_service_name (LIA_APPLICATION (self)),
                bus_type);

  evd_web_service_respond (self->priv->web_service,
                           conn,
                           SOUP_STATUS_OK,
                           NULL,
                           config_json,
                           strlen (config_json),
                           NULL);

  g_free (config_json);
}

static void
web_service_on_request (EvdWebService     *web_service,
                        EvdHttpConnection *conn,
                        EvdHttpRequest    *request,
                        gpointer           user_data)
{
  LiaWebview *self = LIA_WEBVIEW (user_data);
  SoupURI *uri;
  AuthData *auth_data;
  LiaBusType bus_type = LIA_BUS_PUBLIC;
  AppWebDir *app_web_dir;

  /* resolve auth data for this user-agent */
  auth_data = get_auth_data_from_request (self, request);

  if (auth_data != NULL)
    bus_type = auth_data->bus_type;

  uri = evd_http_request_get_uri (request);

  /* resolve requested resource */

  /* request to Webview itself? */
  if (g_regex_match_simple (self->priv->own_path_regexp, uri->path, 0, 0))
    {
      gchar *offset;

      offset = g_strrstr (uri->path, "/" LIA_BASE_IFACE_NAME "/");
      offset += strlen ("/" LIA_BASE_IFACE_NAME "/");

      if (g_strcmp0 (offset, REST_ACTION_CONFIG) == 0)
        {
          handle_webview_config_response (self, conn, request, bus_type);
          return;
        }
      else
        {
          gchar *new_path;

          new_path = g_strdup_printf ("%s" LIA_BASE_IFACE_NAME "/%s",
                                      self->priv->base_path,
                                      offset);

          soup_uri_set_path (uri, new_path);

          g_free (new_path);
        }
    }
  else
  /* sign-in? */
  if (g_strcmp0 (uri->path, self->priv->signin_path) == 0)
    {
      handle_signin_request (self, conn, request, auth_data);
      return;
    }
  /* sign-out? */
  else if (g_strcmp0 (uri->path, self->priv->signout_path) == 0)
    {
      handle_signout_request (self, conn, request, auth_data);
      return;
    }

  /* all other requests */
  app_web_dir = lookup_app_web_dir (self, uri->path);
  if (app_web_dir != NULL)
    {
      if (bus_type <= app_web_dir->bus_type)
        {
          evd_web_service_add_connection_with_request (
                                         EVD_WEB_SERVICE (app_web_dir->web_dir),
                                         conn,
                                         request,
                                         EVD_SERVICE (web_service));
        }
      else
        {
          /* @TODO: respond with a nice 'forbidden' page */
          evd_web_service_respond (self->priv->web_service,
                                   conn,
                                   SOUP_STATUS_FORBIDDEN,
                                   NULL,
                                   NULL,
                                   0,
                                   NULL);
        }
    }
  else
    {
      evd_web_service_add_connection_with_request (
                              EVD_WEB_SERVICE (self->priv->selectors[bus_type]),
                              conn,
                              request,
                              EVD_SERVICE (web_service));
    }
}
