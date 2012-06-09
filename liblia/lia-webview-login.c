typedef struct
{
  LiaWebview *self;
  EvdHttpConnection *conn;
  EvdHttpRequest *request;
} LoginData;

static void
free_login_data (gpointer _data)
{
  LoginData *data = _data;

  g_object_unref (data->self);
  g_object_unref (data->conn);
  g_object_unref (data->request);

  g_slice_free (LoginData, data);
}

static void
respond_login_failed (LoginData *data, GError *error)
{
  evd_web_service_respond (data->self->priv->web_service,
                           data->conn,
                           SOUP_STATUS_UNAUTHORIZED,
                           NULL,
                           error->message,
                           strlen (error->message),
                           NULL);
}

static const gchar *
new_auth_session_data (LiaWebview  *self,
                       gchar       *user_id,
                       LiaBusType   bus_type,
                       gchar       *auth_token)
{
  AuthData *data;
  gchar *session_id;

  data = g_slice_new0 (AuthData);
  data->user_id = user_id;
  data->bus_type = bus_type;
  data->auth_token = auth_token;

  session_id = evd_uuid_new ();
  data->session_id = session_id;

  g_hash_table_insert (self->priv->auth_sessions, session_id, data);

  return session_id;
}

static void
free_auth_session_data (gpointer _data)
{
  AuthData *data = _data;

  g_free (data->user_id);
  g_free (data->auth_token);

  g_slice_free (AuthData, data);
}

static void
on_auth_response (GObject      *obj,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  LoginData *data = user_data;
  GError *error = NULL;
  GVariant *result;

  gchar *user_id;
  gint16 _bus_type;
  LiaBusType bus_type;
  gchar *auth_token;
  const gchar *session_id;

  SoupMessageHeaders *headers;
  gchar *cookie;

  result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (obj),
                                          res,
                                          &error);
  if (! result)
    {
      /* @TODO: Authentication failed */
      g_debug ("Error, authentication failed: %s", error->message);
      respond_login_failed (data, error);

      g_error_free (error);

      goto out;
    }

  g_variant_get (result,
                 "(ssn)",
                 &user_id,
                 &auth_token,
                 &_bus_type);

  g_variant_unref (result);

  /* save session! */
  bus_type = (LiaBusType) _bus_type;
  session_id = new_auth_session_data (data->self, user_id, bus_type, auth_token);

  headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);

  cookie = g_strdup_printf ("%s=%s; path=%s; Secure; HttpOnly",
                            data->self->priv->sid_cookie_name,
                            session_id,
                            data->self->priv->base_path);

  soup_message_headers_append (headers, "Set-Cookie", cookie);
  g_free (cookie);

  evd_web_service_respond (data->self->priv->web_service,
                           data->conn,
                           SOUP_STATUS_OK,
                           headers,
                           NULL,
                           0,
                           NULL);

  soup_message_headers_free (headers);

 out:
  free_login_data (data);
}

static void
on_login_conn_closed (EvdConnection *conn, gpointer user_data)
{
  GCancellable *cancellable = G_CANCELLABLE (user_data);

  g_cancellable_cancel (cancellable);
  g_object_unref (cancellable);
}

static void
on_login_content_read (GObject      *obj,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  LoginData *data = user_data;

  LiaWebview *self;
  EvdHttpConnection *conn;
  gchar *content;
  gssize size;
  GError *error = NULL;
  GHashTable *params;
  GCancellable *cancellable;
  SoupURI *uri;
  GDBusConnection *bus_conn;
  const gchar *core_service_name;

  const gchar *user;
  const gchar *passw;
  gchar *domain;

  self = data->self;
  conn = data->conn;

  content = evd_http_connection_read_all_content_finish (conn,
                                                         res,
                                                         &size,
                                                         &error);
  if (content == NULL)
    {
      /* @TODO: Failed reading login data */
      g_debug ("Error reading login daa: %s", error->message);
      g_error_free (error);
      return;
    }

  params = soup_form_decode (content);
  g_free (content);

  /* call authenticate method of AuthService interface */

  user = (const gchar *) g_hash_table_lookup (params, "user");
  passw = (const gchar *) g_hash_table_lookup (params, "passw");

  cancellable = g_cancellable_new ();
  g_signal_connect (conn,
                    "close",
                    G_CALLBACK (on_login_conn_closed),
                    cancellable);

  uri = evd_http_request_get_uri (data->request);
  domain = g_strdup_printf ("%s:%d", uri->host, uri->port);

  bus_conn = lia_application_get_bus (LIA_APPLICATION (self), LIA_BUS_PRIVATE);
  core_service_name =
    lia_application_get_core_service_name (LIA_APPLICATION (self));

  g_dbus_connection_call (bus_conn,
                          core_service_name,
                          LIA_AUTH_SERVICE_OBJ_PATH,
                          LIA_AUTH_SERVICE_IFACE_NAME,
                          "Authenticate",
                          g_variant_new ("(sss)",
                                         user,
                                         passw,
                                         domain),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          30000,
                          cancellable,
                          on_auth_response,
                          data);

  g_free (domain);
  g_hash_table_unref (params);
}

static void
handle_signin_request (LiaWebview        *self,
                       EvdHttpConnection *conn,
                       EvdHttpRequest    *request,
                       AuthData          *current_auth_data)
{
  LoginData *data;

  if (current_auth_data != NULL)
    g_hash_table_remove (self->priv->auth_sessions,
                         current_auth_data->session_id);

  data = g_slice_new (LoginData);

  data->self = self;
  g_object_ref (self);

  data->conn = conn;
  g_object_ref (conn);

  data->request = request;
  g_object_ref (request);

  evd_http_connection_read_all_content (conn,
                                        NULL,
                                        on_login_content_read,
                                        data);
}

static void
handle_signout_request (LiaWebview        *self,
                        EvdHttpConnection *conn,
                        EvdHttpRequest    *request,
                        AuthData          *auth_data)

{
  SoupMessageHeaders *headers;
  gchar *cookie;

  if (auth_data != NULL)
    g_hash_table_remove (self->priv->auth_sessions, auth_data->session_id);

  headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);

  cookie = g_strdup_printf ("%s=; Expires=Sat, 01 Jan 2000 00:00:00 GMT",
                            self->priv->sid_cookie_name);
  soup_message_headers_append (headers, "Set-Cookie", cookie);
  g_free (cookie);

  evd_web_service_respond (self->priv->web_service,
                           conn,
                           SOUP_STATUS_OK,
                           headers,
                           NULL,
                           0,
                           NULL);

  soup_message_headers_free (headers);
}
