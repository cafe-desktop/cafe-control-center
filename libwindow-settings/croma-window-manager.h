#ifndef MARCO_WINDOW_MANAGER_H
#define MARCO_WINDOW_MANAGER_H

#include <glib-object.h>
#include "cafe-window-manager.h"

#define MARCO_WINDOW_MANAGER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, croma_window_manager_get_type (), MarcoWindowManager)
#define MARCO_WINDOW_MANAGER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, croma_window_manager_get_type (), MarcoWindowManagerClass)
#define IS_MARCO_WINDOW_MANAGER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, croma_window_manager_get_type ())

typedef struct _MarcoWindowManager MarcoWindowManager;
typedef struct _MarcoWindowManagerClass MarcoWindowManagerClass;

typedef struct _MarcoWindowManagerPrivate MarcoWindowManagerPrivate;

struct _MarcoWindowManager
{
	CafeWindowManager parent;
	MarcoWindowManagerPrivate *p;
};

struct _MarcoWindowManagerClass
{
	CafeWindowManagerClass klass;
};

GType      croma_window_manager_get_type             (void);

#endif
