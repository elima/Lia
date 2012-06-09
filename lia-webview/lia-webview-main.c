#include <signal.h>
#include <evd.h>
#include <lia.h>

#define SERVICE_NAME_SUFFIX "Webview"

static LiaWebview *webview;

/*
static gchar *dbus_addr = NULL;

static GOptionEntry entries[] =
{
  { "dbus-addr", 0, 0, G_OPTION_ARG_STRING, &dbus_addr, "Address of the DBus message bus to use", NULL },
  { NULL }
};
*/

gint
main (gint argc, gchar *argv[])
{
  gint exit_code = 0;
  const gchar *base_service_name;
  gchar *service_name;

  /*
  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("- Free-Social.net Web view daemon");
  g_option_context_add_main_entries (context, entries, NULL);
  if (! g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      return -1;
    }
  g_option_context_free (context);
  */

  g_type_init ();
  evd_tls_init (NULL);

  base_service_name = g_getenv (LIA_ENV_KEY_BASE_SERVICE_NAME);
  service_name = g_strdup_printf ("%s.%s",
                                  base_service_name,
                                  SERVICE_NAME_SUFFIX);

  /* web view */
  webview = g_object_new (LIA_TYPE_WEBVIEW,
                          "service-name", service_name,
                          NULL);

  g_free (service_name);

  /* start the show */
  exit_code = lia_application_run (LIA_APPLICATION (webview));

  /* free stuff */
  g_object_unref (webview);

  g_print ("\rlia-webview terminated\n");

  evd_tls_deinit ();

  return exit_code;
}
