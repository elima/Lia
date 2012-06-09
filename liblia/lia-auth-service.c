/*
 * lia-auth-service.c
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

#include "lia-auth-service.h"

#include "lia-core.h"

#define LIA_AUTH_SERVICE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                           LIA_TYPE_AUTH_SERVICE, \
                                           LiaAuthServicePrivate))

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='" LIA_AUTH_SERVICE_IFACE_NAME "'>"
  "    <method name='Authenticate'>"
  "      <arg type='s' name='user_name' direction='in'/>"
  "      <arg type='s' name='password' direction='in'/>"
  "      <arg type='s' name='domain' direction='in'/>"

  "      <arg type='s' name='user_id' direction='out'/>"
  "      <arg type='s' name='auth_token' direction='out'/>"
  "      <arg type='n' name='bus_type' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

/* private data */
struct _LiaAuthServicePrivate
{
  GDBusConnection *dbus_conn;

  guint bus_registration_id;

  GDBusInterfaceVTable bus_iface_vtable;
};

/* properties */
enum
{
  PROP_0,
  PROP_DBUS_CONNECTION
};

static void     lia_auth_service_class_init        (LiaAuthServiceClass *class);
static void     lia_auth_service_init              (LiaAuthService *self);

static void     finalize                           (GObject *obj);
static void     dispose                            (GObject *obj);

static void     set_property                       (GObject      *obj,
                                                    guint         prop_id,
                                                    const GValue *value,
                                                    GParamSpec   *pspec);
static void     get_property                       (GObject    *obj,
                                                    guint       prop_id,
                                                    GValue     *value,
                                                    GParamSpec *pspec);

static void     async_initable_iface_init          (GAsyncInitableIface *iface);

static void     init_async                         (GAsyncInitable      *initable,
                                                    int                  io_priority,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data);
static gboolean init_finish                        (GAsyncInitable      *initable,
                                                    GAsyncResult        *res,
                                                    GError             **error);

static void     on_bus_method_call                 (GDBusConnection       *connection,
                                                    const gchar           *sender,
                                                    const gchar           *object_path,
                                                    const gchar           *interface_name,
                                                    const gchar           *method_name,
                                                    GVariant              *parameters,
                                                    GDBusMethodInvocation *invocation,
                                                    gpointer               user_data);

G_DEFINE_TYPE_WITH_CODE (LiaAuthService, lia_auth_service, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                                async_initable_iface_init));

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = init_async;
  iface->init_finish = init_finish;
}

static void
lia_auth_service_class_init (LiaAuthServiceClass *class)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS (class);

  obj_class->dispose = dispose;
  obj_class->finalize = finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  g_object_class_install_property (obj_class,
                                   PROP_DBUS_CONNECTION,
                                   g_param_spec_object ("dbus-connection",
                                                        "D-Bus connection",
                                                        "The D-Bus connection to publish the auth service on",
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (obj_class, sizeof (LiaAuthServicePrivate));
}

static void
lia_auth_service_init (LiaAuthService *self)
{
  LiaAuthServicePrivate *priv;

  priv = LIA_AUTH_SERVICE_GET_PRIVATE (self);
  self->priv = priv;

  priv->dbus_conn = NULL;

  priv->bus_registration_id = 0;

  priv->bus_iface_vtable.method_call = on_bus_method_call;
}

static void
dispose (GObject *obj)
{
  LiaAuthService *self = LIA_AUTH_SERVICE (obj);

  if (self->priv->dbus_conn != NULL)
    {
      if (self->priv->bus_registration_id > 0)
        g_dbus_connection_unregister_object (self->priv->dbus_conn,
                                             self->priv->bus_registration_id);

      g_object_unref (self->priv->dbus_conn);
      self->priv->dbus_conn = NULL;
    }

  G_OBJECT_CLASS (lia_auth_service_parent_class)->dispose (obj);
}

static void
finalize (GObject *obj)
{
  //  LiaAuthService *self = LIA_AUTH_SERVICE (obj);

  G_OBJECT_CLASS (lia_auth_service_parent_class)->finalize (obj);
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  LiaAuthService *self;

  self = LIA_AUTH_SERVICE (obj);

  switch (prop_id)
    {
    case PROP_DBUS_CONNECTION:
      self->priv->dbus_conn = g_value_get_object (value);
      g_assert (G_IS_DBUS_CONNECTION (self->priv->dbus_conn));
      g_object_ref (self->priv->dbus_conn);
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
  LiaAuthService *self;

  self = LIA_AUTH_SERVICE (obj);

  switch (prop_id)
    {
    case PROP_DBUS_CONNECTION:
      g_value_take_object (value, self->priv->dbus_conn);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
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
  GVariant *result;
  //  LiaAuthService *self = LIA_AUTH_SERVICE (user_data);

  g_print ("method %s called\n", method_name);
  result = g_variant_new ("(ssn)",
                          "elima@free-social.net",
                          "14r34625234f4323423432sdff4f32",
                          LIA_BUS_PRIVATE);

  g_dbus_method_invocation_return_value (invocation, result);
}

/*
static void
abort_init_async (LiaAuthService     *self,
                  GSimpleAsyncResult *res,
                  GError             *error)
{
  g_simple_async_result_set_from_error (res, error);
  g_error_free (error);

  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);

  g_object_unref (self);
}
*/

static void
init_async (GAsyncInitable      *initable,
            int                  io_priority,
            GCancellable        *cancellable,
            GAsyncReadyCallback  callback,
            gpointer             user_data)
{
  LiaAuthService *self = LIA_AUTH_SERVICE (initable);
  GSimpleAsyncResult *res;
  GDBusNodeInfo *introspection_data = NULL;
  GError *error = NULL;

  res = g_simple_async_result_new (G_OBJECT (self),
                                   callback,
                                   user_data,
                                   init_async);

  /* register bus object */
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, &error);
  if (introspection_data != NULL)
    {
      self->priv->bus_registration_id =
        g_dbus_connection_register_object (self->priv->dbus_conn,
                                           LIA_AUTH_SERVICE_OBJ_PATH,
                                           introspection_data->interfaces[0],
                                           &self->priv->bus_iface_vtable,
                                           self,
                                           NULL,
                                           &error);
      g_dbus_node_info_unref (introspection_data);
    }

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    {
      /* @TODO: Load peer's RSA key-pair */
    }

  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);
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
