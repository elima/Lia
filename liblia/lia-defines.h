/*
 * lia-defines.h
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

#ifndef __LIA_DEFINES_H__
#define __LIA_DEFINES_H__

typedef enum
{
  LIA_BUS_PRIVATE   = 0,
  LIA_BUS_PROTECTED = 1,
  LIA_BUS_PUBLIC    = 2
} LiaBusType;

#define LIA_ENV_KEY_PRIVATE_BUS_ADDR     "LIA_PRIVATE_BUS_ADDRESS"
#define LIA_ENV_KEY_PROTECTED_BUS_ADDR   "LIA_PROTECTED_BUS_ADDRESS"
#define LIA_ENV_KEY_PUBLIC_BUS_ADDR      "LIA_PUBLIC_BUS_ADDRESS"

#define LIA_ENV_KEY_BASE_SERVICE_NAME    "LIA_BASE_SERVICE_NAME"
#define LIA_ENV_KEY_CORE_SERVICE_NAME    "LIA_CORE_SERVICE_NAME"
#define LIA_ENV_KEY_WEBVIEW_SERVICE_NAME "LIA_WEBVIEW_SERVICE_NAME"

#define LIA_CORE_SERVICE_NAME_SUFFIX    "Lia.Core"
#define LIA_WEBVIEW_SERVICE_NAME_SUFFIX "Lia.Webview"

#define LIA_BASE_OBJ_PATH   "/org/eventdance/lia"
#define LIA_BASE_IFACE_NAME "org.eventdance.lia"

#define LIA_AUTH_SERVICE_OBJ_PATH   LIA_BASE_OBJ_PATH "/Core/AuthService"
#define LIA_AUTH_SERVICE_IFACE_NAME LIA_BASE_IFACE_NAME ".Core.AuthService"

#define LIA_WEBVIEW_OBJ_PATH   LIA_BASE_OBJ_PATH "/Webview"
#define LIA_WEBVIEW_IFACE_NAME LIA_BASE_IFACE_NAME ".Webview"

#endif /* __LIA_DEFINES_H__ */
