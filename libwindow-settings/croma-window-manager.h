#ifndef MARCO_WINDOW_MANAGER_H
#define MARCO_WINDOW_MANAGER_H

#include <glib-object.h>
#include "cafe-window-manager.h"

#define MARCO_WINDOW_MANAGER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, croma_window_manager_get_type (), CromaWindowManager)
#define MARCO_WINDOW_MANAGER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, croma_window_manager_get_type (), CromaWindowManagerClass)
#define IS_MARCO_WINDOW_MANAGER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, croma_window_manager_get_type ())

typedef struct _CromaWindowManager CromaWindowManager;
typedef struct _CromaWindowManagerClass CromaWindowManagerClass;

typedef struct _CromaWindowManagerPrivate CromaWindowManagerPrivate;

struct _CromaWindowManager
{
	CafeWindowManager parent;
	CromaWindowManagerPrivate *p;
};

struct _CromaWindowManagerClass
{
	CafeWindowManagerClass klass;
};

GType      croma_window_manager_get_type             (void);

#endif
