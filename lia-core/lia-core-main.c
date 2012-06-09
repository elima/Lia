#include <stdio.h>
#include <evd.h>

#include <lia.h>

#define SERVICE_NAME "net.free-social.Lia"

static LiaCore *core = NULL;

gint
main (gint argc, gchar *argv[])
{
  gint exit_status = 0;

  g_type_init ();

  core = g_object_new (LIA_TYPE_CORE,
                       "service-name", SERVICE_NAME,
                       NULL);

  /* start the show */
  exit_status = lia_application_run (LIA_APPLICATION (core));

  /* free stuff */
  g_object_unref (core);

  g_print ("\rlia-core terminated\n");

  return exit_status;
}
